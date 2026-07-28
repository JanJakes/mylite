#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_execution_ast_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    syntax_near_capacity = 80,
    syntax_boundary_max = 81,
    mysql_parse_error = 1064,
    one_mibibyte = 1024 * 1024,
};

struct expected_syntax_error {
    const char *sql;
    size_t sql_length;
    const char *message;
    const char *context;
};

static int test_direct_syntax_diagnostics(void);
static int test_near_text_boundaries_and_large_input(void);
static int test_prepare_syntax_diagnostics(void);
static int test_internal_length_and_embedded_nul_safety(void);
static int expect_execute_syntax_error(mylite_db *database, struct expected_syntax_error expected);
static int expect_prepare_syntax_error(
    mylite_db *database,
    struct expected_syntax_error expected,
    bool buffered
);
static int expect_syntax_diagnostic(
    const mylite_db *database,
    const char *message,
    const char *context
);
static int expect_connection_reuse(mylite_db *database, const char *context);
static int format_expected_message(
    char *message,
    size_t message_capacity,
    const char *near_text,
    size_t near_length,
    unsigned int line
);

int main(void) {
    int failures = 0;

    failures += test_direct_syntax_diagnostics();
    failures += test_near_text_boundaries_and_large_input();
    failures += test_prepare_syntax_diagnostics();
    failures += test_internal_length_and_embedded_nul_safety();

    return failures == 0 ? 0 : 1;
}

static int test_direct_syntax_diagnostics(void) {
    static const struct expected_syntax_error cases[] = {
        {
            .sql = "SELECT FROM t;",
            .sql_length = sizeof("SELECT FROM t;") - 1U,
            .message =
                "You have an error in your SQL syntax; check the manual that corresponds to your "
                "MySQL server version for the right syntax to use near 'FROM t' at line 1",
            .context = "source remainder",
        },
        {
            .sql = "SELECT;",
            .sql_length = sizeof("SELECT;") - 1U,
            .message =
                "You have an error in your SQL syntax; check the manual that corresponds to your "
                "MySQL server version for the right syntax to use near '' at line 1",
            .context = "delimiter end of input",
        },
        {
            .sql = "SELECT",
            .sql_length = sizeof("SELECT") - 1U,
            .message =
                "You have an error in your SQL syntax; check the manual that corresponds to your "
                "MySQL server version for the right syntax to use near '' at line 1",
            .context = "EOF",
        },
        {
            .sql = "SELECT FROM t   ;",
            .sql_length = sizeof("SELECT FROM t   ;") - 1U,
            .message =
                "You have an error in your SQL syntax; check the manual that corresponds to your "
                "MySQL server version for the right syntax to use near 'FROM t' at line 1",
            .context = "trailing whitespace",
        },
        {
            .sql = "SELECT 1 +\nFROM t;",
            .sql_length = sizeof("SELECT 1 +\nFROM t;") - 1U,
            .message =
                "You have an error in your SQL syntax; check the manual that corresponds to your "
                "MySQL server version for the right syntax to use near 'FROM t' at line 2",
            .context = "LF line",
        },
        {
            .sql = "SELECT 1 +\r\nFROM t;",
            .sql_length = sizeof("SELECT 1 +\r\nFROM t;") - 1U,
            .message =
                "You have an error in your SQL syntax; check the manual that corresponds to your "
                "MySQL server version for the right syntax to use near 'FROM t' at line 2",
            .context = "CRLF line",
        },
        {
            .sql = "SELECT 1 +\rFROM t;",
            .sql_length = sizeof("SELECT 1 +\rFROM t;") - 1U,
            .message =
                "You have an error in your SQL syntax; check the manual that corresponds to your "
                "MySQL server version for the right syntax to use near 'FROM t' at line 1",
            .context = "standalone CR line",
        },
        {
            .sql = "SELECT 1;\nSELECT 2;",
            .sql_length = sizeof("SELECT 1;\nSELECT 2;") - 1U,
            .message =
                "You have an error in your SQL syntax; check the manual that corresponds to your "
                "MySQL server version for the right syntax to use near 'SELECT 2' at line 2",
            .context = "multiple statement",
        },
    };
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open direct diagnostic database"
    );

    if (database == NULL) {
        return failures;
    }
    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        failures += expect_execute_syntax_error(database, cases[index]);
    }
    failures += expect_connection_reuse(database, "direct diagnostic reuse");
    mylite_close(database);
    return failures;
}

