#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum {
    sql_capacity = 384,
    mysql_error_parse = 1064,
    mysql_error_incorrect_argument_type = 1232,
    mysql_error_global_variable_scope = 1229,
    mysql_error_session_variable_only = 1238,
};

struct logging_variable {
    const char *name;
    const char *scalar_value;
    const char *show_value;
    const char *bad_value;
    bool read_only;
    bool fixed_global;
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

static int test_logging_values_show_and_scope(void);
static int test_logging_set_and_diagnostics(void);
static int test_long_query_time_session_state(void);
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

static const char log_slave_updates_warning[] =
    "'@@log_slave_updates' is deprecated and will be removed in a future release. "
    "Please use log_replica_updates instead.";
static const char log_slow_slave_statements_warning[] =
    "'@@log_slow_slave_statements' is deprecated and will be removed in a future release. "
    "Please use log_slow_replica_statements instead.";
static const char log_statements_unsafe_for_binlog_warning[] =
    "'@@log_statements_unsafe_for_binlog' is deprecated and will be removed in a future "
    "release.";

static const struct logging_variable logging_variables[] = {
    {"general_log", "0", "OFF", "ON", false, true},
    {"general_log_file",
     "/var/lib/mysql/mylite.log",
     "/var/lib/mysql/mylite.log",
     "'/tmp/mylite.log'",
     false,
     true},
    {"log_error", "stderr", "stderr", "DEFAULT", true, false},
    {"log_error_services",
     "log_filter_internal; log_sink_internal",
     "log_filter_internal; log_sink_internal",
     "'log_sink_json'",
     false,
     true},
    {"log_error_suppression_list", "", "", "'MY-000000'", false, true},
    {"log_error_verbosity", "2", "2", "3", false, true},
    {"log_output", "FILE", "FILE", "'TABLE'", false, true},
    {"log_queries_not_using_indexes", "0", "OFF", "ON", false, true},
    {"log_raw", "0", "OFF", "ON", false, true},
    {"log_replica_updates", "1", "ON", "DEFAULT", true, false},
    {"log_slave_updates", "1", "ON", "DEFAULT", true, false},
    {"log_slow_admin_statements", "0", "OFF", "ON", false, true},
    {"log_slow_extra", "0", "OFF", "ON", false, true},
    {"log_slow_replica_statements", "0", "OFF", "ON", false, true},
    {"log_slow_slave_statements", "0", "OFF", "ON", false, true},
    {"log_statements_unsafe_for_binlog", "1", "ON", "OFF", false, true},
    {"log_throttle_queries_not_using_indexes", "0", "0", "1", false, true},
    {"log_timestamps", "UTC", "UTC", "SYSTEM", false, true},
    {"slow_query_log", "0", "OFF", "ON", false, true},
    {"slow_query_log_file",
     "/var/lib/mysql/mylite-slow.log",
     "/var/lib/mysql/mylite-slow.log",
     "'/tmp/mylite-slow.log'",
     false,
     true},
};

int main(void) {
    int failures = 0;

    failures += test_logging_values_show_and_scope();
    failures += test_logging_set_and_diagnostics();
    failures += test_long_query_time_session_state();

    return failures == 0 ? 0 : 1;
}

static int test_logging_values_show_and_scope(void) {
    struct expected_sql_error global_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    mylite_db *database = NULL;
    char sql[sql_capacity];
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open logging db");
    for (size_t index = 0U; index < sizeof(logging_variables) / sizeof(logging_variables[0]);
         ++index) {
        const struct logging_variable *variable = &logging_variables[index];

        snprintf(sql, sizeof(sql), "SELECT @@%s, @@GLOBAL.%s", variable->name, variable->name);
        failures += expect_values(
            database,
            sql,
            (const char *[]){variable->scalar_value, variable->scalar_value},
            2U,
            "logging scalar/global"
        );
        snprintf(sql, sizeof(sql), "SHOW VARIABLES LIKE '%s'", variable->name);
        failures += expect_show_value(
            database,
            (struct expected_show_value){.sql = sql,
                                         .name = variable->name,
                                         .value = variable->show_value,
                                         .context = "logging show"}
        );
        snprintf(sql, sizeof(sql), "SHOW GLOBAL VARIABLES LIKE '%s'", variable->name);
        failures += expect_show_value(
            database,
            (struct expected_show_value){.sql = sql,
                                         .name = variable->name,
                                         .value = variable->show_value,
                                         .context = "logging show global"}
        );
        snprintf(sql, sizeof(sql), "SHOW SESSION VARIABLES LIKE '%s'", variable->name);
        failures += expect_show_value(
            database,
            (struct expected_show_value){.sql = sql,
                                         .name = variable->name,
                                         .value = variable->show_value,
                                         .context = "logging show session"}
        );

        snprintf(sql, sizeof(sql), "SELECT @@SESSION.%s", variable->name);
        failures += execute_error(database, sql, global_only_read);
        snprintf(sql, sizeof(sql), "SELECT @@LOCAL.%s", variable->name);
        failures += execute_error(database, sql, global_only_read);
    }

    failures += expect_values(
        database,
        "SELECT @@log_slave_updates, @@warning_count",
        (const char *[]){"1", "1"},
        2U,
        "log_slave_updates warning"
    );
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1287", log_slave_updates_warning},
        3U,
        "log_slave_updates warning row"
    );
    failures += expect_values(
        database,
        "SELECT @@log_slow_slave_statements, @@warning_count",
        (const char *[]){"0", "1"},
        2U,
        "log_slow_slave_statements warning"
    );
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1287", log_slow_slave_statements_warning},
        3U,
        "log_slow_slave_statements warning row"
    );
    failures += expect_values(
        database,
        "SELECT @@log_statements_unsafe_for_binlog, @@warning_count",
        (const char *[]){"1", "1"},
        2U,
        "log_statements_unsafe_for_binlog warning"
    );
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1287", log_statements_unsafe_for_binlog_warning},
        3U,
        "log_statements_unsafe_for_binlog warning row"
    );

    mylite_close(database);
    return failures;
}

