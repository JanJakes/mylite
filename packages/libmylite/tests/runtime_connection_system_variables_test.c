#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    default_scalar_value_count = 24,
    session_set_value_count = 9,
    mysql_error_parse = 1064,
    mysql_error_incorrect_argument_type = 1232,
    mysql_error_read_only = 1238,
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

static int test_defaults_show_and_scope(void);
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

    failures += test_defaults_show_and_scope();
    failures += test_set_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_defaults_show_and_scope(void) {
    static const struct expected_show_value expected_show_values[] = {
        {"SHOW VARIABLES LIKE 'back_log'", "back_log", "151", "back_log show"},
        {"SHOW VARIABLES LIKE 'bind_address'", "bind_address", "*", "bind_address show"},
        {"SHOW VARIABLES LIKE 'host_cache_size'", "host_cache_size", "0", "host_cache_size show"},
        {"SHOW VARIABLES LIKE 'connection_memory_chunk_size'",
         "connection_memory_chunk_size",
         "8192",
         "connection_memory_chunk_size show"},
        {"SHOW VARIABLES LIKE 'connection_memory_limit'",
         "connection_memory_limit",
         "18446744073709551615",
         "connection_memory_limit show"},
        {"SHOW VARIABLES LIKE 'global_connection_memory_limit'",
         "global_connection_memory_limit",
         "18446744073709551615",
         "global_connection_memory_limit show"},
        {"SHOW VARIABLES LIKE 'global_connection_memory_tracking'",
         "global_connection_memory_tracking",
         "OFF",
         "global_connection_memory_tracking show"},
        {"SHOW VARIABLES LIKE 'connection_control_failed_connections_threshold'",
         "connection_control_failed_connections_threshold",
         "3",
         "connection_control_failed_connections_threshold show"},
        {"SHOW VARIABLES LIKE 'connection_control_max_connection_delay'",
         "connection_control_max_connection_delay",
         "2147483647",
         "connection_control_max_connection_delay show"},
        {"SHOW VARIABLES LIKE 'connection_control_min_connection_delay'",
         "connection_control_min_connection_delay",
         "1000",
         "connection_control_min_connection_delay show"},
    };
    struct expected_sql_error global_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open connection db");
    failures += expect_values(
        database,
        "SELECT @@back_log, @@GLOBAL.back_log, "
        "@@bind_address, @@GLOBAL.bind_address, "
        "@@host_cache_size, @@GLOBAL.host_cache_size, "
        "@@connection_memory_chunk_size, @@GLOBAL.connection_memory_chunk_size, "
        "@@SESSION.connection_memory_chunk_size, "
        "@@connection_memory_limit, @@GLOBAL.connection_memory_limit, "
        "@@SESSION.connection_memory_limit, "
        "@@global_connection_memory_limit, @@GLOBAL.global_connection_memory_limit, "
        "@@global_connection_memory_tracking, @@GLOBAL.global_connection_memory_tracking, "
        "@@SESSION.global_connection_memory_tracking, "
        "@@connection_control_failed_connections_threshold, "
        "@@GLOBAL.connection_control_failed_connections_threshold, "
        "@@connection_control_max_connection_delay, "
        "@@GLOBAL.connection_control_max_connection_delay, "
        "@@connection_control_min_connection_delay, "
        "@@GLOBAL.connection_control_min_connection_delay, @@warning_count",
        (const char *[]){"151",
                         "151",
                         "*",
                         "*",
                         "0",
                         "0",
                         "8192",
                         "8192",
                         "8192",
                         "18446744073709551615",
                         "18446744073709551615",
                         "18446744073709551615",
                         "18446744073709551615",
                         "18446744073709551615",
                         "0",
                         "0",
                         "0",
                         "3",
                         "3",
                         "2147483647",
                         "2147483647",
                         "1000",
                         "1000",
                         "0"},
        default_scalar_value_count,
        "connection system defaults"
    );

    for (size_t index = 0U; index < sizeof(expected_show_values) / sizeof(expected_show_values[0]);
         ++index) {
        failures += expect_show_value(database, expected_show_values[index]);
    }

    failures += execute_error(database, "SELECT @@SESSION.back_log", global_only_read);
    failures += execute_error(database, "SELECT @@SESSION.bind_address", global_only_read);
    failures += execute_error(database, "SELECT @@SESSION.host_cache_size", global_only_read);
    failures += execute_error(
        database,
        "SELECT @@SESSION.global_connection_memory_limit",
        global_only_read
    );
    failures += execute_error(
        database,
        "SELECT @@SESSION.connection_control_failed_connections_threshold",
        global_only_read
    );

    mylite_close(database);
    return failures;
}

