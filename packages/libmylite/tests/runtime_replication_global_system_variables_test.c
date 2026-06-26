#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum {
    sql_capacity = 384,
    scalar_column_count = 22,
    show_variable_column_count = 2,
    show_variable_row_count = 22,
    mysql_error_parse = 1064,
    mysql_error_global_variable_scope = 1229,
    mysql_error_session_variable_only = 1238,
};

struct replication_variable {
    const char *name;
    const char *scalar_value;
    const char *show_value;
    const char *exact_set_value;
    const char *bad_set_value;
    bool read_only;
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

struct expected_warning {
    const char *message;
    const char *context;
};

static int test_replication_values_show_and_scope(void);
static int test_replication_set_and_diagnostics(void);
static int test_replication_deprecation_warnings(void);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_warning(mylite_db *database, struct expected_warning expected);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
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
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

static const struct replication_variable replication_variables[] = {
    {"relay_log", "mylite-relay-bin", "mylite-relay-bin", NULL, NULL, true},
    {"relay_log_basename",
     "/var/lib/mysql/mylite-relay-bin",
     "/var/lib/mysql/mylite-relay-bin",
     NULL,
     NULL,
     true},
    {"relay_log_index",
     "/var/lib/mysql/mylite-relay-bin.index",
     "/var/lib/mysql/mylite-relay-bin.index",
     NULL,
     NULL,
     true},
    {"relay_log_purge", "1", "ON", "ON", "OFF", false},
    {"relay_log_recovery", "0", "OFF", NULL, NULL, true},
    {"relay_log_space_limit", "0", "0", NULL, NULL, true},
    {"replication_optimize_for_static_plugin_config", "0", "OFF", "OFF", "ON", false},
    {"replication_sender_observe_commit_only", "0", "OFF", "OFF", "ON", false},
    {"report_host", NULL, "", NULL, NULL, true},
    {"report_password", NULL, "", NULL, NULL, true},
    {"report_port", "3306", "3306", NULL, NULL, true},
    {"report_user", NULL, "", NULL, NULL, true},
    {"rpl_read_size", "8192", "8192", "8192", "1", false},
    {"rpl_stop_replica_timeout", "31536000", "31536000", "31536000", "1", false},
    {"rpl_stop_slave_timeout", "31536000", "31536000", "31536000", "1", false},
    {"skip_replica_start", "0", "OFF", NULL, NULL, true},
    {"skip_slave_start", "0", "OFF", NULL, NULL, true},
    {"source_verify_checksum", "0", "OFF", "OFF", "ON", false},
    {"sync_master_info", "10000", "10000", "10000", "1", false},
    {"sync_relay_log", "10000", "10000", "10000", "1", false},
    {"sync_relay_log_info", "10000", "10000", "10000", "1", false},
    {"sync_source_info", "10000", "10000", "10000", "1", false},
};

static const char rpl_stop_slave_timeout_warning[] =
    "'@@rpl_stop_slave_timeout' is deprecated and will be removed in a future release. "
    "Please use rpl_stop_replica_timeout instead.";
static const char skip_slave_start_warning[] =
    "'@@skip_slave_start' is deprecated and will be removed in a future release. Please use "
    "skip_replica_start instead.";
static const char sync_master_info_warning[] =
    "'@@sync_master_info' is deprecated and will be removed in a future release. Please use "
    "sync_source_info instead.";
static const char sync_relay_log_info_warning[] =
    "'@@sync_relay_log_info' is deprecated and will be removed in a future release.";

int main(void) {
    int failures = 0;

    failures += test_replication_values_show_and_scope();
    failures += test_replication_set_and_diagnostics();
    failures += test_replication_deprecation_warnings();

    return failures == 0 ? 0 : 1;
}

static int test_replication_values_show_and_scope(void) {
    static const char *const scalar_values[] = {
        "mylite-relay-bin",
        "/var/lib/mysql/mylite-relay-bin",
        "/var/lib/mysql/mylite-relay-bin.index",
        "1",
        "0",
        "0",
        "0",
        "0",
        NULL,
        NULL,
        "3306",
        NULL,
        "8192",
        "31536000",
        "31536000",
        "0",
        "0",
        "0",
        "10000",
        "10000",
        "10000",
        "10000",
    };
    static const char *const show_rows[] = {
        "relay_log",
        "mylite-relay-bin",
        "relay_log_basename",
        "/var/lib/mysql/mylite-relay-bin",
        "relay_log_index",
        "/var/lib/mysql/mylite-relay-bin.index",
        "relay_log_purge",
        "ON",
        "relay_log_recovery",
        "OFF",
        "relay_log_space_limit",
        "0",
        "replication_optimize_for_static_plugin_config",
        "OFF",
        "replication_sender_observe_commit_only",
        "OFF",
        "report_host",
        "",
        "report_password",
        "",
        "report_port",
        "3306",
        "report_user",
        "",
        "rpl_read_size",
        "8192",
        "rpl_stop_replica_timeout",
        "31536000",
        "rpl_stop_slave_timeout",
        "31536000",
        "skip_replica_start",
        "OFF",
        "skip_slave_start",
        "OFF",
        "source_verify_checksum",
        "OFF",
        "sync_master_info",
        "10000",
        "sync_relay_log",
        "10000",
        "sync_relay_log_info",
        "10000",
        "sync_source_info",
        "10000",
    };
    struct expected_sql_error global_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    mylite_db *database = NULL;
    char sql[sql_capacity];
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open replication db");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@relay_log, @@relay_log_basename, @@relay_log_index, "
                   "@@relay_log_purge, @@relay_log_recovery, @@relay_log_space_limit, "
                   "@@replication_optimize_for_static_plugin_config, "
                   "@@replication_sender_observe_commit_only, "
                   "@@report_host, @@report_password, @@report_port, @@report_user, "
                   "@@rpl_read_size, @@rpl_stop_replica_timeout, "
                   "@@rpl_stop_slave_timeout, @@skip_replica_start, @@skip_slave_start, "
                   "@@source_verify_checksum, @@sync_master_info, @@sync_relay_log, "
                   "@@sync_relay_log_info, @@sync_source_info",
            .values = scalar_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .context = "replication scalar values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES WHERE Variable_name IN "
                   "('relay_log','relay_log_basename','relay_log_index','relay_log_purge',"
                   "'relay_log_recovery','relay_log_space_limit',"
                   "'replication_optimize_for_static_plugin_config',"
                   "'replication_sender_observe_commit_only',"
                   "'report_host','report_password','report_port','report_user',"
                   "'rpl_read_size','rpl_stop_replica_timeout','rpl_stop_slave_timeout',"
                   "'skip_replica_start','skip_slave_start','source_verify_checksum',"
                   "'sync_master_info','sync_relay_log','sync_relay_log_info',"
                   "'sync_source_info')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "replication SHOW VARIABLES rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW GLOBAL VARIABLES WHERE Variable_name IN "
                   "('relay_log','relay_log_basename','relay_log_index','relay_log_purge',"
                   "'relay_log_recovery','relay_log_space_limit',"
                   "'replication_optimize_for_static_plugin_config',"
                   "'replication_sender_observe_commit_only',"
                   "'report_host','report_password','report_port','report_user',"
                   "'rpl_read_size','rpl_stop_replica_timeout','rpl_stop_slave_timeout',"
                   "'skip_replica_start','skip_slave_start','source_verify_checksum',"
                   "'sync_master_info','sync_relay_log','sync_relay_log_info',"
                   "'sync_source_info')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "replication SHOW GLOBAL VARIABLES rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW SESSION VARIABLES WHERE Variable_name IN "
                   "('relay_log','relay_log_basename','relay_log_index','relay_log_purge',"
                   "'relay_log_recovery','relay_log_space_limit',"
                   "'replication_optimize_for_static_plugin_config',"
                   "'replication_sender_observe_commit_only',"
                   "'report_host','report_password','report_port','report_user',"
                   "'rpl_read_size','rpl_stop_replica_timeout','rpl_stop_slave_timeout',"
                   "'skip_replica_start','skip_slave_start','source_verify_checksum',"
                   "'sync_master_info','sync_relay_log','sync_relay_log_info',"
                   "'sync_source_info')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "replication SHOW SESSION VARIABLES rows",
        }
    );

    for (size_t index = 0U;
         index < sizeof(replication_variables) / sizeof(replication_variables[0]);
         ++index) {
        snprintf(sql, sizeof(sql), "SELECT @@SESSION.%s", replication_variables[index].name);
        failures += execute_error(database, sql, global_only_read);
        snprintf(sql, sizeof(sql), "SELECT @@LOCAL.%s", replication_variables[index].name);
        failures += execute_error(database, sql, global_only_read);
    }

    mylite_close(database);
    return failures;
}

