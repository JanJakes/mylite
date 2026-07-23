#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

#ifndef P_tmpdir
#  define P_tmpdir "."
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    mysql_error_parse = 1064,
    mysql_error_incorrect_argument_type = 1232,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_result {
    const char *const *columns;
    const char *const *values;
    size_t count;
    const char *context;
};

struct expected_single_value {
    const char *sql;
    const char *value;
    const char *context;
};

struct expected_warning_rows {
    size_t row_count;
    const char *level;
    const char *code;
    const char *message_part;
    const char *context;
};

static int test_values_and_assignment(void);
static int test_warning_cap_and_sql_notes(void);
static int test_independent_handles(void);
static int expect_result(const mylite_result *result, struct expected_result expected);
static int expect_single_value(mylite_db *database, struct expected_single_value expected);
static int expect_show_warning_rows(mylite_db *database, struct expected_warning_rows expected);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_values_and_assignment();
    failures += test_warning_cap_and_sql_notes();
    failures += test_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_values_and_assignment(void) {
    static const char *const default_columns[] = {
        "@@max_error_count",
        "@@session.max_error_count",
        "@@local.max_error_count",
        "@@global.max_error_count",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const default_values[] = {"1024", "1024", "1024", "1024", "0", "-1"};
    static const char *const changed_columns[] = {
        "@@max_error_count",
        "@@session.max_error_count",
        "@@global.max_error_count",
        "@@warning_count",
    };
    static const char *const changed_values[] = {"2", "2", "1024", "0"};
    static const char *const default_again_values[] = {"1024", "1024", "1024", "0"};
    static const char *const show_columns[] = {"Variable_name", "Value"};
    static const char *const show_values[] = {"max_error_count", "2"};
    static const char *const clamp_columns[] = {
        "@@max_error_count",
        "@@warning_count",
    };
    static const char *const negative_values[] = {"0", "1"};
    static const char *const high_values[] = {"65535", "1"};
    static const char *const boolean_columns[] = {
        "@max_error_count_true",
        "@@max_error_count",
    };
    static const char *const boolean_values[] = {"1", "0"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open max error count");
    failures += execute_ok(
        database,
        "SELECT @@max_error_count, @@session.max_error_count, @@local.max_error_count, "
        "@@global.max_error_count, @@warning_count, ROW_COUNT()",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = default_columns,
            .values = default_values,
            .count = sizeof(default_columns) / sizeof(default_columns[0]),
            .context = "default max_error_count values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "SET SESSION max_error_count = 2");
    failures += execute_ok(
        database,
        "SELECT @@max_error_count, @@session.max_error_count, "
        "@@global.max_error_count, @@warning_count",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = changed_columns,
            .values = changed_values,
            .count = sizeof(changed_columns) / sizeof(changed_columns[0]),
            .context = "changed max_error_count values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SHOW SESSION VARIABLES LIKE 'max_error_count'", &result);
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = show_columns,
            .values = show_values,
            .count = sizeof(show_columns) / sizeof(show_columns[0]),
            .context = "SHOW SESSION VARIABLES max_error_count",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "SET max_error_count = DEFAULT");
    failures += execute_ok(
        database,
        "SELECT @@max_error_count, @@session.max_error_count, "
        "@@global.max_error_count, @@warning_count",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = changed_columns,
            .values = default_again_values,
            .count = sizeof(changed_columns) / sizeof(changed_columns[0]),
            .context = "defaulted max_error_count values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "SET max_error_count = -1");
    failures += expect_show_warning_rows(
        database,
        (struct expected_warning_rows){
            .row_count = 1U,
            .level = "Warning",
            .code = "1292",
            .message_part = "Truncated incorrect max_error_count value: '-1'",
            .context = "negative max_error_count warning",
        }
    );
    failures += execute_ok(database, "SELECT @@max_error_count, @@warning_count", &result);
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = clamp_columns,
            .values = negative_values,
            .count = sizeof(clamp_columns) / sizeof(clamp_columns[0]),
            .context = "negative max_error_count clamp",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "SET max_error_count = DEFAULT");
    failures += execute_statement_ok(database, "SET max_error_count = 65536");
    failures += expect_show_warning_rows(
        database,
        (struct expected_warning_rows){
            .row_count = 1U,
            .level = "Warning",
            .code = "1292",
            .message_part = "Truncated incorrect max_error_count value: '65536'",
            .context = "high max_error_count warning",
        }
    );
    failures += execute_ok(database, "SELECT @@max_error_count, @@warning_count", &result);
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = clamp_columns,
            .values = high_values,
            .count = sizeof(clamp_columns) / sizeof(clamp_columns[0]),
            .context = "high max_error_count clamp",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "SET max_error_count = TRUE");
    failures += execute_statement_ok(database, "SET @max_error_count_true = @@max_error_count");
    failures += execute_statement_ok(database, "SET max_error_count = FALSE");
    failures += execute_ok(database, "SELECT @max_error_count_true, @@max_error_count", &result);
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = boolean_columns,
            .values = boolean_values,
            .count = sizeof(boolean_columns) / sizeof(boolean_columns[0]),
            .context = "boolean max_error_count assignment",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SET max_error_count = '7'",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_argument_type,
            .sqlstate = "42000",
            .message_part = "Incorrect argument type to variable 'max_error_count'",
        }
    );
    failures += execute_error(
        database,
        "SET max_error_count = NULL",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_argument_type,
            .sqlstate = "42000",
            .message_part = "Incorrect argument type to variable 'max_error_count'",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_warning_cap_and_sql_notes(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "cap") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open cap diagnostics");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");

    failures += execute_statement_ok(database, "SET max_error_count = 1");
    failures += execute_ok(database, "DROP TABLE IF EXISTS a, b, c", &result);
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        3U,
        "capped drop warning count"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_single_value(
        database,
        (struct expected_single_value){
            .sql = "SHOW COUNT(*) WARNINGS",
            .value = "3",
            .context = "capped count",
        }
    );
    failures += expect_show_warning_rows(
        database,
        (struct expected_warning_rows){
            .row_count = 1U,
            .level = "Note",
            .code = "1051",
            .message_part = "Unknown table",
            .context = "capped retained note rows",
        }
    );
    failures += expect_single_value(
        database,
        (struct expected_single_value){
            .sql = "SHOW COUNT(*) WARNINGS",
            .value = "3",
            .context = "count preserved",
        }
    );

    failures += execute_statement_ok(database, "SET max_error_count = 0");
    failures += execute_ok(database, "DROP TABLE IF EXISTS a, b, c", &result);
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        3U,
        "zero-cap drop warning count"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_single_value(
        database,
        (struct expected_single_value){
            .sql = "SHOW COUNT(*) WARNINGS",
            .value = "3",
            .context = "zero cap count",
        }
    );
    failures += expect_show_warning_rows(
        database,
        (struct expected_warning_rows){
            .row_count = 0U,
            .level = NULL,
            .code = NULL,
            .message_part = NULL,
            .context = "zero cap retained rows",
        }
    );

    failures += execute_error(
        database,
        "BAD SQL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += expect_single_value(
        database,
        (struct expected_single_value){
            .sql = "SHOW COUNT(*) WARNINGS",
            .value = "0",
            .context = "zero cap parse warning count",
        }
    );
    failures += expect_show_warning_rows(
        database,
        (struct expected_warning_rows){
            .row_count = 0U,
            .level = NULL,
            .code = NULL,
            .message_part = NULL,
            .context = "zero cap parse warning rows",
        }
    );
    failures += expect_single_value(
        database,
        (struct expected_single_value){
            .sql = "SHOW COUNT(*) ERRORS",
            .value = "0",
            .context = "zero cap parse error count",
        }
    );

    failures += execute_statement_ok(database, "SET max_error_count = 5");
    failures += execute_statement_ok(database, "SET sql_notes = 0");
    failures += execute_ok(database, "DROP TABLE IF EXISTS suppressed_note", &result);
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        0U,
        "sql_notes suppressed count"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_single_value(
        database,
        (struct expected_single_value){
            .sql = "SHOW COUNT(*) WARNINGS",
            .value = "0",
            .context = "sql_notes suppressed show count",
        }
    );
    failures += execute_statement_ok(database, "SET sql_notes = DEFAULT");
    failures += execute_statement_ok(database, "DROP TABLE IF EXISTS visible_note");
    failures += expect_single_value(
        database,
        (struct expected_single_value){
            .sql = "SHOW COUNT(*) WARNINGS",
            .value = "1",
            .context = "sql_notes restored count",
        }
    );
    failures += expect_show_warning_rows(
        database,
        (struct expected_warning_rows){
            .row_count = 1U,
            .level = "Note",
            .code = "1051",
            .message_part = "Unknown table",
            .context = "sql_notes restored row",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_handles(void) {
    static const char *const columns[] = {
        "@@max_error_count",
        "@@warning_count",
    };
    static const char *const first_values[] = {"7", "0"};
    static const char *const second_values[] = {"1024", "0"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_open_memory(&first),
        MYLITE_OK,
        "open first max error handle"
    );
    failures += mylite_test_expect_int(
        mylite_open_memory(&second),
        MYLITE_OK,
        "open second max error handle"
    );
    failures += execute_statement_ok(first, "SET max_error_count = 7");

    failures += execute_ok(first, "SELECT @@max_error_count, @@warning_count", &result);
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = columns,
            .values = first_values,
            .count = sizeof(columns) / sizeof(columns[0]),
            .context = "first handle max_error_count",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(second, "SELECT @@max_error_count, @@warning_count", &result);
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = columns,
            .values = second_values,
            .count = sizeof(columns) / sizeof(columns[0]),
            .context = "second handle max_error_count",
        }
    );
    mylite_result_free(result);

    mylite_close(second);
    mylite_close(first);
    return failures;
}

