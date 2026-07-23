#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    default_scalar_value_count = 24,
    session_assignment_value_count = 13,
    user_variable_assignment_value_count = 4,
    mysql_error_parse = 1064,
    mysql_error_invalid_value = 1231,
    mysql_error_incorrect_argument_type = 1232,
    mysql_error_session_variable_read_only = 1621,
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
static int test_set_session_values_and_interactions(void);
static int test_deprecated_warnings_and_diagnostics(void);
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
    failures += test_set_session_values_and_interactions();
    failures += test_deprecated_warnings_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_values_show_and_scope(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open M session limit db");
    failures += expect_values(
        database,
        "SELECT @@max_delayed_threads, @@GLOBAL.max_delayed_threads, "
        "@@max_execution_time, @@SESSION.max_execution_time, "
        "@@max_heap_table_size, @@LOCAL.max_heap_table_size, "
        "@@max_insert_delayed_threads, @@GLOBAL.max_insert_delayed_threads, "
        "@@max_join_size, @@SESSION.max_join_size, "
        "@@max_length_for_sort_data, @@LOCAL.max_length_for_sort_data, "
        "@@max_points_in_geometry, @@SESSION.max_points_in_geometry, "
        "@@max_seeks_for_key, @@GLOBAL.max_seeks_for_key, "
        "@@max_sort_length, @@SESSION.max_sort_length, "
        "@@max_sp_recursion_depth, @@LOCAL.max_sp_recursion_depth, "
        "@@max_user_connections, @@SESSION.max_user_connections, "
        "@@min_examined_row_limit, @@GLOBAL.min_examined_row_limit",
        (const char *[]){"20",
                         "20",
                         "0",
                         "0",
                         "16777216",
                         "16777216",
                         "20",
                         "20",
                         "18446744073709551615",
                         "18446744073709551615",
                         "4096",
                         "4096",
                         "65536",
                         "65536",
                         "18446744073709551615",
                         "18446744073709551615",
                         "1024",
                         "1024",
                         "0",
                         "0",
                         "0",
                         "0",
                         "0",
                         "0"},
        default_scalar_value_count,
        "M session limit defaults"
    );

    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'max_execution_time'",
                                     .name = "max_execution_time",
                                     .value = "0",
                                     .context = "max_execution_time show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW GLOBAL VARIABLES LIKE 'max_heap_table_size'",
                                     .name = "max_heap_table_size",
                                     .value = "16777216",
                                     .context = "max_heap_table_size show global"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW SESSION VARIABLES LIKE 'max_join_size'",
                                     .name = "max_join_size",
                                     .value = "18446744073709551615",
                                     .context = "max_join_size show session"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'max_user_connections'",
                                     .name = "max_user_connections",
                                     .value = "0",
                                     .context = "max_user_connections show"}
    );

    mylite_close(database);
    return failures;
}

static int test_set_session_values_and_interactions(void) {
    struct expected_sql_error session_read_only = {
        .code = mysql_error_session_variable_read_only,
        .sqlstate = "HY000",
        .message_part =
            "SESSION variable 'max_user_connections' is read-only. Use SET GLOBAL to assign",
    };
    struct expected_sql_error unsupported_global = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "fixed no-op",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open M session SET db");

    failures += execute_statement_ok(database, "SET max_execution_time = 11");
    failures += execute_statement_ok(database, "SET max_heap_table_size = 0");
    failures += execute_statement_ok(database, "SET max_join_size = 42");
    failures += execute_statement_ok(database, "SET max_length_for_sort_data = 0");
    failures += execute_statement_ok(database, "SET max_points_in_geometry = 0");
    failures += execute_statement_ok(database, "SET max_seeks_for_key = 7");
    failures += execute_statement_ok(database, "SET max_sort_length = 18446744073709551615");
    failures += execute_statement_ok(database, "SET max_sp_recursion_depth = 18446744073709551615");
    failures += execute_statement_ok(database, "SET min_examined_row_limit = 9");
    failures += expect_values(
        database,
        "SELECT @@max_execution_time, @@GLOBAL.max_execution_time, "
        "@@max_heap_table_size, @@GLOBAL.max_heap_table_size, "
        "@@max_join_size, @@GLOBAL.max_join_size, @@sql_big_selects, "
        "@@max_length_for_sort_data, @@max_points_in_geometry, @@max_seeks_for_key, "
        "@@max_sort_length, @@max_sp_recursion_depth, @@min_examined_row_limit",
        (const char *[]){"11",
                         "0",
                         "16384",
                         "16777216",
                         "42",
                         "18446744073709551615",
                         "0",
                         "4",
                         "3",
                         "7",
                         "8388608",
                         "255",
                         "9"},
        session_assignment_value_count,
        "M session limit assigned values"
    );

    failures += execute_statement_ok(database, "SET max_join_size = DEFAULT");
    failures += expect_values(
        database,
        "SELECT @@max_join_size, @@sql_big_selects",
        (const char *[]){"18446744073709551615", "1"},
        2U,
        "max_join_size default restores sql_big_selects"
    );

    failures += execute_statement_ok(database, "SET @m_limit = 15");
    failures += execute_statement_ok(database, "SET max_execution_time = @m_limit");
    failures += execute_statement_ok(database, "SET max_heap_table_size = @m_limit");
    failures += execute_statement_ok(database, "SET max_points_in_geometry = @m_limit");
    failures += execute_statement_ok(database, "SET max_sort_length = @m_limit");
    failures += expect_values(
        database,
        "SELECT @@max_execution_time, @@max_heap_table_size, "
        "@@max_points_in_geometry, @@max_sort_length",
        (const char *[]){"15", "16384", "15", "15"},
        user_variable_assignment_value_count,
        "M session limit user-variable assignments"
    );

    failures += execute_statement_ok(database, "SET GLOBAL max_execution_time = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL max_heap_table_size = 16777216");
    failures += execute_statement_ok(database, "SET GLOBAL max_join_size = 18446744073709551615");
    failures += execute_statement_ok(database, "SET GLOBAL max_user_connections = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL max_user_connections = 0");
    failures += execute_error(database, "SET GLOBAL max_user_connections = 1", unsupported_global);
    failures += execute_error(database, "SET max_user_connections = DEFAULT", session_read_only);

    failures += execute_statement_ok(database, "SET max_execution_time = 12");
    failures += execute_error(
        database,
        "SET max_execution_time = 13, max_user_connections = DEFAULT",
        session_read_only
    );
    failures += expect_values(
        database,
        "SELECT @@max_execution_time",
        (const char *[]){"12"},
        1U,
        "M session limit failed multi-SET rollback"
    );

    mylite_close(database);
    return failures;
}

