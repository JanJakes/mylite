#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_duplicate_entry = 1062,
    mysql_warning_data_out_of_range = 1264,
    mysql_warning_data_truncated = 1265,
    mysql_warning_division_by_zero = 1365,
    warning_out_of_range_positive = 999,
    warning_out_of_range_negative = -999,
};

static int test_direct_warning_snapshot(void);
static int test_statement_error_isolation(void);
static int test_prepared_warning_snapshot(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_setup(mylite_db *database, const char *sql);
static int expect_diagnostic(
    const struct mylite_diagnostic *diagnostic,
    int code,
    const char *sqlstate,
    const char *message,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_direct_warning_snapshot();
    failures += test_statement_error_isolation();
    failures += test_prepared_warning_snapshot();

    return failures == 0 ? 0 : 1;
}

static int test_direct_warning_snapshot(void) {
    struct mylite_diagnostic first = {0};
    struct mylite_diagnostic invalid = {
        .error_code = -1,
        .sqlstate = "XXXXX",
        .message = "stale",
    };
    mylite_db *database = NULL;
    mylite_result *capped_result = NULL;
    mylite_result *counted_result = NULL;
    mylite_result *warning_result = NULL;
    mylite_result *success_result = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open direct warning database"
    );
    failures += execute_setup(database, "CREATE DATABASE app");
    failures += execute_setup(database, "USE app");

    failures += execute_ok(database, "DO 5 DIV 0, 6 DIV 0", &warning_result);
    failures += mylite_test_expect_size(
        mylite_result_warning_count(warning_result),
        2U,
        "direct total warning count"
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_record_count(warning_result),
        2U,
        "direct retained warning count"
    );
    failures += mylite_test_expect_int(
        mylite_result_warning_at(warning_result, 0U, &first),
        MYLITE_OK,
        "copy first direct warning"
    );
    failures += expect_diagnostic(
        &first,
        mysql_warning_division_by_zero,
        "22012",
        "Division by 0",
        "first direct warning"
    );
    failures += mylite_test_expect_int(
        mylite_result_warning_at(warning_result, 2U, &invalid),
        MYLITE_MISUSE,
        "reject direct warning index"
    );
    failures += mylite_test_expect_int(invalid.error_code, 0, "zero invalid warning code");
    failures += mylite_test_expect_text(invalid.sqlstate, "", "zero invalid warning SQLSTATE");
    failures += mylite_test_expect_text(invalid.message, "", "zero invalid warning message");
    failures += mylite_test_expect_int(
        mylite_result_warning_at(warning_result, 0U, NULL),
        MYLITE_MISUSE,
        "reject null direct warning output"
    );

    failures += execute_setup(database, "SET SESSION max_error_count = 1");
    failures += execute_ok(database, "DO 5 DIV 0, 6 DIV 0, 7 DIV 0", &capped_result);
    failures += mylite_test_expect_size(
        mylite_result_warning_count(capped_result),
        3U,
        "capped total warning count"
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_record_count(capped_result),
        1U,
        "capped retained warning count"
    );
    mylite_result_free(capped_result);

    failures += execute_ok(database, "DROP DATABASE IF EXISTS absent", &counted_result);
    failures += mylite_test_expect_size(
        mylite_result_warning_count(counted_result),
        1U,
        "counted-only total warning count"
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_record_count(counted_result),
        0U,
        "counted-only retained warning count"
    );
    mylite_result_free(counted_result);

    failures += execute_ok(database, "DO 0", &success_result);
    mylite_result_free(success_result);
    mylite_close(database);

    failures += mylite_test_expect_size(
        mylite_result_warning_count(warning_result),
        2U,
        "direct warning total after close"
    );
    failures += mylite_test_expect_int(
        mylite_result_warning_at(warning_result, 1U, &invalid),
        MYLITE_OK,
        "copy direct warning after close"
    );
    failures += expect_diagnostic(
        &invalid,
        mysql_warning_division_by_zero,
        "22012",
        "Division by 0",
        "direct warning after close"
    );
    failures += expect_diagnostic(
        &first,
        mysql_warning_division_by_zero,
        "22012",
        "Division by 0",
        "caller copy after close"
    );
    mylite_result_free(warning_result);

    failures += mylite_test_expect_int(
        mylite_result_warning_at(NULL, 0U, &invalid),
        MYLITE_MISUSE,
        "reject null warning result"
    );
    failures += mylite_test_expect_int(invalid.error_code, 0, "zero null-result warning code");
    return failures;
}

