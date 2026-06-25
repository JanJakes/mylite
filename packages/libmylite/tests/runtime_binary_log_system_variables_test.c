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

struct binary_log_variable {
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

static int test_binary_log_values_show_and_scope(void);
static int test_binary_log_set_and_diagnostics(void);
static int test_binary_log_deprecation_warnings(void);
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

static const char binlog_format_warning[] =
    "'@@binlog_format' is deprecated and will be removed in a future release.";
static const char binlog_max_flush_queue_time_warning[] =
    "'@@binlog_max_flush_queue_time' is deprecated and will be removed in a future release.";

static const struct binary_log_variable binary_log_variables[] = {
    {"binlog_cache_size", "32768", "32768", false},
    {"binlog_checksum", "CRC32", "CRC32", false},
    {"binlog_direct_non_transactional_updates", "0", "OFF", true},
    {"binlog_encryption", "0", "OFF", false},
    {"binlog_error_action", "ABORT_SERVER", "ABORT_SERVER", false},
    {"binlog_expire_logs_auto_purge", "1", "ON", false},
    {"binlog_expire_logs_seconds", "2592000", "2592000", false},
    {"binlog_format", "ROW", "ROW", true},
    {"binlog_group_commit_sync_delay", "0", "0", false},
    {"binlog_group_commit_sync_no_delay_count", "0", "0", false},
    {"binlog_gtid_simple_recovery", "1", "ON", false},
    {"binlog_max_flush_queue_time", "0", "0", false},
    {"binlog_order_commits", "1", "ON", false},
    {"binlog_rotate_encryption_master_key_at_startup", "0", "OFF", false},
    {"binlog_row_event_max_size", "8192", "8192", false},
    {"binlog_row_image", "FULL", "FULL", true},
    {"binlog_row_metadata", "MINIMAL", "MINIMAL", false},
    {"binlog_row_value_options", "", "", true},
    {"binlog_rows_query_log_events", "0", "OFF", true},
    {"binlog_stmt_cache_size", "32768", "32768", false},
    {"binlog_transaction_compression", "0", "OFF", true},
    {"binlog_transaction_compression_level_zstd", "3", "3", true},
    {"binlog_transaction_dependency_history_size", "25000", "25000", false},
};

int main(void) {
    int failures = 0;

    failures += test_binary_log_values_show_and_scope();
    failures += test_binary_log_set_and_diagnostics();
    failures += test_binary_log_deprecation_warnings();

    return failures == 0 ? 0 : 1;
}

static int test_binary_log_values_show_and_scope(void) {
    struct expected_sql_error global_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    mylite_db *database = NULL;
    char sql[sql_capacity];
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open binary-log db");
    for (size_t index = 0U; index < sizeof(binary_log_variables) / sizeof(binary_log_variables[0]);
         ++index) {
        const struct binary_log_variable *variable = &binary_log_variables[index];

        snprintf(sql, sizeof(sql), "SELECT @@%s, @@GLOBAL.%s", variable->name, variable->name);
        failures += expect_values(
            database,
            sql,
            (const char *[]){variable->scalar_value, variable->scalar_value},
            2U,
            "binary-log scalar/global"
        );

        snprintf(sql, sizeof(sql), "SHOW VARIABLES LIKE '%s'", variable->name);
        failures += expect_show_value(
            database,
            (struct expected_show_value){.sql = sql,
                                         .name = variable->name,
                                         .value = variable->show_value,
                                         .context = "binary-log show"}
        );
        snprintf(sql, sizeof(sql), "SHOW GLOBAL VARIABLES LIKE '%s'", variable->name);
        failures += expect_show_value(
            database,
            (struct expected_show_value){.sql = sql,
                                         .name = variable->name,
                                         .value = variable->show_value,
                                         .context = "binary-log show global"}
        );
        snprintf(sql, sizeof(sql), "SHOW SESSION VARIABLES LIKE '%s'", variable->name);
        failures += expect_show_value(
            database,
            (struct expected_show_value){.sql = sql,
                                         .name = variable->name,
                                         .value = variable->show_value,
                                         .context = "binary-log show session"}
        );

        snprintf(sql, sizeof(sql), "SELECT @@SESSION.%s", variable->name);
        if (variable->session_scope) {
            failures += expect_values(
                database,
                sql,
                (const char *[]){variable->scalar_value},
                1U,
                "binary-log session scalar"
            );
        } else {
            failures += execute_error(database, sql, global_only_read);
        }
    }

    mylite_close(database);
    return failures;
}

static int test_binary_log_set_and_diagnostics(void) {
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
        .message_part = "binary log system variables from user variables",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open binary-log SET db");

    failures += execute_error(database, "SET binlog_cache_size = DEFAULT", global_only_set);
    failures += execute_statement_ok(database, "SET GLOBAL binlog_cache_size = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL binlog_cache_size = 32768");
    failures += execute_error(database, "SET GLOBAL binlog_cache_size = 32769", unsupported_set);
    failures += execute_statement_ok(database, "SET GLOBAL binlog_checksum = CRC32");
    failures += execute_statement_ok(database, "SET GLOBAL binlog_encryption = OFF");
    failures += execute_statement_ok(database, "SET GLOBAL binlog_expire_logs_auto_purge = ON");
    failures += execute_statement_ok(database, "SET SESSION binlog_format = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION binlog_format = ROW");
    failures += execute_error(database, "SET SESSION binlog_format = STATEMENT", unsupported_set);
    failures += execute_statement_ok(database, "SET SESSION binlog_row_image = FULL");
    failures += execute_statement_ok(database, "SET SESSION binlog_row_value_options = ''");
    failures += execute_statement_ok(database, "SET SESSION binlog_rows_query_log_events = OFF");
    failures += execute_statement_ok(database, "SET SESSION binlog_transaction_compression = OFF");
    failures +=
        execute_statement_ok(database, "SET SESSION binlog_transaction_compression_level_zstd = 3");

    failures +=
        execute_error(database, "SET GLOBAL binlog_gtid_simple_recovery = DEFAULT", read_only_set);
    failures += execute_error(
        database,
        "SET GLOBAL binlog_rotate_encryption_master_key_at_startup = DEFAULT",
        read_only_set
    );
    failures +=
        execute_error(database, "SET GLOBAL binlog_row_event_max_size = DEFAULT", read_only_set);

    failures += execute_statement_ok(database, "SET @binlog_format_value = 'ROW'");
    failures += execute_error(
        database,
        "SET SESSION binlog_format = @binlog_format_value",
        unsupported_user_variable_set
    );
    failures += expect_values(
        database,
        "SELECT @@binlog_format",
        (const char *[]){"ROW"},
        1U,
        "binary-log failed user variable assignment"
    );

    mylite_close(database);
    return failures;
}

static int test_binary_log_deprecation_warnings(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open binary-log warning db");
    failures += expect_values(
        database,
        "SELECT @@binlog_format, @@warning_count",
        (const char *[]){"ROW", "1"},
        2U,
        "binlog_format scalar warning count"
    );
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1287", binlog_format_warning},
        3U,
        "binlog_format scalar warning row"
    );
    failures += expect_values(
        database,
        "SELECT @@GLOBAL.binlog_max_flush_queue_time, @@warning_count",
        (const char *[]){"0", "1"},
        2U,
        "binlog_max_flush_queue_time scalar warning count"
    );
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1287", binlog_max_flush_queue_time_warning},
        3U,
        "binlog_max_flush_queue_time scalar warning row"
    );
    failures += execute_statement_ok(database, "SET GLOBAL binlog_max_flush_queue_time = DEFAULT");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1287", binlog_max_flush_queue_time_warning},
        3U,
        "binlog_max_flush_queue_time SET warning row"
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
        return 1;
    }
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_size(mylite_result_column_count(result), expected_count, context);
    for (size_t column = 0U; column < expected_count; ++column) {
        failures +=
            expect_text(mylite_result_value_text(result, 0U, column), expected[column], context);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_show_value(mylite_db *database, struct expected_show_value expected) {
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
    failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures += expect_size(mylite_result_column_count(result), 2U, expected.context);
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

    if (rc != MYLITE_OK) {
        fprintf(stderr, "expected OK for [%s], got %d %s\n", sql, rc, mylite_errmsg(database));
        return 1;
    }
    mylite_result_free(result);
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "expected error for [%s], got OK\n", sql);
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
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
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