static int test_deprecated_warnings_and_diagnostics(void) {
    struct expected_sql_error invalid_delayed_value = {
        .code = mysql_error_invalid_value,
        .sqlstate = "42000",
        .message_part = "can't be set to the value of '1'",
    };
    struct expected_sql_error incorrect_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open M warning db");

    failures += expect_values(
        database,
        "SELECT @@max_delayed_threads",
        (const char *[]){"20"},
        1U,
        "max_delayed_threads read"
    );
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning",
                         "1287",
                         "'@@max_delayed_threads' is deprecated and will be removed in a "
                         "future release."},
        3U,
        "max_delayed_threads read warning"
    );
    failures += execute_statement_ok(database, "SET max_delayed_threads = 0");
    failures += expect_values(
        database,
        "SELECT @@max_delayed_threads",
        (const char *[]){"0"},
        1U,
        "max_delayed_threads zero"
    );

    failures += expect_values(
        database,
        "SELECT @@max_insert_delayed_threads",
        (const char *[]){"20"},
        1U,
        "max_insert_delayed_threads read"
    );
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning",
                         "1287",
                         "'@@max_insert_delayed_threads' is deprecated and will be removed in a "
                         "future release."},
        3U,
        "max_insert_delayed_threads read warning"
    );

    failures += execute_statement_ok(database, "SET max_length_for_sort_data = 0");
    failures += expect_values(
        database,
        "SELECT @@max_length_for_sort_data",
        (const char *[]){"4"},
        1U,
        "max_length_for_sort_data lower clamp"
    );
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning",
                         "1287",
                         "'@@max_length_for_sort_data' is deprecated and will be removed in a "
                         "future release."},
        3U,
        "max_length_for_sort_data warning"
    );

    failures += execute_error(database, "SET max_delayed_threads = 1", invalid_delayed_value);
    failures +=
        execute_error(database, "SET max_insert_delayed_threads = TRUE", invalid_delayed_value);
    failures += execute_error(database, "SET max_execution_time = 'bogus'", incorrect_type);
    failures += execute_error(database, "SET max_heap_table_size = NULL", incorrect_type);
    failures += execute_statement_ok(database, "SET @bad_limit = 'bogus'");
    failures += execute_error(database, "SET max_sort_length = @bad_limit", incorrect_type);

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
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_execute(database, sql, strlen(sql), &result),
        MYLITE_OK,
        context
    );
    if (result == NULL) {
        fprintf(stderr, "%s: execute failed: %s\n", context, mylite_errmsg(database));
        fprintf(stderr, "%s: missing result\n", context);
        return failures + 1;
    }
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), expected_count, context);
    for (size_t index = 0U; index < expected_count; ++index) {
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, 0U, index),
            expected[index],
            context
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_show_value(mylite_db *database, struct expected_show_value expected) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_execute(database, expected.sql, strlen(expected.sql), &result),
        MYLITE_OK,
        expected.context
    );
    if (result == NULL) {
        fprintf(stderr, "%s: execute failed: %s\n", expected.context, mylite_errmsg(database));
        fprintf(stderr, "%s: missing result\n", expected.context);
        return failures + 1;
    }
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures += mylite_test_expect_size(mylite_result_column_count(result), 2U, expected.context);
    failures += mylite_test_expect_text(
        mylite_result_value_text(result, 0U, 0U),
        expected.name,
        expected.context
    );
    failures += mylite_test_expect_text(
        mylite_result_value_text(result, 0U, 1U),
        expected.value,
        expected.context
    );
    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures =
        mylite_test_expect_int(mylite_execute(database, sql, strlen(sql), &result), MYLITE_OK, sql);

    if (failures != 0) {
        fprintf(stderr, "expected OK for [%s], got %s\n", sql, mylite_errmsg(database));
    }

    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}