static int test_statement_error_isolation(void) {
    mylite_db *database = NULL;
    mylite_stmt *first = NULL;
    mylite_stmt *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open statement error database"
    );
    failures += execute_setup(database, "CREATE DATABASE app");
    failures += execute_setup(database, "USE app");
    failures += execute_setup(database, "CREATE TABLE first_rows(id INT PRIMARY KEY)");
    failures += execute_setup(database, "CREATE TABLE second_rows(id INT PRIMARY KEY)");
    failures += execute_setup(database, "INSERT INTO first_rows VALUES (1)");
    failures += execute_setup(database, "INSERT INTO second_rows VALUES (1)");

    failures += mylite_test_expect_int(
        mylite_prepare_buffered(
            database,
            "INSERT INTO first_rows VALUES (?)",
            strlen("INSERT INTO first_rows VALUES (?)"),
            &first
        ),
        MYLITE_OK,
        "prepare first failing statement"
    );
    failures += mylite_test_expect_int(
        mylite_prepare_buffered(
            database,
            "INSERT INTO second_rows VALUES (?)",
            strlen("INSERT INTO second_rows VALUES (?)"),
            &second
        ),
        MYLITE_OK,
        "prepare second failing statement"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_bind_int64(first, 0U, 1), MYLITE_OK, "bind first");
    failures +=
        mylite_test_expect_int(mylite_stmt_bind_int64(second, 0U, 1), MYLITE_OK, "bind second");
    failures +=
        mylite_test_expect_int(mylite_stmt_step(first), MYLITE_ERROR, "execute first duplicate");
    failures += mylite_test_expect_int(
        mylite_stmt_errcode(first),
        mysql_error_duplicate_entry,
        "first duplicate code"
    );
    failures += mylite_test_expect_text(mylite_stmt_sqlstate(first), "23000", "first SQLSTATE");
    failures += mylite_test_expect_contains(
        mylite_stmt_errmsg(first),
        "first_rows.PRIMARY",
        "first duplicate message"
    );

    failures +=
        mylite_test_expect_int(mylite_stmt_step(second), MYLITE_ERROR, "execute second duplicate");
    failures += mylite_test_expect_int(
        mylite_stmt_errcode(second),
        mysql_error_duplicate_entry,
        "second duplicate code"
    );
    failures += mylite_test_expect_contains(
        mylite_stmt_errmsg(second),
        "second_rows.PRIMARY",
        "second duplicate message"
    );
    failures += mylite_test_expect_contains(
        mylite_stmt_errmsg(first),
        "first_rows.PRIMARY",
        "first duplicate remains isolated"
    );

    failures += execute_ok(database, "DO 0", &result);
    mylite_result_free(result);
    failures += mylite_test_expect_contains(
        mylite_stmt_errmsg(first),
        "first_rows.PRIMARY",
        "first duplicate after connection success"
    );
    failures += mylite_test_expect_contains(
        mylite_stmt_errmsg(second),
        "second_rows.PRIMARY",
        "second duplicate after connection success"
    );

    failures +=
        mylite_test_expect_int(mylite_stmt_reset(first), MYLITE_OK, "reset first statement");
    failures += mylite_test_expect_int(mylite_stmt_errcode(first), 0, "reset first code");
    failures +=
        mylite_test_expect_text(mylite_stmt_sqlstate(first), "00000", "reset first SQLSTATE");
    failures += mylite_test_expect_text(mylite_stmt_errmsg(first), "", "reset first message");
    failures += mylite_test_expect_contains(
        mylite_stmt_errmsg(second),
        "second_rows.PRIMARY",
        "second duplicate after first reset"
    );

    failures += mylite_test_expect_int(mylite_stmt_finalize(first), MYLITE_OK, "finalize first");
    failures += mylite_test_expect_int(mylite_stmt_finalize(second), MYLITE_OK, "finalize second");
    mylite_close(database);
    return failures;
}

