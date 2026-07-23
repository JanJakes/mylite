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

enum {
    test_path_capacity = 1024,
    show_warnings_column_count = 3,
    final_row_count = 8,
    mysql_error_parse = 1064,
    mysql_error_bad_null = 1048,
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

struct expected_warnings {
    size_t row_count;
    const char *const *levels;
    const char *const *codes;
    const char *const *message_parts;
    const char *context;
};

struct expected_dml {
    const char *sql;
    int64_t affected_rows;
    size_t warning_count;
};

static const char *const show_warnings_names[show_warnings_column_count] = {
    "Level",
    "Code",
    "Message",
};

static int test_replace_modifiers_for_supported_forms(void);
static int seed_schema(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_dml_ok(mylite_db *database, struct expected_dml expectation);
static int expect_warning_count(mylite_db *database, const char *expected, const char *context);
static int expect_show_warnings(mylite_db *database, struct expected_warnings expectation);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_replace_modifiers_for_supported_forms();

    return failures == 0 ? 0 : 1;
}

static int test_replace_modifiers_for_supported_forms(void) {
    static const char *const delayed_warning_levels[] = {"Warning"};
    static const char *const delayed_warning_codes[] = {"3005"};
    static const char *const delayed_warning_messages[] = {
        "REPLACE DELAYED is no longer supported",
    };
    static const char *const delayed_error_levels[] = {"Warning", "Error"};
    static const char *const delayed_error_codes[] = {"3005", "1048"};
    static const char *const delayed_error_messages[] = {
        "REPLACE DELAYED is no longer supported",
        "Column 'id' cannot be null",
    };
    static const char *const final_rows[] = {
        "1",  "11",  NULL, "2",  "22",  NULL, "3",  "33",  NULL, "4",  "44",  NULL,
        "10", "100", NULL, "11", "110", "11", "12", "120", NULL, "13", "130", NULL,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "modifiers") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open modifier database");
    failures += seed_schema(database);

    failures += expect_dml_ok(
        database,
        (struct expected_dml){
            .sql = "REPLACE LOW_PRIORITY INTO numbers (id, nn) VALUES (1, 11)",
            .affected_rows = 1,
        }
    );
    failures += expect_warning_count(database, "0", "low priority values warning count");
    failures += expect_dml_ok(
        database,
        (struct expected_dml){
            .sql = "REPLACE LOW_PRIORITY INTO numbers SET id = 2, nn = 22",
            .affected_rows = 1,
        }
    );
    failures += expect_dml_ok(
        database,
        (struct expected_dml){
            .sql = "REPLACE LOW_PRIORITY INTO numbers (id, nn, n) "
                   "SELECT id, nn, n FROM src ORDER BY id LIMIT 1",
            .affected_rows = 1,
        }
    );
    failures += expect_dml_ok(
        database,
        (struct expected_dml){
            .sql = "REPLACE LOW_PRIORITY INTO numbers (id, nn) SELECT 12, 120",
            .affected_rows = 1,
        }
    );

    failures += expect_dml_ok(
        database,
        (struct expected_dml){
            .sql = "REPLACE DELAYED INTO numbers (id, nn) VALUES (3, 33)",
            .affected_rows = 1,
            .warning_count = 1U,
        }
    );
    failures += expect_warning_count(database, "1", "delayed values warning count");
    failures += expect_show_warnings(
        database,
        (struct expected_warnings){
            .row_count = 1U,
            .levels = delayed_warning_levels,
            .codes = delayed_warning_codes,
            .message_parts = delayed_warning_messages,
            .context = "delayed values warning row",
        }
    );
    failures += expect_dml_ok(
        database,
        (struct expected_dml){
            .sql = "REPLACE DELAYED INTO numbers SET id = 4, nn = 44",
            .affected_rows = 1,
            .warning_count = 1U,
        }
    );
    failures += expect_show_warnings(
        database,
        (struct expected_warnings){
            .row_count = 1U,
            .levels = delayed_warning_levels,
            .codes = delayed_warning_codes,
            .message_parts = delayed_warning_messages,
            .context = "delayed set warning row",
        }
    );
    failures += expect_dml_ok(
        database,
        (struct expected_dml){
            .sql = "REPLACE DELAYED INTO numbers (id, nn, n) "
                   "SELECT id, nn, n FROM src ORDER BY id DESC LIMIT 1",
            .affected_rows = 1,
            .warning_count = 1U,
        }
    );
    failures += expect_show_warnings(
        database,
        (struct expected_warnings){
            .row_count = 1U,
            .levels = delayed_warning_levels,
            .codes = delayed_warning_codes,
            .message_parts = delayed_warning_messages,
            .context = "delayed select warning row",
        }
    );
    failures += expect_dml_ok(
        database,
        (struct expected_dml){
            .sql = "REPLACE DELAYED INTO numbers (id, nn) SELECT 13, 130 FROM DUAL",
            .affected_rows = 1,
            .warning_count = 1U,
        }
    );
    failures += expect_show_warnings(
        database,
        (struct expected_warnings){
            .row_count = 1U,
            .levels = delayed_warning_levels,
            .codes = delayed_warning_codes,
            .message_parts = delayed_warning_messages,
            .context = "delayed row-scalar select warning row",
        }
    );

    failures += execute_error(
        database,
        "REPLACE DELAYED INTO numbers (id, nn) VALUES (NULL, 1)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'id' cannot be null",
        }
    );
    failures += expect_warning_count(database, "2", "delayed error warning count");
    failures += expect_show_warnings(
        database,
        (struct expected_warnings){
            .row_count = 2U,
            .levels = delayed_error_levels,
            .codes = delayed_error_codes,
            .message_parts = delayed_error_messages,
            .context = "delayed error warning rows",
        }
    );