static int test_replication_set_and_diagnostics(void) {
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
    struct expected_sql_error unsupported_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "fixed no-op",
    };
    struct expected_sql_error unsupported_user_variable_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "replication system variables from user variables",
    };
    mylite_db *database = NULL;
    char sql[sql_capacity];
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open replication SET db");
    for (size_t index = 0U;
         index < sizeof(replication_variables) / sizeof(replication_variables[0]);
         ++index) {
        const struct replication_variable *variable = &replication_variables[index];

        snprintf(sql, sizeof(sql), "SET %s = DEFAULT", variable->name);
        failures += execute_error(database, sql, variable->read_only ? read_only : global_only_set);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = DEFAULT", variable->name);
        if (variable->read_only) {
            failures += execute_error(database, sql, read_only);
            continue;
        }

        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = %s", variable->name, variable->exact_set_value);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SELECT @@GLOBAL.%s", variable->name);
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = sql,
                .values = (const char *[]){variable->scalar_value},
                .column_count = 1U,
                .row_count = 1U,
                .context = "replication global no-op value",
            }
        );

        snprintf(sql, sizeof(sql), "SET GLOBAL %s = %s", variable->name, variable->bad_set_value);
        failures += execute_error(database, sql, unsupported_set);
    }

    failures += execute_statement_ok(database, "SET @replication_global = 10000");
    failures += execute_error(
        database,
        "SET GLOBAL sync_source_info = @replication_global",
        unsupported_user_variable_set
    );
    failures += execute_error(database, "SET GLOBAL relay_log = @replication_global", read_only);

    mylite_close(database);
    return failures;
}

