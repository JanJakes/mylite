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
    path_suffix_capacity = 16,
    string_row_count = 5,
};

#define WP_UTF8_TEXT                                                                               \
    "\xC4"                                                                                         \
    "\x85"                                                                                         \
    "\xC5"                                                                                         \
    "\x82"                                                                                         \
    "\xC3"                                                                                         \
    "\xB3"                                                                                         \
    "\xC5"                                                                                         \
    "\x82"                                                                                         \
    "\xC5"                                                                                         \
    "\xBA"                                                                                         \
    "\xC4"                                                                                         \
    "\x87"                                                                                         \
    "\xC4"                                                                                         \
    "\x99"                                                                                         \
    "\xE2"                                                                                         \
    "\x80"                                                                                         \
    "\xA0"

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_string_predicate_queries(void);
static int test_string_predicate_dml(void);
static int test_string_predicate_diagnostics(void);
static int populate_strings(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query expected);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_string_predicate_queries();
    failures += test_string_predicate_dml();
    failures += test_string_predicate_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_string_predicate_queries(void) {
    static const char *const varchar_equal_ids[] = {"1", "2"};
    static const char *const char_equal_ids[] = {"1", "2", "3"};
    static const char *const text_equal_ids[] = {"1", "2"};
    static const char *const varchar_not_equal_ids[] = {"3", "5"};
    static const char *const varchar_null_safe_ids[] = {"1", "2"};
    static const char *const not_null_safe_ids[] = {"3", "4", "5"};
    static const char *const logical_ids[] = {"1", "2", "5"};
    static const char *const aggregate_count[] = {"2"};
    static const char *const utf8_ids[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "queries", path, sizeof(path));
    failures += populate_strings(database);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v = 'abc' ORDER BY id",
            .values = varchar_equal_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "varchar equality folds ASCII case",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE c = 'abc' ORDER BY id",
            .values = char_equal_ids,
            .column_count = 1U,
            .row_count = 3U,
            .context = "char equality uses canonical char storage",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE t = 'abc' ORDER BY id",
            .values = text_equal_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "text equality folds ASCII case",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v <> 'abc' ORDER BY id",
            .values = varchar_not_equal_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "varchar inequality excludes nulls",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v != 'abc' ORDER BY id",
            .values = varchar_not_equal_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "varchar bang inequality excludes nulls",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v <=> 'abc' ORDER BY id",
            .values = varchar_null_safe_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "varchar null-safe equality",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE NOT (v <=> 'abc') ORDER BY id",
            .values = not_null_safe_ids,
            .column_count = 1U,
            .row_count = 3U,
            .context = "not null-safe equality includes nulls",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v = 'abc' OR t = 'ab' ORDER BY id",
            .values = logical_ids,
            .column_count = 1U,
            .row_count = 3U,
            .context = "string predicates compose with logical operators",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM strings WHERE v = 'abc'",
            .values = aggregate_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "aggregate source filter string predicate",
        }
    );
    failures +=
        execute_ok(database, "CREATE TABLE utf8_strings (id INT, v VARCHAR(16), t TEXT)", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO utf8_strings VALUES (1, '" WP_UTF8_TEXT "', '" WP_UTF8_TEXT "')",
        NULL
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM utf8_strings WHERE v = '" WP_UTF8_TEXT "' AND t = '" WP_UTF8_TEXT
                   "'",
            .values = utf8_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "UTF-8 string equality predicate",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_string_predicate_dml(void) {
    static const char *const after_update_rows[] = {
        "1",
        "hit",
        "2",
        "hit",
        "3",
        "abc  ",
        "4",
        NULL,
        "5",
        "ab",
    };
    static const char *const after_delete_rows[] = {
        "3",
        "abc  ",
        "4",
        NULL,
        "5",
        "ab",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "dml", path, sizeof(path));
    failures += populate_strings(database);
    failures += expect_dml_ok(database, "UPDATE strings SET v = 'hit' WHERE v = 'abc'", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM strings ORDER BY id",
            .values = after_update_rows,
            .column_count = 2U,
            .row_count = string_row_count,
            .context = "updated rows after string predicate",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM strings WHERE t = 'ABC'", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, t FROM strings ORDER BY id",
            .values = after_delete_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "remaining rows after string predicate delete",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_string_predicate_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += populate_strings(database);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v = 1",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "numeric string equality predicate no match",
        }
    );
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int populate_strings(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(
        database,
        "CREATE TABLE strings (id INT, c CHAR(5), v VARCHAR(5), t TEXT)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO strings VALUES "
        "(1, 'abc', 'abc', 'abc'), "
        "(2, 'ABC', 'ABC', 'ABC'), "
        "(3, 'abc  ', 'abc  ', 'abc  '), "
        "(4, NULL, NULL, NULL), "
        "(5, 'ab', 'ab', 'ab')",
        NULL
    );
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return 0;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[value_index],
                expected.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
) {
    int failures = 0;

    if (mylite_test_make_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += mylite_test_expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    return mylite_test_expect_text(actual, expected, context);
}