static int expect_result(const mylite_result *result, struct expected_result expected) {
    int failures = 0;

    if (result == NULL) {
        fprintf(stderr, "%s: expected result, got NULL\n", expected.context);
        return 1;
    }

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.count,
        expected.context
    );
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t index = 0U; index < expected.count; ++index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, index),
            expected.columns[index],
            expected.context
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0U, index),
            expected.values[index],
            expected.context
        );
    }

    return failures;
}

static int expect_single_value(mylite_db *database, struct expected_single_value expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    failures += mylite_test_expect_size(mylite_result_column_count(result), 1U, expected.context);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, 0U, 0U),
        expected.value,
        expected.context
    );
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);
    mylite_result_free(result);
    return failures;
}

static int expect_show_warning_rows(mylite_db *database, struct expected_warning_rows expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW WARNINGS", &result);

    failures += mylite_test_expect_size(mylite_result_column_count(result), 3U, expected.context);
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);
    if (expected.row_count > 0U) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0U, 0U),
            expected.level,
            expected.context
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0U, 1U),
            expected.code,
            expected.context
        );
        failures += mylite_test_expect_contains(
            mylite_result_value_text(result, 0U, 2U),
            expected.message_part,
            expected.context
        );
    }
    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected success for [%s], got rc=%d err=%d state=%s message=%s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    mylite_result_free(result);
    if (rc == MYLITE_OK) {
        fprintf(stderr, "expected error for [%s], got success\n", sql);
        return 1;
    }

    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return;
    }
    (void)remove(buffer);
}