    failures += execute_error(
        database,
        "REPLACE LOW_PRIORITY DELAYED INTO numbers VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "REPLACE HIGH_PRIORITY INTO numbers VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO LOW_PRIORITY numbers VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn, n FROM numbers ORDER BY id",
            .values = final_rows,
            .column_count = 3U,
            .row_count = final_row_count,
            .context = "replace modifier final rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int seed_schema(mylite_db *database) {
    int failures = 0;

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE numbers (id INT NOT NULL, nn INT NOT NULL, n INT NULL)"
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE src (id INT NOT NULL, nn INT NOT NULL, n INT NULL)"
    );
    failures += execute_statement_ok(
        database,
        "INSERT INTO src (id, nn, n) VALUES (10, 100, NULL), (11, 110, 11)"
    );

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
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

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += mylite_test_expect_true(result == NULL, "failed execute leaves result null");
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    mylite_result_free(result);

    return failures;
}

static int expect_dml_ok(mylite_db *database, struct expected_dml expectation) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expectation.sql, &result);

    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, "DML column count");
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "DML row count");
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        expectation.affected_rows,
        "DML affected rows"
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        expectation.warning_count,
        "DML warning count"
    );
    mylite_result_free(result);

    return failures;
}

static int expect_warning_count(mylite_db *database, const char *expected, const char *context) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW COUNT(*) WARNINGS", &result);

    failures += mylite_test_expect_size(mylite_result_column_count(result), 1U, context);
    failures += mylite_test_expect_text(
        mylite_result_column_name(result, 0U),
        "@@session.warning_count",
        context
    );
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
    failures +=
        mylite_test_expect_text(mylite_result_value_text(result, 0U, 0U), expected, context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, context);
    mylite_result_free(result);

    return failures;
}

static int expect_show_warnings(mylite_db *database, struct expected_warnings expectation) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW WARNINGS", &result);

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        show_warnings_column_count,
        expectation.context
    );
    for (size_t column_index = 0U; column_index < show_warnings_column_count; ++column_index) {
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, column_index),
            show_warnings_names[column_index],
            expectation.context
        );
    }
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expectation.row_count,
        expectation.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), 0U, expectation.context);
    for (size_t row = 0U; row < expectation.row_count; ++row) {
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, row, 0U),
            expectation.levels[row],
            expectation.context
        );
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, row, 1U),
            expectation.codes[row],
            expectation.context
        );
        failures += mylite_test_expect_contains(
            mylite_result_value_text(result, row, 2U),
            expectation.message_parts[row],
            expectation.context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, query.context);
    mylite_result_free(result);

    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        return mylite_test_expect_true(actual == NULL, context);
    }

    return mylite_test_expect_text(actual, expected, context);
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}