static int test_near_text_boundaries_and_large_input(void) {
    static const char prefix[] = "SELECT 1 x ";
    static const size_t boundary_lengths[] = {79U, 80U, 81U};
    mylite_db *database = NULL;
    char expected_message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    char boundary_sql[(sizeof(prefix) - 1U) + syntax_boundary_max + 1U];
    char *large_sql = NULL;
    const size_t large_sql_length = (sizeof(prefix) - 1U) + (size_t)one_mibibyte + 1U;
    int failures = mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open boundary diagnostic database"
    );

    if (database == NULL) {
        return failures;
    }
    memcpy(boundary_sql, prefix, sizeof(prefix) - 1U);
    for (size_t index = 0U; index < sizeof(boundary_lengths) / sizeof(boundary_lengths[0]);
         ++index) {
        const size_t token_length = boundary_lengths[index];
        const size_t sql_length = (sizeof(prefix) - 1U) + token_length + 1U;
        const size_t expected_near_length = token_length < (size_t)syntax_near_capacity
                                                ? token_length
                                                : (size_t)syntax_near_capacity;

        memset(boundary_sql + sizeof(prefix) - 1U, 'a', token_length);
        boundary_sql[sql_length - 1U] = ';';
        failures += mylite_test_expect_true(
            format_expected_message(
                expected_message,
                sizeof(expected_message),
                boundary_sql + sizeof(prefix) - 1U,
                expected_near_length,
                1U
            ) == 0,
            "format boundary expected message"
        );
        failures += expect_execute_syntax_error(
            database,
            (struct expected_syntax_error){
                .sql = boundary_sql,
                .sql_length = sql_length,
                .message = expected_message,
                .context = "near-text boundary",
            }
        );
    }

    large_sql = (char *)malloc(large_sql_length);
    failures += mylite_test_expect_true(large_sql != NULL, "allocate one-MiB diagnostic SQL");
    if (large_sql != NULL) {
        memcpy(large_sql, prefix, sizeof(prefix) - 1U);
        memset(large_sql + sizeof(prefix) - 1U, 'a', (size_t)one_mibibyte);
        large_sql[large_sql_length - 1U] = ';';
        failures += mylite_test_expect_true(
            format_expected_message(
                expected_message,
                sizeof(expected_message),
                large_sql + sizeof(prefix) - 1U,
                (size_t)syntax_near_capacity,
                1U
            ) == 0,
            "format one-MiB expected message"
        );
        failures += expect_execute_syntax_error(
            database,
            (struct expected_syntax_error){
                .sql = large_sql,
                .sql_length = large_sql_length,
                .message = expected_message,
                .context = "one-MiB non-NUL-terminated token",
            }
        );
    }
    failures += expect_connection_reuse(database, "large diagnostic reuse");
    free(large_sql);
    mylite_close(database);
    return failures;
}

static int test_prepare_syntax_diagnostics(void) {
    static const struct expected_syntax_error expected = {
        .sql = "SELECT FROM t;",
        .sql_length = sizeof("SELECT FROM t;") - 1U,
        .message =
            "You have an error in your SQL syntax; check the manual that corresponds to your "
            "MySQL server version for the right syntax to use near 'FROM t' at line 1",
        .context = "prepare syntax diagnostic",
    };
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open prepare diagnostic database"
    );

    if (database == NULL) {
        return failures;
    }
    failures += expect_prepare_syntax_error(database, expected, false);
    failures += expect_prepare_syntax_error(database, expected, true);
    failures += expect_connection_reuse(database, "prepare diagnostic reuse");
    mylite_close(database);
    return failures;
}

