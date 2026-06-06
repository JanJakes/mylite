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
    varchar_ge_match_count = 5,
    iso_row_count = 5,
    post_delete_string_row_count = 5,
    string_row_count = 8,
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
    size_t warning_count;
    const char *context;
};

static int test_string_range_predicate_queries(void);
static int test_iso_like_string_ranges(void);
static int test_string_range_predicate_dml_persistence(void);
static int test_string_range_predicate_diagnostics(void);
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

    failures += test_string_range_predicate_queries();
    failures += test_iso_like_string_ranges();
    failures += test_string_range_predicate_dml_persistence();
    failures += test_string_range_predicate_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_string_range_predicate_queries(void) {
    static const char *const varchar_gt_ids[] = {"3", "5", "7"};
    static const char *const varchar_ge_ids[] = {"1", "2", "3", "5", "7"};
    static const char *const varchar_lt_ids[] = {"4", "8"};
    static const char *const varchar_le_ids[] = {"1", "2", "4", "8"};
    static const char *const varchar_between_ids[] = {"1", "2", "4"};
    static const char *const varchar_not_between_ids[] = {"3", "5", "7", "8"};
    static const char *const varchar_in_ids[] = {"1", "2", "3"};
    static const char *const varchar_not_in_ids[] = {"4", "5", "7", "8"};
    static const char *const varchar_in_null_ids[] = {"1", "2"};
    static const char *const char_between_ids[] = {"1", "2", "4", "5"};
    static const char *const text_gt_ids[] = {"3", "5", "7"};
    static const char *const logical_ids[] = {"3", "4", "5", "7"};
    static const char *const aggregate_count[] = {"3"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "queries", path, sizeof(path));
    failures += populate_strings(database);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v > 'abc' ORDER BY id",
            .values = varchar_gt_ids,
            .column_count = 1U,
            .row_count = 3U,
            .context = "varchar greater-than uses collation and trailing spaces",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v >= 'abc' ORDER BY id",
            .values = varchar_ge_ids,
            .column_count = 1U,
            .row_count = varchar_ge_match_count,
            .context = "varchar greater-equal folds ASCII case",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v < 'abc' ORDER BY id",
            .values = varchar_lt_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "varchar less-than uses collation",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v <= 'abc' ORDER BY id",
            .values = varchar_le_ids,
            .column_count = 1U,
            .row_count = 4U,
            .context = "varchar less-equal folds ASCII case",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v BETWEEN 'ab' AND 'abc' ORDER BY id",
            .values = varchar_between_ids,
            .column_count = 1U,
            .row_count = 3U,
            .context = "varchar between excludes trailing-space value above upper bound",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v NOT BETWEEN 'ab' AND 'abc' ORDER BY id",
            .values = varchar_not_between_ids,
            .column_count = 1U,
            .row_count = 4U,
            .context = "varchar not between excludes nulls",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v IN ('abc', 'abd') ORDER BY id",
            .values = varchar_in_ids,
            .column_count = 1U,
            .row_count = 3U,
            .context = "varchar IN folds ASCII case",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v NOT IN ('abc', 'abd') ORDER BY id",
            .values = varchar_not_in_ids,
            .column_count = 1U,
            .row_count = 4U,
            .context = "varchar NOT IN excludes nulls",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v IN ('abc', NULL) ORDER BY id",
            .values = varchar_in_null_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "varchar IN with NULL list value matches equal values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v NOT IN ('abc', NULL) ORDER BY id",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "varchar NOT IN with NULL list value has no true nonmatching rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE c BETWEEN 'ab' AND 'abc' ORDER BY id",
            .values = char_between_ids,
            .column_count = 1U,
            .row_count = 4U,
            .context = "char between uses canonical char storage",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE t > 'abc' ORDER BY id",
            .values = text_gt_ids,
            .column_count = 1U,
            .row_count = 3U,
            .context = "text greater-than mirrors varchar range behavior",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v > 'abc' OR t IN ('ab') ORDER BY id",
            .values = logical_ids,
            .column_count = 1U,
            .row_count = 4U,
            .context = "string range predicates compose with logical operators",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM strings WHERE v > 'abc'",
            .values = aggregate_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "aggregate source filter string range predicate",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_iso_like_string_ranges(void) {
    static const char *const between_ids[] = {"2", "3"};
    static const char *const greater_ids[] = {"3", "4"};
    static const char *const ordered_ids[] = {"5", "1", "2", "3", "4"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "iso", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE dates (id INT, option_value VARCHAR(32))", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO dates VALUES "
        "(1, '2016-01-14T23:59:59Z'), "
        "(2, '2016-01-15T00:00:00Z'), "
        "(3, '2016-01-15T12:00:00Z'), "
        "(4, '2016-01-16T00:00:00Z'), "
        "(5, NULL)",
        NULL
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM dates WHERE option_value BETWEEN "
                   "'2016-01-15T00:00:00Z' AND '2016-01-15T23:59:59Z' ORDER BY id",
            .values = between_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "iso-like varchar between uses string comparison",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM dates WHERE option_value > '2016-01-15T00:00:00Z' "
                   "ORDER BY id",
            .values = greater_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "iso-like varchar greater-than uses string comparison",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM dates ORDER BY option_value",
            .values = ordered_ids,
            .column_count = 1U,
            .row_count = iso_row_count,
            .context = "iso-like varchar order keeps null first",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_string_range_predicate_dml_persistence(void) {
    static const char *const after_update_rows[] = {
        "1",
        "old",
        "2",
        "old",
        "3",
        "range",
        "4",
        "old",
        "5",
        "range",
        "6",
        "old",
        "7",
        "range",
        "8",
        "old",
    };
    static const char *const after_delete_rows[] = {
        "3",
        "abd",
        "range",
        "5",
        "abc  ",
        "range",
        "6",
        NULL,
        "old",
        "7",
        "b",
        "range",
        "8",
        "aa",
        "old",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "dml", path, sizeof(path));
    failures += populate_strings(database);
    failures += expect_dml_ok(database, "UPDATE strings SET flag = 'range' WHERE v > 'abc'", 3);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flag FROM strings ORDER BY id",
            .values = after_update_rows,
            .column_count = 2U,
            .row_count = string_row_count,
            .context = "updated rows after string greater-than predicate",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM strings WHERE v BETWEEN 'ab' AND 'abc'", 3);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, flag FROM strings ORDER BY id",
            .values = after_delete_rows,
            .column_count = 3U,
            .row_count = post_delete_string_row_count,
            .context = "remaining rows after string between delete",
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
        "string range DML preserves preamble"
    );

    failures += reopen_app_database(&database, path);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, flag FROM strings ORDER BY id",
            .values = after_delete_rows,
            .column_count = 3U,
            .row_count = post_delete_string_row_count,
            .context = "string range DML persists after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_string_range_predicate_diagnostics(void) {
    static const char *const integer_string_coercion_ids[] = {
        "1",
        "2",
        "3",
        "4",
        "5",
        "6",
        "7",
        "8",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += populate_strings(database);
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE missing > 'abc'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v > 1",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "numeric string range predicate no match",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE v BETWEEN 'a' AND 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE string predicates support only string literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE v IN ('abc', 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE string predicates support only string literals",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE id > 'abc' ORDER BY id",
            .values = integer_string_coercion_ids,
            .column_count = 1U,
            .row_count =
                sizeof(integer_string_coercion_ids) / sizeof(integer_string_coercion_ids[0]),
            .warning_count = 1U,
            .context = "integer predicate coerces nonnumeric string literal",
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
        "CREATE TABLE strings (id INT, c CHAR(5), v VARCHAR(8), t TEXT, flag VARCHAR(8))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO strings VALUES "
        "(1, 'abc', 'abc', 'abc', 'old'), "
        "(2, 'ABC', 'ABC', 'ABC', 'old'), "
        "(3, 'abd', 'abd', 'abd', 'old'), "
        "(4, 'ab', 'ab', 'ab', 'old'), "
        "(5, 'abc  ', 'abc  ', 'abc  ', 'old'), "
        "(6, NULL, NULL, NULL, 'old'), "
        "(7, 'b', 'b', 'b', 'old'), "
        "(8, 'aa', 'aa', 'aa', 'old')",
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
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);

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
        "/tmp/mylite-string-range-predicates-%s-%d.mylite",
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
