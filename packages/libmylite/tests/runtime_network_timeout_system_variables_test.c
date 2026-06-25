#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    default_scalar_value_count = 12,
    session_set_value_count = 12,
    mysql_error_parse = 1064,
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
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

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

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open network db");
    failures += expect_values(
        database,
        "SELECT @@connect_timeout, @@GLOBAL.connect_timeout, "
        "@@net_read_timeout, @@GLOBAL.net_read_timeout, @@SESSION.net_read_timeout, "
        "@@LOCAL.net_read_timeout, "
        "@@net_retry_count, @@GLOBAL.net_retry_count, @@SESSION.net_retry_count, "
        "@@net_write_timeout, @@GLOBAL.net_write_timeout, @@SESSION.net_write_timeout",
        (const char *[]){
            "10",
            "10",
            "30",
            "30",
            "30",
            "30",
            "10",
            "10",
            "10",
            "60",
            "60",
            "60",
        },
        default_scalar_value_count,
        "network timeout defaults"
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'connect_timeout'",
                                     .name = "connect_timeout",
                                     .value = "10",
                                     .context = "connect_timeout show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'net_read_timeout'",
                                     .name = "net_read_timeout",
                                     .value = "30",
                                     .context = "net_read_timeout show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW GLOBAL VARIABLES LIKE 'net_retry_count'",
                                     .name = "net_retry_count",
                                     .value = "10",
                                     .context = "net_retry_count show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW SESSION VARIABLES LIKE 'net_write_timeout'",
                                     .name = "net_write_timeout",
                                     .value = "60",
                                     .context = "net_write_timeout show"}
    );
    failures += execute_error(database, "SELECT @@SESSION.connect_timeout", global_only_read);
    failures += execute_error(database, "SELECT @@LOCAL.connect_timeout", global_only_read);

    mylite_close(database);
    return failures;
}