static int test_logging_set_and_diagnostics(void) {
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
    struct expected_sql_error unsupported_global_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "SET supports only fixed no-op system variable assignments",
    };
    struct expected_sql_error unsupported_user_variable_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "server logging system variables from user variables are not supported",
    };
    mylite_db *database = NULL;
    char sql[sql_capacity];
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open logging SET db");
    for (size_t index = 0U; index < sizeof(logging_variables) / sizeof(logging_variables[0]);
         ++index) {
        const struct logging_variable *variable = &logging_variables[index];

        snprintf(sql, sizeof(sql), "SET %s = DEFAULT", variable->name);
        failures +=
            execute_error(database, sql, variable->read_only ? read_only_set : global_only_set);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = DEFAULT", variable->name);
        if (variable->read_only) {
            failures += execute_error(database, sql, read_only_set);
        } else {
            failures += execute_statement_ok(database, sql);
            snprintf(sql, sizeof(sql), "SELECT @@GLOBAL.%s", variable->name);
            failures += expect_values(
                database,
                sql,
                (const char *[]){variable->scalar_value},
                1U,
                "logging global default no-op"
            );

            snprintf(sql, sizeof(sql), "SET GLOBAL %s = %s", variable->name, variable->bad_value);
            failures += execute_error(database, sql, unsupported_global_set);
        }
    }

    failures += execute_statement_ok(database, "SET @server_logging = 0");
    failures += execute_error(
        database,
        "SET GLOBAL general_log = @server_logging",
        unsupported_user_variable_set
    );

    mylite_close(database);
    return failures;
}

static int test_long_query_time_session_state(void) {
    struct expected_sql_error global_only_set = {
        .code = mysql_error_global_variable_scope,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable and should be set with SET GLOBAL",
    };
    struct expected_sql_error incorrect_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type to variable 'long_query_time'",
    };
    struct expected_sql_error unsupported_global_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "SET GLOBAL long_query_time assignment is not supported",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open long_query_time db");
    failures += expect_values(
        database,
        "SELECT @@long_query_time, @@SESSION.long_query_time, @@GLOBAL.long_query_time",
        (const char *[]){"10.000000", "10.000000", "10.000000"},
        3U,
        "long_query_time defaults"
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'long_query_time'",
                                     .name = "long_query_time",
                                     .value = "10.000000",
                                     .context = "long_query_time show"}
    );
    failures += execute_statement_ok(database, "SET long_query_time = 1.2345678");
    failures += expect_values(
        database,
        "SELECT @@long_query_time, @@GLOBAL.long_query_time",
        (const char *[]){"1.234568", "10.000000"},
        2U,
        "long_query_time rounded session"
    );
    failures += execute_statement_ok(database, "SET LOCAL long_query_time = 0");
    failures += expect_values(
        database,
        "SELECT @@LOCAL.long_query_time",
        (const char *[]){"0.000000"},
        1U,
        "long_query_time local"
    );
    failures += execute_statement_ok(database, "SET long_query_time = -1");
    failures += expect_values(
        database,
        "SELECT @@long_query_time, @@warning_count",
        (const char *[]){"0.000000", "1"},
        2U,
        "long_query_time negative clamp"
    );
    failures += execute_statement_ok(database, "SET long_query_time = 31536001");
    failures += expect_values(
        database,
        "SELECT @@long_query_time, @@warning_count",
        (const char *[]){"31536000.000000", "1"},
        2U,
        "long_query_time maximum clamp"
    );
    failures += execute_statement_ok(database, "SET @lqt_int = 3");
    failures += execute_statement_ok(database, "SET long_query_time = @lqt_int");
    failures += expect_values(
        database,
        "SELECT @@long_query_time",
        (const char *[]){"3.000000"},
        1U,
        "long_query_time integer user variable"
    );
    failures += execute_statement_ok(database, "SET @lqt_decimal = 3.5");
    failures += execute_statement_ok(database, "SET long_query_time = @lqt_decimal");
    failures += expect_values(
        database,
        "SELECT @@long_query_time",
        (const char *[]){"3.500000"},
        1U,
        "long_query_time decimal user variable"
    );
    failures += execute_error(database, "SET long_query_time = '1.5'", incorrect_type);
    failures += execute_error(database, "SET long_query_time = NULL", incorrect_type);
    failures += execute_error(database, "SET long_query_time = ON", incorrect_type);
    failures += execute_statement_ok(database, "SET GLOBAL long_query_time = DEFAULT");
    failures += execute_error(database, "SET GLOBAL long_query_time = 2", unsupported_global_set);
    failures += execute_statement_ok(database, "SET long_query_time = DEFAULT");
    failures +=
        execute_error(database, "SET long_query_time = 2, general_log = ON", global_only_set);
    failures += expect_values(
        database,
        "SELECT @@long_query_time",
        (const char *[]){"10.000000"},
        1U,
        "long_query_time rollback"
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
