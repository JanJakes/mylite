#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

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
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
    string_row_count = 8,
    like_ab_match_count = 6,
    like_remaining_row_count = 5,
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

static int test_like_predicate_queries(void);
static int test_like_predicate_dml_persistence(void);
static int test_like_sql_mode(void);
static int test_like_predicate_diagnostics(void);
static int test_independent_like_handles(void);
static int populate_strings(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query expected);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int reopen_app_database(mylite_db **out_database, const char *path);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_like_predicate_queries();
    failures += test_like_predicate_dml_persistence();
    failures += test_like_sql_mode();
    failures += test_like_predicate_diagnostics();
    failures += test_independent_like_handles();

    return failures == 0 ? 0 : 1;
}

static int test_like_predicate_queries(void) {
    static const char *const like_ab_ids[] = {"1", "2", "3", "4", "5", "6"};
    static const char *const like_ab_single_ids[] = {"1", "2"};
    static const char *const like_binary_ab_ids[] = {"1", "3", "4", "5", "6"};
    static const char *const like_binary_upper_ab_ids[] = {"2"};
    static const char *const not_like_binary_ab_ids[] = {"2", "8"};
    static const char *const char_exact_ids[] = {"1", "2", "6"};
    static const char *const escaped_underscore_ids[] = {"4"};
    static const char *const escaped_percent_ids[] = {"5"};
    static const char *const concat_escaped_underscore_ids[] = {"4"};
    static const char *const joined_concat_ids[] = {"1", "8"};
    static const char *const visible_having_names[] = {"visible_1", "visible_2"};
    static const char *const not_like_ab_ids[] = {"8"};
    static const char *const aggregate_count[] = {"6"};
    static const char *const logical_ids[] = {"1", "8"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "queries", path, sizeof(path));
    failures += populate_strings(database);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v LIKE 'ab%' ORDER BY id",
            .values = like_ab_ids,
            .column_count = 1U,
            .row_count = like_ab_match_count,
            .context = "varchar LIKE prefix folds ASCII case",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v LIKE 'ab_' ORDER BY id",
            .values = like_ab_single_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "varchar LIKE single-character wildcard",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v LIKE BINARY 'ab%' ORDER BY id",
            .values = like_binary_ab_ids,
            .column_count = 1U,
            .row_count = 5U,
            .context = "LIKE BINARY prefix is case-sensitive",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v LIKE BINARY 'AB%' ORDER BY id",
            .values = like_binary_upper_ab_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "LIKE BINARY uppercase prefix stays distinct",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v LIKE BINARY 'ab\\_%' ORDER BY id",
            .values = escaped_underscore_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "LIKE BINARY default backslash escapes underscore",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v LIKE BINARY NULL ORDER BY id",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "LIKE BINARY NULL pattern matches no WHERE rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v NOT LIKE BINARY 'ab%' ORDER BY id",
            .values = not_like_binary_ab_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "NOT LIKE BINARY excludes NULL and keeps case sensitivity",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE c LIKE 'abc' ORDER BY id",
            .values = char_exact_ids,
            .column_count = 1U,
            .row_count = 3U,
            .context = "char LIKE uses canonical char storage",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE t LIKE 'ab%' ORDER BY id",
            .values = like_ab_ids,
            .column_count = 1U,
            .row_count = like_ab_match_count,
            .context = "text LIKE prefix folds ASCII case",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v LIKE 'ab\\_%' ORDER BY id",
            .values = escaped_underscore_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "default LIKE backslash escapes underscore",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v LIKE CONCAT('ab\\_', '%') ORDER BY id",
            .values = concat_escaped_underscore_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "LIKE accepts CONCAT pattern expression",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE having_names(name VARCHAR(32))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO having_names VALUES "
        "('visible_1'), ('_hidden'), ('visible_2'), ('_internal')",
        NULL
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT name FROM having_names WHERE name IS NOT NULL "
                   "HAVING name NOT LIKE CONCAT('\\_', '%') ORDER BY name LIMIT 2",
            .values = visible_having_names,
            .column_count = 1U,
            .row_count = 2U,
            .context = "SELECT DISTINCT HAVING accepts NOT LIKE CONCAT predicate",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE join_terms(id INT, suffix VARCHAR(8), deadline LONGTEXT)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO join_terms VALUES (1, 'y', '1'), (2, 'missing', '1')",
        NULL
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT jt.id, s.id FROM join_terms AS jt "
                   "JOIN strings AS s ON s.v = CONCAT('x', jt.suffix) "
                   "WHERE jt.deadline < UNIX_TIMESTAMP() ORDER BY jt.id, s.id",
            .values = joined_concat_ids,
            .column_count = 2U,
            .row_count = 1U,
            .context = "joined equality accepts CONCAT descriptor expression and timeout filter",
        }
    );
    {
        mylite_result *age_result = NULL;
        failures += execute_ok(
            database,
            "SELECT UNIX_TIMESTAMP() - jt.deadline AS age FROM join_terms AS jt WHERE jt.id = 1",
            &age_result
        );
        if (age_result != NULL) {
            const char *age_text = NULL;
            char *age_end = NULL;
            long long age = -1LL;
            size_t age_column_count = mylite_result_column_count(age_result);
            size_t age_row_count = mylite_result_row_count(age_result);

            failures += expect_size(
                age_column_count,
                1U,
                "unix_timestamp text arithmetic columns"
            );
            failures += expect_size(
                age_row_count,
                1U,
                "unix_timestamp text arithmetic rows"
            );
            if (age_column_count == 1U && age_row_count == 1U) {
                age_text = mylite_result_value_text(age_result, 0U, 0U);
                age = age_text == NULL ? -1LL : strtoll(age_text, &age_end, 10);
            }
            if (age_text == NULL || age_end == age_text || *age_end != '\0' || age <= 0) {
                fprintf(
                    stderr,
                    "unix_timestamp text arithmetic age: expected positive integer, got %s\n",
                    age_text == NULL ? "(null)" : age_text
                );
                ++failures;
            }
            mylite_result_free(age_result);
        }
    }
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v LIKE 'ab\\%%' ORDER BY id",
            .values = escaped_percent_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "default LIKE backslash escapes percent",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v NOT LIKE 'ab%' ORDER BY id",
            .values = not_like_ab_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "NOT LIKE excludes NULL values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE NOT (v LIKE 'ab%') ORDER BY id",
            .values = not_like_ab_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "prefix NOT over LIKE excludes NULL values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM strings WHERE v LIKE 'ab%'",
            .values = aggregate_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "aggregate source filter LIKE predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE (v LIKE 'ab%' AND id = 1) OR t LIKE 'xy' "
                   "ORDER BY id",
            .values = logical_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "LIKE predicates compose with logical operators",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_like_predicate_dml_persistence(void) {
    static const char *const after_update_rows[] = {
        "1",
        "abc",
        "2",
        "ABC",
        "3",
        "prefix",
        "4",
        "prefix",
        "5",
        "ab%1",
        "6",
        "abc  ",
        "7",
        NULL,
        "8",
        "xy",
    };
    static const char *const after_delete_rows[] = {
        "1",
        "abc",
        "2",
        "ABC",
        "5",
        "ab%1",
        "6",
        "abc  ",
        "7",
        NULL,
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "dml", path, sizeof(path));
    failures += populate_strings(database);
    failures += expect_dml_ok(
        database,
        "UPDATE strings SET v = 'prefix' WHERE v LIKE 'abcd' OR v LIKE 'ab\\_1'",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM strings ORDER BY id",
            .values = after_update_rows,
            .column_count = 2U,
            .row_count = string_row_count,
            .context = "updated rows after LIKE predicate",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM strings WHERE v NOT LIKE 'ab%'", 3);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM strings ORDER BY id",
            .values = after_delete_rows,
            .column_count = 2U,
            .row_count = like_remaining_row_count,
            .context = "remaining rows after NOT LIKE delete",
        }
    );

    mylite_close(database);
    database = NULL;

    mylite_file_preamble_init(expected_preamble);
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "LIKE DML preserves preamble"
    );

    failures += reopen_app_database(&database, path);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM strings ORDER BY id",
            .values = after_delete_rows,
            .column_count = 2U,
            .row_count = like_remaining_row_count,
            .context = "LIKE DML persists after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_like_sql_mode(void) {
    static const char *const default_escape_ids[] = {"4"};
    static const char *const no_backslash_escape_ids[] = {"9"};
    static const char *const no_backslash_escape_binary_ids[] = {"9"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "sql-mode", path, sizeof(path));
    failures += populate_strings(database);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v LIKE 'ab\\_%' ORDER BY id",
            .values = default_escape_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "default sql_mode LIKE escape",
        }
    );
    failures += execute_ok(
        database,
        "INSERT INTO strings VALUES (9, 'ab\\\\_1', 'ab\\\\_1', 'ab\\\\_1')",
        NULL
    );
    failures += execute_ok(database, "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'", NULL);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v LIKE 'ab\\_%' ORDER BY id",
            .values = no_backslash_escape_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "NO_BACKSLASH_ESCAPES treats LIKE backslash as ordinary text",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v LIKE BINARY 'ab\\_%' ORDER BY id",
            .values = no_backslash_escape_binary_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "NO_BACKSLASH_ESCAPES treats LIKE BINARY backslash as ordinary text",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_like_predicate_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += populate_strings(database);
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE id LIKE '1%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE LIKE predicates support only string columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE missing LIKE 'a%'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE v LIKE 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '1'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE v LIKE DATABASE()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'DATABASE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE v LIKE 'a%' ESCAPE '#'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'ESCAPE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE v LIKE '"
        "\xC3"
        "\xA9"
        "%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE LIKE pattern literals support only ASCII text",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE v LIKE 'a\\0%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE LIKE pattern literals do not support NUL bytes",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_like_handles(void) {
    static const char *const left_row[] = {"left"};
    static const char *const right_row[] = {"xy"};
    char left_path[test_path_capacity];
    char right_path[test_path_capacity];
    mylite_db *left = NULL;
    mylite_db *right = NULL;
    int failures = 0;

    failures += open_app_database(&left, "independent-left", left_path, sizeof(left_path));
    failures += open_app_database(&right, "independent-right", right_path, sizeof(right_path));
    failures += populate_strings(left);
    failures += populate_strings(right);
    failures += expect_dml_ok(left, "UPDATE strings SET v = 'left' WHERE v LIKE 'xy'", 1);
    failures += expect_query_values(
        left,
        (struct expected_query){
            .sql = "SELECT v FROM strings WHERE id = 8",
            .values = left_row,
            .column_count = 1U,
            .row_count = 1U,
            .context = "left handle LIKE update",
        }
    );
    failures += expect_query_values(
        right,
        (struct expected_query){
            .sql = "SELECT v FROM strings WHERE id = 8",
            .values = right_row,
            .column_count = 1U,
            .row_count = 1U,
            .context = "right handle remains independent",
        }
    );

    mylite_close(left);
    mylite_close(right);
    remove_related_files(left_path);
    remove_related_files(right_path);
    return failures;
}