static int test_set_and_diagnostics(void) {
    struct expected_sql_error global_only_set = {
        .code = mysql_error_global_variable_scope,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable and should be set with SET GLOBAL",
    };
    struct expected_sql_error incorrect_read_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type to variable 'net_read_timeout'",
    };
    struct expected_sql_error incorrect_retry_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type to variable 'net_retry_count'",
    };
    struct expected_sql_error unsupported_global = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "fixed no-op global assignments",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open network SET db");
    failures += execute_statement_ok(database, "SET SESSION net_read_timeout = 5");
    failures += execute_statement_ok(database, "SET LOCAL net_write_timeout = 6");
    failures += execute_statement_ok(database, "SET @@SESSION.net_retry_count = 7");
    failures += expect_values(
        database,
        "SELECT @@net_read_timeout, @@GLOBAL.net_read_timeout, @@SESSION.net_read_timeout, "
        "@@net_write_timeout, @@GLOBAL.net_write_timeout, @@SESSION.net_write_timeout, "
        "@@net_retry_count, @@GLOBAL.net_retry_count, @@SESSION.net_retry_count, "
        "@@warning_count, @@error_count, ROW_COUNT()",
        (const char *[]){"5", "30", "5", "6", "60", "6", "7", "10", "7", "0", "0", "0"},
        session_set_value_count,
        "network timeout session SET values"
    );

    failures += execute_statement_ok(database, "SET SESSION net_read_timeout = DEFAULT");
    failures += execute_statement_ok(database, "SET @@net_write_timeout = DEFAULT");
    failures += execute_statement_ok(database, "SET @@LOCAL.net_retry_count = DEFAULT");
    failures += expect_values(
        database,
        "SELECT @@net_read_timeout, @@net_write_timeout, @@net_retry_count",
        (const char *[]){"30", "60", "10"},
        3U,
        "network timeout default assignments"
    );

    failures += execute_statement_ok(database, "SET SESSION net_read_timeout = 0");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1292", "Truncated incorrect net_read_timeout value: '0'"},
        3U,
        "net_read_timeout warning row"
    );
    failures += expect_values(
        database,
        "SELECT @@net_read_timeout, @@warning_count",
        (const char *[]){"1", "1"},
        2U,
        "net_read_timeout lower clamp"
    );
    failures += execute_statement_ok(database, "SET SESSION net_read_timeout = 31536001");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1292", "Truncated incorrect net_read_timeout value: '31536001'"
        },
        3U,
        "net_read_timeout upper warning row"
    );
    failures += expect_values(
        database,
        "SELECT @@net_read_timeout, @@warning_count",
        (const char *[]){"31536000", "1"},
        2U,
        "net_read_timeout upper clamp"
    );
    failures += execute_statement_ok(database, "SET SESSION net_write_timeout = -1");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1292", "Truncated incorrect net_write_timeout value: '-1'"},
        3U,
        "net_write_timeout warning row"
    );
    failures += execute_statement_ok(database, "SET SESSION net_retry_count = 0");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1292", "Truncated incorrect net_retry_count value: '0'"},
        3U,
        "net_retry_count warning row"
    );

    failures += execute_statement_ok(database, "SET SESSION net_read_timeout = TRUE");
    failures += execute_statement_ok(database, "SET SESSION net_write_timeout = FALSE");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1292", "Truncated incorrect net_write_timeout value: '0'"},
        3U,
        "net_write_timeout false warning"
    );
    failures +=
        execute_statement_ok(database, "SET SESSION net_retry_count = 18446744073709551615");
    failures += expect_values(
        database,
        "SELECT @@net_read_timeout, @@net_write_timeout, @@net_retry_count, @@warning_count",
        (const char *[]){"1", "1", "18446744073709551615", "0"},
        4U,
        "network timeout boolean and max assignment"
    );

    failures += execute_statement_ok(database, "SET @nr = 8");
    failures += execute_statement_ok(database, "SET SESSION net_read_timeout = @nr");
    failures += execute_statement_ok(database, "SET @nw = -2");
    failures += execute_statement_ok(database, "SET SESSION net_write_timeout = @nw");
    failures += execute_statement_ok(database, "SET @nt = 9");
    failures += execute_statement_ok(database, "SET SESSION net_retry_count = @nt");
    failures += expect_values(
        database,
        "SELECT @@net_read_timeout, @@net_write_timeout, @@net_retry_count",
        (const char *[]){"8", "1", "9"},
        3U,
        "network timeout user variables"
    );

    failures += execute_error(database, "SET connect_timeout = DEFAULT", global_only_set);
    failures += execute_error(database, "SET SESSION net_read_timeout = '5'", incorrect_read_type);
    failures += execute_error(database, "SET SESSION net_read_timeout = 1.5", incorrect_read_type);
    failures += execute_error(database, "SET SESSION net_read_timeout = ON", incorrect_read_type);
    failures += execute_error(
        database,
        "SET SESSION net_retry_count = 18446744073709551616",
        incorrect_retry_type
    );

    failures += execute_statement_ok(database, "SET GLOBAL connect_timeout = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL connect_timeout = 10");
    failures += execute_statement_ok(database, "SET GLOBAL net_read_timeout = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL net_read_timeout = 30");
    failures += execute_statement_ok(database, "SET GLOBAL net_retry_count = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL net_retry_count = 10");
    failures += execute_statement_ok(database, "SET GLOBAL net_write_timeout = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL net_write_timeout = 60");
    failures += execute_error(database, "SET GLOBAL connect_timeout = 20", unsupported_global);
    failures += execute_error(database, "SET GLOBAL net_read_timeout = 31", unsupported_global);
    failures += execute_error(database, "SET GLOBAL net_retry_count = 11", unsupported_global);
    failures += execute_error(database, "SET GLOBAL net_write_timeout = 61", unsupported_global);

    failures += execute_statement_ok(database, "SET net_read_timeout = DEFAULT");
    failures += execute_error(
        database,
        "SET net_read_timeout = 11, net_write_timeout = 'bad'",
        (struct expected_sql_error){.code = mysql_error_incorrect_argument_type,
                                    .sqlstate = "42000",
                                    .message_part =
                                        "Incorrect argument type to variable 'net_write_timeout'"}
    );
    failures += expect_values(
        database,
        "SELECT @@net_read_timeout",
        (const char *[]){"30"},
        1U,
        "network timeout rollback"
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