static int test_internal_length_and_embedded_nul_safety(void) {
    char readable_near[syntax_near_capacity];
    char embedded_nul[] = {'x', '\0', 'y'};
    char expected_message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    struct mylite_sql_parse_result parse_result = {0};
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open internal diagnostic database"
    );

    if (database == NULL) {
        return failures;
    }
    memset(readable_near, 'a', sizeof(readable_near));
    parse_result.error_token = (struct mylite_sql_token){
        .kind = MYLITE_SQL_TOKEN_IDENTIFIER,
        .text = readable_near,
        .length = SIZE_MAX,
        .source_length = SIZE_MAX,
    };
    mylite_execution_set_parse_result_error(database, &parse_result);
    failures += mylite_test_expect_true(
        format_expected_message(
            expected_message,
            sizeof(expected_message),
            readable_near,
            sizeof(readable_near),
            1U
        ) == 0,
        "format SIZE_MAX expected message"
    );
    failures += expect_syntax_diagnostic(database, expected_message, "SIZE_MAX token length");

    parse_result.error_token = (struct mylite_sql_token){
        .kind = MYLITE_SQL_TOKEN_ERROR,
        .text = embedded_nul,
        .length = sizeof(embedded_nul),
        .source_length = sizeof(embedded_nul),
    };
    mylite_execution_set_parse_result_error(database, &parse_result);
    failures += mylite_test_expect_true(
        format_expected_message(expected_message, sizeof(expected_message), embedded_nul, 1U, 1U) ==
            0,
        "format embedded-NUL expected message"
    );
    failures += expect_syntax_diagnostic(database, expected_message, "embedded-NUL token");
    failures += expect_connection_reuse(database, "internal diagnostic reuse");
    mylite_close(database);
    return failures;
}

static int expect_execute_syntax_error(mylite_db *database, struct expected_syntax_error expected) {
    mylite_result *result = NULL;
    const int status = mylite_execute(database, expected.sql, expected.sql_length, &result);
    int failures = mylite_test_expect_int(status, MYLITE_ERROR, expected.context);

    failures += mylite_test_expect_true(result == NULL, "syntax error has no result");
    failures += expect_syntax_diagnostic(database, expected.message, expected.context);
    mylite_result_free(result);
    return failures;
}

static int expect_prepare_syntax_error(
    mylite_db *database,
    struct expected_syntax_error expected,
    bool buffered
) {
    mylite_stmt *statement = NULL;
    const int status =
        buffered ? mylite_prepare_buffered(database, expected.sql, expected.sql_length, &statement)
                 : mylite_prepare(database, expected.sql, expected.sql_length, &statement);
    int failures = mylite_test_expect_int(status, MYLITE_ERROR, expected.context);

    failures += mylite_test_expect_true(statement == NULL, "syntax error has no statement");
    failures += expect_syntax_diagnostic(database, expected.message, expected.context);
    if (statement != NULL) {
        failures += mylite_stmt_finalize(statement);
    }
    return failures;
}

static int expect_syntax_diagnostic(
    const mylite_db *database,
    const char *message,
    const char *context
) {
    int failures = mylite_test_expect_int(mylite_errcode(database), mysql_parse_error, context);

    failures += mylite_test_expect_text(mylite_sqlstate(database), "42000", context);
    failures += mylite_test_expect_text(mylite_errmsg(database), message, context);
    failures += mylite_test_expect_true(
        mylite_errmsg(database) != NULL &&
            strlen(mylite_errmsg(database)) < (size_t)MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY,
        "syntax diagnostic fits public capacity"
    );
    return failures;
}

static int expect_connection_reuse(mylite_db *database, const char *context) {
    static const char sql[] = "SELECT 9";
    mylite_result *result = NULL;
    const int status = mylite_execute(database, sql, sizeof(sql) - 1U, &result);
    int failures = mylite_test_expect_int(status, MYLITE_OK, context);

    failures += mylite_test_expect_true(result != NULL, context);
    mylite_result_free(result);
    return failures;
}

static int format_expected_message(
    char *message,
    size_t message_capacity,
    const char *near_text,
    size_t near_length,
    unsigned int line
) {
    int written = 0;

    if (message == NULL || message_capacity == 0U || near_text == NULL ||
        near_length > (size_t)syntax_near_capacity) {
        return -1;
    }
    written = snprintf(
        message,
        message_capacity,
        "You have an error in your SQL syntax; check the manual that corresponds to your "
        "MySQL server version for the right syntax to use near '%.*s' at line %u",
        (int)near_length,
        near_text,
        line
    );
    return written < 0 || (size_t)written >= message_capacity ? -1 : 0;
}