static int populate_strings(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(
        database,
        "CREATE TABLE strings (id INT, c CHAR(5), v VARCHAR(8), t TEXT)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO strings VALUES "
        "(1, 'abc', 'abc', 'abc'), "
        "(2, 'ABC', 'ABC', 'ABC'), "
        "(3, 'abcd', 'abcd', 'abcd'), "
        "(4, 'ab_1', 'ab_1', 'ab_1'), "
        "(5, 'ab%1', 'ab%1', 'ab%1'), "
        "(6, 'abc  ', 'abc  ', 'abc  '), "
        "(7, NULL, NULL, NULL), "
        "(8, 'xy', 'xy', 'xy')",
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

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
    failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

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

    if (make_test_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
}

static int reopen_app_database(mylite_db **out_database, const char *path) {
    int failures = expect_int(mylite_open(path, out_database), MYLITE_OK, "reopen app database");

    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-like-predicates-%s-%d.mylite",
        name,
        current_process_id()
    );

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
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

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;
    int failures = 0;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        failures += 1;
    }
    if (failures == 0) {
        bytes_read = fread(buffer, 1U, size, file);
        if (bytes_read != size) {
            fprintf(stderr, "%s: expected %zu bytes, read %zu\n", path, size, bytes_read);
            failures += 1;
        }
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "%s: failed to close file\n", path);
        failures += 1;
    }

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

    return expect_text(actual, expected, context);
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
        );
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
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "(null)" : expected,
                actual == NULL ? "(null)" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected \"%s\" to contain \"%s\"\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle
        );
        return 1;
    }
    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte sequence mismatch\n", context);
        return 1;
    }
    return 0;
}