static int test_replication_deprecation_warnings(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open replication warning db");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@GLOBAL.rpl_stop_slave_timeout, @@warning_count",
            .values = (const char *[]){"31536000", "1"},
            .column_count = 2U,
            .row_count = 1U,
            .context = "rpl_stop_slave_timeout scalar warning count",
        }
    );
    failures += expect_warning(
        database,
        (struct expected_warning){
            .message = rpl_stop_slave_timeout_warning,
            .context = "rpl_stop_slave_timeout scalar warning",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@GLOBAL.skip_slave_start, @@warning_count",
            .values = (const char *[]){"0", "1"},
            .column_count = 2U,
            .row_count = 1U,
            .context = "skip_slave_start scalar warning count",
        }
    );
    failures += expect_warning(
        database,
        (struct expected_warning){
            .message = skip_slave_start_warning,
            .context = "skip_slave_start scalar warning",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@GLOBAL.sync_master_info, @@warning_count",
            .values = (const char *[]){"10000", "1"},
            .column_count = 2U,
            .row_count = 1U,
            .context = "sync_master_info scalar warning count",
        }
    );
    failures += expect_warning(
        database,
        (struct expected_warning){
            .message = sync_master_info_warning,
            .context = "sync_master_info scalar warning",
        }
    );
    failures += execute_statement_ok(database, "SET GLOBAL sync_relay_log_info = DEFAULT");
    failures += expect_warning(
        database,
        (struct expected_warning){
            .message = sync_relay_log_info_warning,
            .context = "sync_relay_log_info SET warning",
        }
    );
    failures += execute_statement_ok(database, "SET GLOBAL rpl_stop_slave_timeout = DEFAULT");
    failures += expect_warning(
        database,
        (struct expected_warning){
            .message = rpl_stop_slave_timeout_warning,
            .context = "rpl_stop_slave_timeout SET warning",
        }
    );

    mylite_close(database);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, query.sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            failures += expect_result_value(
                result,
                row,
                column,
                query.values[(row * query.column_count) + column],
                query.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_warning(mylite_db *database, struct expected_warning expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW WARNINGS LIMIT 1", &result);

    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 3U, expected.context);
        failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, 0U),
            "Warning",
            expected.context
        );
        failures +=
            expect_text_or_null(mylite_result_value_text(result, 0U, 1U), "1287", expected.context);
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, 2U),
            expected.message,
            expected.context
        );
    } else {
        failures += 1;
    }

    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "expected OK for [%s], got %d %s\n", sql, rc, mylite_errmsg(database));
        return 1;
    }
    return 0;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    mylite_result_free(result);
    if (rc == MYLITE_OK) {
        fprintf(stderr, "expected error for [%s], got OK\n", sql);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    return expect_text_or_null(mylite_result_value_text(result, row, column), expected, context);
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
        );
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

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
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
