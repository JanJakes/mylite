#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    default_scalar_value_count = 11,
    session_set_value_count = 8,
    mysql_error_parse = 1064,
    mysql_error_invalid_value = 1231,
    mysql_error_incorrect_argument_type = 1232,
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

static int test_values_show_and_scope(void);
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

int main(void) {
    int failures = 0;

    failures += test_values_show_and_scope();
    failures += test_set_and_diagnostics();

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

    failures += mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open tuning db");
    failures += expect_values(
        database,
        "SELECT @@lock_wait_timeout, @@GLOBAL.lock_wait_timeout, @@SESSION.lock_wait_timeout, "
        "@@low_priority_updates, @@GLOBAL.low_priority_updates, @@SESSION.low_priority_updates, "
        "@@slow_launch_time, @@GLOBAL.slow_launch_time, "
        "@@sort_buffer_size, @@GLOBAL.sort_buffer_size, @@SESSION.sort_buffer_size",
        (const char *[]
        ){"31536000", "31536000", "31536000", "0", "0", "0", "2", "2", "262144", "262144", "262144"
        },
        default_scalar_value_count,
        "session tuning defaults"
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'lock_wait_timeout'",
                                     .name = "lock_wait_timeout",
                                     .value = "31536000",
                                     .context = "lock_wait_timeout show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW SESSION VARIABLES LIKE 'low_priority_updates'",
                                     .name = "low_priority_updates",
                                     .value = "OFF",
                                     .context = "low_priority_updates show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW GLOBAL VARIABLES LIKE 'slow_launch_time'",
                                     .name = "slow_launch_time",
                                     .value = "2",
                                     .context = "slow_launch_time show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'sort_buffer_size'",
                                     .name = "sort_buffer_size",
                                     .value = "262144",
                                     .context = "sort_buffer_size show"}
    );
    failures += execute_error(database, "SELECT @@SESSION.slow_launch_time", global_only_read);
    failures += execute_error(database, "SELECT @@LOCAL.slow_launch_time", global_only_read);

    mylite_close(database);
    return failures;
}

static int test_set_and_diagnostics(void) {
    struct expected_sql_error global_only_set = {
        .code = mysql_error_global_variable_scope,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable and should be set with SET GLOBAL",
    };
    struct expected_sql_error incorrect_lock_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type to variable 'lock_wait_timeout'",
    };
    struct expected_sql_error incorrect_sort_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type to variable 'sort_buffer_size'",
    };
    struct expected_sql_error invalid_low_priority_value = {
        .code = mysql_error_invalid_value,
        .sqlstate = "42000",
        .message_part = "Variable 'low_priority_updates' can't be set to the value of '2'",
    };
    struct expected_sql_error unsupported_timeout_global = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "fixed no-op global assignments",
    };
    struct expected_sql_error unsupported_global = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "assignment is not supported",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open tuning SET db");
    failures += execute_statement_ok(database, "SET lock_wait_timeout = 10");
    failures += execute_statement_ok(database, "SET low_priority_updates = ON");
    failures += execute_statement_ok(database, "SET sort_buffer_size = 65536");
    failures += expect_values(
        database,
        "SELECT @@lock_wait_timeout, @@GLOBAL.lock_wait_timeout, "
        "@@low_priority_updates, @@GLOBAL.low_priority_updates, "
        "@@sort_buffer_size, @@GLOBAL.sort_buffer_size, @@warning_count, @@error_count",
        (const char *[]){"10", "31536000", "1", "0", "65536", "262144", "0", "0"},
        session_set_value_count,
        "session tuning session SET values"
    );

    failures += execute_statement_ok(database, "SET lock_wait_timeout = 0");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1292", "Truncated incorrect lock_wait_timeout value: '0'"},
        3U,
        "lock_wait_timeout warning row"
    );
    failures += expect_values(
        database,
        "SELECT @@lock_wait_timeout, @@warning_count",
        (const char *[]){"1", "1"},
        2U,
        "lock_wait_timeout lower clamp"
    );
    failures += execute_statement_ok(database, "SET sort_buffer_size = TRUE");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1292", "Truncated incorrect sort_buffer_size value: '1'"},
        3U,
        "sort_buffer_size warning row"
    );
    failures += expect_values(
        database,
        "SELECT @@sort_buffer_size, @@warning_count",
        (const char *[]){"32768", "1"},
        2U,
        "sort_buffer_size boolean clamp"
    );

    failures += execute_statement_ok(database, "SET @lock_value = 7");
    failures += execute_statement_ok(database, "SET lock_wait_timeout = @lock_value");
    failures += execute_statement_ok(database, "SET @low_text = 'OFF'");
    failures += execute_statement_ok(database, "SET low_priority_updates = @low_text");
    failures += execute_statement_ok(database, "SET @sort_value = 65536");
    failures += execute_statement_ok(database, "SET sort_buffer_size = @sort_value");
    failures += expect_values(
        database,
        "SELECT @@lock_wait_timeout, @@low_priority_updates, @@sort_buffer_size",
        (const char *[]){"7", "0", "65536"},
        3U,
        "session tuning user variables"
    );

    failures += execute_error(database, "SET lock_wait_timeout = '10'", incorrect_lock_type);
    failures += execute_error(database, "SET lock_wait_timeout = ON", incorrect_lock_type);
    failures += execute_error(database, "SET low_priority_updates = 2", invalid_low_priority_value);
    failures += execute_error(database, "SET sort_buffer_size = '65536'", incorrect_sort_type);
    failures += execute_error(database, "SET sort_buffer_size = ON", incorrect_sort_type);
    failures += execute_error(database, "SET slow_launch_time = DEFAULT", global_only_set);

    failures += execute_statement_ok(database, "SET GLOBAL lock_wait_timeout = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL low_priority_updates = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL slow_launch_time = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL sort_buffer_size = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL slow_launch_time = 2");
    failures +=
        execute_error(database, "SET GLOBAL lock_wait_timeout = 20", unsupported_timeout_global);
    failures += execute_error(database, "SET GLOBAL low_priority_updates = ON", unsupported_global);
    failures += execute_error(database, "SET GLOBAL slow_launch_time = 3", unsupported_global);
    failures += execute_error(database, "SET GLOBAL sort_buffer_size = 65536", unsupported_global);

    failures += execute_statement_ok(database, "SET lock_wait_timeout = DEFAULT");
    failures += execute_error(
        database,
        "SET lock_wait_timeout = 11, sort_buffer_size = 'bad'",
        incorrect_sort_type
    );
    failures += expect_values(
        database,
        "SELECT @@lock_wait_timeout",
        (const char *[]){"31536000"},
        1U,
        "session tuning rollback"
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

    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}
