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
    mysql_error_regular_expression = 3696,
    mysql_error_regular_expression_character_range = 3697,
    string_row_count = 9,
    regexp_remaining_row_count = 7,
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

static int test_regexp_predicate_queries(void);
static int test_regexp_predicate_dml_persistence(void);
static int test_regexp_pattern_operators(void);
static int test_regexp_predicate_diagnostics(void);
static int test_independent_regexp_handles(void);
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

    failures += test_regexp_predicate_queries();
    failures += test_regexp_predicate_dml_persistence();
    failures += test_regexp_pattern_operators();
    failures += test_regexp_predicate_diagnostics();
    failures += test_independent_regexp_handles();

    return failures == 0 ? 0 : 1;
}

static int test_regexp_predicate_queries(void) {
    static const char *const prefix_ids[] = {"1", "2", "3", "9"};
    static const char *const rss_ids[] = {"4", "6"};
    static const char *const char_exact_ids[] = {"1", "2", "9"};
    static const char *const class_ids[] = {"1", "2", "3"};
    static const char *const negated_class_ids[] = {"4"};
    static const char *const not_prefix_ids[] = {"4", "5", "6", "8"};
    static const char *const aggregate_count[] = {"4"};
    static const char *const integer_regexp_ids[] = {"1"};
    static const char *const integer_not_regexp_ids[] = {"2", "3"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "queries", path, sizeof(path));
    failures += populate_strings(database);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v REGEXP '^ab' ORDER BY id",
            .values = prefix_ids,
            .column_count = 1U,
            .row_count = 4U,
            .context = "REGEXP prefix folds ASCII case",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v RLIKE '^AB' ORDER BY id",
            .values = prefix_ids,
            .column_count = 1U,
            .row_count = 4U,
            .context = "RLIKE synonym folds ASCII case",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v REGEXP '^rss_.+$' ORDER BY id",
            .values = rss_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "WordPress rss REGEXP shape",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE c REGEXP '^abc$' ORDER BY id",
            .values = char_exact_ids,
            .column_count = 1U,
            .row_count = 3U,
            .context = "CHAR REGEXP observes canonical char storage",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v REGEXP '^[a-d]+$' ORDER BY id",
            .values = class_ids,
            .column_count = 1U,
            .row_count = 3U,
            .context = "REGEXP bracket class range",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v REGEXP '^rss_[^0-9]+$' ORDER BY id",
            .values = negated_class_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "REGEXP negated bracket class",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v NOT REGEXP '^ab' ORDER BY id",
            .values = not_prefix_ids,
            .column_count = 1U,
            .row_count = 4U,
            .context = "NOT REGEXP excludes NULL rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE NOT (v RLIKE '^ab') ORDER BY id",
            .values = not_prefix_ids,
            .column_count = 1U,
            .row_count = 4U,
            .context = "wrapped NOT RLIKE excludes NULL rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM strings WHERE v REGEXP '^ab'",
            .values = aggregate_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "aggregate source filter REGEXP predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE id REGEXP '^1$' ORDER BY id",
            .values = integer_regexp_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "integer REGEXP casts subject to text",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE id <= 3 AND id NOT REGEXP '^1$' ORDER BY id",
            .values = integer_not_regexp_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "integer NOT REGEXP casts subject to text",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_regexp_predicate_dml_persistence(void) {
    static const char *const after_update_rows[] = {
        "1",
        "abc",
        "2",
        "ABC",
        "3",
        "abcd",
        "4",
        "hit",
        "5",
        "rss_",
        "6",
        "hit",
        "7",
        NULL,
        "8",
        "xy",
        "9",
        "abc  ",
    };
    static const char *const after_delete_rows[] = {
        "1",
        "abc",
        "2",
        "ABC",
        "3",
        "abcd",
        "5",
        "rss_",
        "7",
        NULL,
        "8",
        "xy",
        "9",
        "abc  ",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "dml", path, sizeof(path));
    failures += populate_strings(database);
    failures +=
        expect_dml_ok(database, "UPDATE strings SET v = 'hit' WHERE v REGEXP '^rss_.+$'", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM strings ORDER BY id",
            .values = after_update_rows,
            .column_count = 2U,
            .row_count = string_row_count,
            .context = "updated rows after REGEXP predicate",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM strings WHERE v RLIKE '^hit$'", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM strings ORDER BY id",
            .values = after_delete_rows,
            .column_count = 2U,
            .row_count = regexp_remaining_row_count,
            .context = "remaining rows after RLIKE delete",
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
        "REGEXP DML preserves preamble"
    );

    failures += reopen_app_database(&database, path);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM strings ORDER BY id",
            .values = after_delete_rows,
            .column_count = 2U,
            .row_count = regexp_remaining_row_count,
            .context = "REGEXP DML persists after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_regexp_pattern_operators(void) {
    static const char *const plus_quantifier_ids[] = {"2"};
    static const char *const escaped_plus_ids[] = {"1"};
    static const char *const zero_or_more_ids[] = {"4", "5", "6"};
    static const char *const zero_or_one_ids[] = {"4", "5"};
    static const char *const backtracking_ids[] = {"5", "6"};
    static const char *const dot_ids[] = {"8"};
    static const char *const fixed_repeat_optional_group_ids[] = {"9", "10"};
    static const char *const role_alternation_ids[] = {"13", "14"};
    static const char *const not_role_alternation_ids[] = {"15", "16"};
    static const char *const escaped_alternation_ids[] = {"16"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "operators", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE strings (id INT, v VARCHAR(64))", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO strings VALUES "
        "(1, '1+2'), (2, '12'), (3, '1++2'), (4, 'ac'), (5, 'abc'), (6, 'abbc'), "
        "(7, 'a\\nb'), (8, 'axb'), "
        "(9, 'rss_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'), "
        "(10, 'rss_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa_ts'), "
        "(11, 'rss_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa_t'), "
        "(12, 'rss_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa_tsx'), "
        "(13, 'a:1:{s:13:\"administrator\";b:1;}'), "
        "(14, 'a:1:{s:10:\"subscriber\";b:1;}'), "
        "(15, 'a:1:{s:8:\"customer\";b:1;}'), "
        "(16, 'role|literal')",
        NULL
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v REGEXP '1+2' ORDER BY id",
            .values = plus_quantifier_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "REGEXP plus is a quantifier",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v REGEXP '1\\\\+2' ORDER BY id",
            .values = escaped_plus_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "REGEXP escaped plus matches literal plus",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v REGEXP '^ab*c$' ORDER BY id",
            .values = zero_or_more_ids,
            .column_count = 1U,
            .row_count = 3U,
            .context = "REGEXP star allows zero or more matches",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v REGEXP '^ab?c$' ORDER BY id",
            .values = zero_or_one_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "REGEXP question mark allows zero or one match",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v REGEXP '^ab*bc$' ORDER BY id",
            .values = backtracking_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "REGEXP quantifiers backtrack within the supported subset",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v REGEXP '^a.b$' ORDER BY id",
            .values = dot_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "REGEXP dot does not match line terminators by default",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v REGEXP "
                   "'^rss_[0-9a-f]{32}(_ts)?$' ORDER BY id",
            .values = fixed_repeat_optional_group_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "REGEXP fixed repeat with optional literal group",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v REGEXP "
                   "'administrator|editor|author|contributor|subscriber|uploader' "
                   "ORDER BY id",
            .values = role_alternation_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "REGEXP top-level alternation matches WordPress role names",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE id >= 13 AND v NOT REGEXP "
                   "'administrator|editor|author|contributor|subscriber|uploader' "
                   "ORDER BY id",
            .values = not_role_alternation_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "NOT REGEXP top-level alternation excludes role names",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v REGEXP 'role\\\\|literal' ORDER BY id",
            .values = escaped_alternation_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "REGEXP escaped alternation operator matches literal pipe",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_regexp_predicate_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += populate_strings(database);
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE missing RLIKE '^a$'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE v REGEXP 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '1'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE v REGEXP DATABASE()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE v REGEXP '['",
        (struct expected_sql_error){
            .code = mysql_error_regular_expression,
            .sqlstate = "HY000",
            .message_part = "unclosed bracket expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE v REGEXP '[z-a]'",
        (struct expected_sql_error){
            .code = mysql_error_regular_expression_character_range,
            .sqlstate = "HY000",
            .message_part = "invalid character range",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE v REGEXP '(a|b)'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "baseline ASCII regular expression subset",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE v REGEXP '"
        "\xC3"
        "\xA9"
        "'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE REGEXP pattern literals support only ASCII text",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE v REGEXP 'a\\0'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE REGEXP pattern literals do not support NUL bytes",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_regexp_handles(void) {
    static const char *const left_row[] = {"left"};
    static const char *const right_row[] = {"rss_a"};
    char left_path[test_path_capacity];
    char right_path[test_path_capacity];
    mylite_db *left = NULL;
    mylite_db *right = NULL;
    int failures = 0;

    failures += open_app_database(&left, "independent-left", left_path, sizeof(left_path));
    failures += open_app_database(&right, "independent-right", right_path, sizeof(right_path));
    failures += populate_strings(left);
    failures += populate_strings(right);
    failures += expect_dml_ok(left, "UPDATE strings SET v = 'left' WHERE v REGEXP '^rss_a$'", 1);
    failures += expect_query_values(
        left,
        (struct expected_query){
            .sql = "SELECT v FROM strings WHERE id = 4",
            .values = left_row,
            .column_count = 1U,
            .row_count = 1U,
            .context = "left handle REGEXP update",
        }
    );
    failures += expect_query_values(
        right,
        (struct expected_query){
            .sql = "SELECT v FROM strings WHERE id = 4",
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
        "CREATE TABLE strings (id INT, c CHAR(8), v VARCHAR(16), t TEXT)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO strings VALUES "
        "(1, 'abc', 'abc', 'abc'), "
        "(2, 'ABC', 'ABC', 'ABC'), "
        "(3, 'abcd', 'abcd', 'abcd'), "
        "(4, 'rss_a', 'rss_a', 'rss_a'), "
        "(5, 'rss_', 'rss_', 'rss_'), "
        "(6, 'rss_12', 'rss_12', 'rss_12'), "
        "(7, NULL, NULL, NULL), "
        "(8, 'xy', 'xy', 'xy'), "
        "(9, 'abc  ', 'abc  ', 'abc  ')",
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
        "/tmp/mylite-regexp-rlike-predicates-%s-%d.mylite",
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