static int test_set_and_diagnostics(void) {
    struct expected_sql_error read_only = {
        .code = mysql_error_read_only,
        .sqlstate = "HY000",
        .message_part = "read only variable",
    };
    struct expected_sql_error global_only_set = {
        .code = mysql_error_global_variable_scope,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable and should be set with SET GLOBAL",
    };
    struct expected_sql_error unsupported_global = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "fixed no-op global assignments",
    };
    struct expected_sql_error incorrect_chunk_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type to variable 'connection_memory_chunk_size'",
    };
    struct expected_sql_error incorrect_limit_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type to variable 'connection_memory_limit'",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open connection SET db");

    failures += execute_error(database, "SET GLOBAL back_log = DEFAULT", read_only);
    failures += execute_error(database, "SET GLOBAL bind_address = DEFAULT", read_only);
    failures += execute_error(database, "SET host_cache_size = DEFAULT", global_only_set);
    failures += execute_statement_ok(database, "SET GLOBAL host_cache_size = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL host_cache_size = 0");
    failures += execute_error(database, "SET GLOBAL host_cache_size = 1", unsupported_global);
    failures += execute_statement_ok(
        database,
        "SET GLOBAL connection_control_failed_connections_threshold = DEFAULT"
    );
    failures += execute_statement_ok(
        database,
        "SET GLOBAL connection_control_max_connection_delay = 2147483647"
    );
    failures +=
        execute_statement_ok(database, "SET GLOBAL connection_control_min_connection_delay = 1000");
    failures += execute_error(
        database,
        "SET connection_control_failed_connections_threshold = DEFAULT",
        global_only_set
    );
    failures += execute_error(
        database,
        "SET GLOBAL connection_control_failed_connections_threshold = 4",
        unsupported_global
    );

    failures += execute_statement_ok(database, "SET SESSION connection_memory_chunk_size = 4096");
    failures += execute_statement_ok(database, "SET @@LOCAL.connection_memory_limit = 2097152");
    failures +=
        execute_statement_ok(database, "SET SESSION global_connection_memory_tracking = ON");
    failures += expect_values(
        database,
        "SELECT @@connection_memory_chunk_size, @@GLOBAL.connection_memory_chunk_size, "
        "@@SESSION.connection_memory_chunk_size, "
        "@@connection_memory_limit, @@GLOBAL.connection_memory_limit, "
        "@@SESSION.connection_memory_limit, "
        "@@global_connection_memory_tracking, @@GLOBAL.global_connection_memory_tracking, "
        "@@SESSION.global_connection_memory_tracking",
        (const char *[]
        ){"4096", "8192", "4096", "2097152", "18446744073709551615", "2097152", "1", "0", "1"},
        session_set_value_count,
        "connection memory session SET values"
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW SESSION VARIABLES LIKE "
                                            "'global_connection_memory_tracking'",
                                     .name = "global_connection_memory_tracking",
                                     .value = "ON",
                                     .context = "global_connection_memory_tracking session show"}
    );

    failures += execute_statement_ok(database, "SET SESSION connection_memory_chunk_size = 0");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]
        ){"Warning", "1292", "Truncated incorrect connection_memory_chunk_size value: '0'"},
        3U,
        "connection_memory_chunk_size lower warning row"
    );
    failures += expect_values(
        database,
        "SELECT @@connection_memory_chunk_size, @@warning_count",
        (const char *[]){"1", "1"},
        2U,
        "connection_memory_chunk_size lower clamp"
    );
    failures +=
        execute_statement_ok(database, "SET SESSION connection_memory_chunk_size = 536870913");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]
        ){"Warning", "1292", "Truncated incorrect connection_memory_chunk_size value: '536870913'"},
        3U,
        "connection_memory_chunk_size upper warning row"
    );
    failures += expect_values(
        database,
        "SELECT @@connection_memory_chunk_size, @@warning_count",
        (const char *[]){"536870912", "1"},
        2U,
        "connection_memory_chunk_size upper clamp"
    );
    failures += execute_statement_ok(database, "SET SESSION connection_memory_limit = 1");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1292", "Truncated incorrect connection_memory_limit value: '1'"
        },
        3U,
        "connection_memory_limit lower warning row"
    );
    failures += expect_values(
        database,
        "SELECT @@connection_memory_limit, @@warning_count",
        (const char *[]){"2097152", "1"},
        2U,
        "connection_memory_limit lower clamp"
    );
    failures += execute_statement_ok(database, "SET SESSION connection_memory_limit = TRUE");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1292", "Truncated incorrect connection_memory_limit value: '1'"
        },
        3U,
        "connection_memory_limit TRUE warning row"
    );
    failures += expect_values(
        database,
        "SELECT @@connection_memory_limit, @@warning_count",
        (const char *[]){"2097152", "1"},
        2U,
        "connection_memory_limit TRUE clamp"
    );

    failures += execute_statement_ok(database, "SET @chunk = 16");
    failures += execute_statement_ok(database, "SET SESSION connection_memory_chunk_size = @chunk");
    failures += execute_statement_ok(database, "SET @limit = 2097153");
    failures += execute_statement_ok(database, "SET SESSION connection_memory_limit = @limit");
    failures += expect_values(
        database,
        "SELECT @@connection_memory_chunk_size, @@connection_memory_limit",
        (const char *[]){"16", "2097153"},
        2U,
        "connection memory user variable assignment"
    );

    failures +=
        execute_statement_ok(database, "SET SESSION connection_memory_chunk_size = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION connection_memory_limit = DEFAULT");
    failures +=
        execute_statement_ok(database, "SET SESSION global_connection_memory_tracking = DEFAULT");
    failures += expect_values(
        database,
        "SELECT @@connection_memory_chunk_size, @@connection_memory_limit, "
        "@@global_connection_memory_tracking",
        (const char *[]){"8192", "18446744073709551615", "0"},
        3U,
        "connection memory default assignments"
    );

    failures += execute_error(
        database,
        "SET SESSION connection_memory_chunk_size = '4096'",
        incorrect_chunk_type
    );
    failures += execute_error(
        database,
        "SET SESSION connection_memory_limit = 18446744073709551616",
        incorrect_limit_type
    );
    failures += execute_error(
        database,
        "SET connection_memory_chunk_size = 1000, connection_memory_limit = 'bad'",
        incorrect_limit_type
    );
    failures += expect_values(
        database,
        "SELECT @@connection_memory_chunk_size",
        (const char *[]){"8192"},
        1U,
        "connection memory failed multi-SET rollback"
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