static int test_prepared_warning_snapshot(void) {
    static const int expected_codes[] = {
        mysql_warning_data_out_of_range,
        mysql_warning_data_truncated,
        mysql_warning_data_out_of_range,
        mysql_warning_data_truncated,
    };
    static const char *const expected_messages[] = {
        "Out of range value for column 'tiny_value' at row 1",
        "Data truncated for column 'short_text' at row 1",
        "Out of range value for column 'tiny_value' at row 2",
        "Data truncated for column 'short_text' at row 2",
    };
    struct mylite_diagnostic copied = {0};
    mylite_db *database = NULL;
    mylite_stmt *statement = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open prepared warning database"
    );
    failures += execute_setup(database, "CREATE DATABASE app");
    failures += execute_setup(database, "USE app");
    failures += execute_setup(database, "SET SESSION sql_mode = ''");
    failures += execute_setup(
        database,
        "CREATE TABLE warning_rows(tiny_value TINYINT, short_text VARCHAR(2))"
    );
    failures += mylite_test_expect_int(
        mylite_prepare_buffered(
            database,
            "INSERT INTO warning_rows(tiny_value, short_text) VALUES (?, ?), (?, ?)",
            strlen("INSERT INTO warning_rows(tiny_value, short_text) VALUES (?, ?), (?, ?)"),
            &statement
        ),
        MYLITE_OK,
        "prepare warning statement"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(statement, 0U, warning_out_of_range_positive),
        MYLITE_OK,
        "bind warning tiny row one"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(statement, 1U, "long", strlen("long")),
        MYLITE_OK,
        "bind warning text row one"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(statement, 2U, warning_out_of_range_negative),
        MYLITE_OK,
        "bind warning tiny row two"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(statement, 3U, "wide", strlen("wide")),
        MYLITE_OK,
        "bind warning text row two"
    );

    failures +=
        mylite_test_expect_int(mylite_stmt_step(statement), MYLITE_DONE, "execute warnings");
    failures += mylite_test_expect_int(mylite_stmt_errcode(statement), 0, "warning statement code");
    failures +=
        mylite_test_expect_text(mylite_stmt_sqlstate(statement), "00000", "warning SQLSTATE");
    failures += mylite_test_expect_size(
        mylite_stmt_warning_count(statement),
        4U,
        "prepared total warnings"
    );
    failures += mylite_test_expect_size(
        mylite_stmt_warning_record_count(statement),
        4U,
        "prepared retained warnings"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_warning_at(statement, 0U, NULL),
        MYLITE_MISUSE,
        "reject null prepared warning output"
    );
    for (size_t index = 0U; index < 4U; ++index) {
        failures += mylite_test_expect_int(
            mylite_stmt_warning_at(statement, index, &copied),
            MYLITE_OK,
            "copy prepared warning"
        );
        failures += expect_diagnostic(
            &copied,
            expected_codes[index],
            index % 2U == 0U ? "22003" : "01000",
            expected_messages[index],
            "prepared warning record"
        );
    }

    failures += execute_ok(database, "DO 0", &result);
    mylite_result_free(result);
    failures += expect_diagnostic(
        &copied,
        mysql_warning_data_truncated,
        "01000",
        expected_messages[3],
        "prepared warning caller copy"
    );
    failures += mylite_test_expect_size(
        mylite_stmt_warning_count(statement),
        4U,
        "prepared warnings after connection call"
    );

    failures +=
        mylite_test_expect_int(mylite_stmt_reset(statement), MYLITE_OK, "reset warning statement");
    failures +=
        mylite_test_expect_size(mylite_stmt_warning_count(statement), 0U, "reset warning total");
    failures += mylite_test_expect_size(
        mylite_stmt_warning_record_count(statement),
        0U,
        "reset warning records"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(statement), MYLITE_DONE, "reexecute warnings");
    failures += mylite_test_expect_size(
        mylite_stmt_warning_count(statement),
        4U,
        "reexecuted warning total"
    );

    mylite_close(database);
    failures += expect_diagnostic(
        &copied,
        mysql_warning_data_truncated,
        "01000",
        expected_messages[3],
        "prepared warning caller copy after close"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(statement),
        MYLITE_OK,
        "finalize detached warning"
    );
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: execute failed: %d/%s/%s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    return 0;
}

static int execute_setup(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int expect_diagnostic(
    const struct mylite_diagnostic *diagnostic,
    int code,
    const char *sqlstate,
    const char *message,
    const char *context
) {
    int failures = 0;

    failures +=
        mylite_test_expect_int(diagnostic == NULL ? 0 : diagnostic->error_code, code, context);
    failures += mylite_test_expect_text(
        diagnostic == NULL ? NULL : diagnostic->sqlstate,
        sqlstate,
        context
    );
    failures +=
        mylite_test_expect_text(diagnostic == NULL ? NULL : diagnostic->message, message, context);
    return failures;
}
