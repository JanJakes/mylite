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
    test_path_suffix_capacity = 16,
    mysql_error_bad_null = 1048,
    mysql_error_unknown_column = 1054,
    mysql_error_duplicate_entry = 1062,
    mysql_error_parse = 1064,
    mysql_error_data_out_of_range = 1264,
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

static int test_multiple_assignment_success_persistence_and_limits(void);
static int test_multiple_assignment_errors_and_atomicity(void);
static int test_independent_multiple_assignment_handles(void);
static int open_app_database(const char *path, mylite_db **out_database);
static int reset_rows_table(mylite_db *database);
static int reset_key_table(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_update_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
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

    failures += test_multiple_assignment_success_persistence_and_limits();
    failures += test_multiple_assignment_errors_and_atomicity();
    failures += test_independent_multiple_assignment_handles();

    return failures == 0 ? 0 : 1;
}

static int test_multiple_assignment_success_persistence_and_limits(void) {
    static const char *const after_partial_update[] = {
        "1",
        "1",
        "2",
        "2025-01-01 00:00:00",
        "2",
        "1",
        "2",
        "2023-11-14 22:14:20",
        "3",
        NULL,
        "5",
        "2025-01-01 00:00:00",
    };
    static const char *const after_null_update[] = {"3", NULL, "9"};
    static const char *const after_defaults[] = {"1", "11", "d"};
    static const char *const after_signed_literals[] = {"2", "-2", "3"};
    static const char *const after_explicit_timestamp[] = {
        "1",
        "100",
        "2023-11-14 22:15:20",
    };
    static const char *const after_order_limit[] = {
        "1",
        "100",
        "2",
        "2",
        "-2",
        "3",
        "3",
        "10",
        "20",
    };
    static const char *const after_reopen[] = {
        "1",
        "100",
        "2023-11-14 22:15:20",
        "2",
        "-2",
        "2023-11-14 22:15:20",
        "3",
        "10",
        "2023-11-14 22:16:20",
    };
    static const char *const after_composite_key_update[] = {
        "71",
        "71",
        "nav_menu",
        "",
        "0",
    };
    static const char *const after_primary_key_update[] = {
        "72",
        "72",
        "post_tag",
        "renamed",
        "0",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += open_app_database(path, &database);
    failures += reset_rows_table(database);
    failures += execute_ok(database, "SET timestamp = 1700000060", NULL);
    failures += expect_update_ok(database, "UPDATE rows_t SET a = 1, b = 2 WHERE id IN (1, 2)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, b, ts FROM rows_t ORDER BY id",
            .values = after_partial_update,
            .column_count = 4U,
            .row_count = 3U,
            .context = "partial multiple assignment update",
        }
    );
    failures += execute_ok(database, "SET timestamp = 1700000120", NULL);
    failures += expect_update_ok(database, "UPDATE rows_t SET a = 1, b = 2 WHERE id IN (1, 2)", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, b, ts FROM rows_t ORDER BY id",
            .values = after_partial_update,
            .column_count = 4U,
            .row_count = 3U,
            .context = "all no-op multiple assignment update",
        }
    );
    failures += expect_update_ok(database, "UPDATE rows_t SET a = NULL, b = 9 WHERE id = 3", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, b FROM rows_t WHERE id = 3",
            .values = after_null_update,
            .column_count = 3U,
            .row_count = 1U,
            .context = "nullable null multiple assignment",
        }
    );
    failures +=
        expect_update_ok(database, "UPDATE rows_t SET nn = DEFAULT, s = DEFAULT WHERE id = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn, s FROM rows_t WHERE id = 1",
            .values = after_defaults,
            .column_count = 3U,
            .row_count = 1U,
            .context = "default multiple assignment",
        }
    );
    failures += expect_update_ok(database, "UPDATE rows_t SET a = -2, b = +3 WHERE id = 2", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, b FROM rows_t WHERE id = 2",
            .values = after_signed_literals,
            .column_count = 3U,
            .row_count = 1U,
            .context = "signed literal multiple assignment",
        }
    );
    failures += execute_ok(database, "SET timestamp = 1700000120", NULL);
    failures += expect_update_ok(
        database,
        "UPDATE rows_t SET a = 100, ts = CURRENT_TIMESTAMP WHERE id = 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, ts FROM rows_t WHERE id = 1",
            .values = after_explicit_timestamp,
            .column_count = 3U,
            .row_count = 1U,
            .context = "explicit current timestamp multiple assignment",
        }
    );
    failures += execute_ok(database, "SET timestamp = 1700000180", NULL);
    failures +=
        expect_update_ok(database, "UPDATE rows_t SET a = 10, b = 20 ORDER BY id DESC LIMIT 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, b FROM rows_t ORDER BY id",
            .values = after_order_limit,
            .column_count = 3U,
            .row_count = 3U,
            .context = "ordered limited multiple assignment",
        }
    );
    failures += expect_update_ok(database, "UPDATE rows_t SET nn = NULL, b = 99 WHERE id = 999", 0);
    failures += expect_update_ok(database, "UPDATE rows_t SET nn = NULL, b = 99 LIMIT 0", 0);
    failures += reset_key_table(database);
    failures += expect_update_ok(
        database,
        "UPDATE key_t SET term_id = 71, taxonomy = 'nav_menu', description = '', parent = 0 "
        "WHERE term_taxonomy_id = 71",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT term_taxonomy_id, term_id, taxonomy, description, parent "
                   "FROM key_t WHERE term_taxonomy_id = 71",
            .values = after_composite_key_update,
            .column_count = 5U,
            .row_count = 1U,
            .context = "composite unique key multiple assignment",
        }
    );
    failures += expect_update_ok(
        database,
        "UPDATE key_t SET term_taxonomy_id = 72, term_id = 72, taxonomy = 'post_tag', "
        "description = 'renamed' WHERE term_taxonomy_id = 71",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT term_taxonomy_id, term_id, taxonomy, description, parent "
                   "FROM key_t WHERE term_taxonomy_id = 72",
            .values = after_primary_key_update,
            .column_count = 5U,
            .row_count = 1U,
            .context = "primary key multiple assignment",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "multiple assignment preserves file preamble"
    );

    failures +=
        expect_int(mylite_open(path, &database), MYLITE_OK, "reopen multiple assignment file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, ts FROM rows_t ORDER BY id",
            .values = after_reopen,
            .column_count = 3U,
            .row_count = 3U,
            .context = "multiple assignment persists after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_multiple_assignment_errors_and_atomicity(void) {
    static const struct expected_sql_error parse_unsupported = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "UPDATE multiple assignments",
    };
    static const char *const unchanged_after_error[] = {"1", "2", "3", "one"};
    static const char *const unchanged_key_after_error[] = {
        "71",
        "0",
        "nav_menu",
        "pending",
        "1",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "errors") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += open_app_database(path, &database);
    failures += reset_rows_table(database);

    failures += execute_error(
        database,
        "UPDATE rows_t SET a = 1, missing = 2 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE rows_t SET nn = NULL, b = 99 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'nn' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "UPDATE rows_t SET a = 123456789012345678901234, b = 99 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'a'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, nn, s FROM rows_t WHERE id = 1",
            .values = unchanged_after_error,
            .column_count = 4U,
            .row_count = 1U,
            .context = "failed multiple assignment leaves row unchanged",
        }
    );
    failures +=
        execute_error(database, "UPDATE rows_t SET a = 7, a = 8 WHERE id = 1", parse_unsupported);
    failures += execute_error(
        database,
        "UPDATE rows_t SET rows_t.a = 1, b = 2 WHERE id = 1",
        parse_unsupported
    );
    failures += execute_error(
        database,
        "UPDATE rows_t SET a = a + 1, b = 2 WHERE id = 1",
        parse_unsupported
    );
    failures += execute_error(
        database,
        "UPDATE rows_t SET a = 1, b = (SELECT a FROM rows_t LIMIT 1) WHERE id = 1",
        parse_unsupported
    );
    failures +=
        execute_error(database, "UPDATE rows_t SET id = 5, a = 1 WHERE id = 1", parse_unsupported);
    failures += reset_key_table(database);
    failures += execute_error(
        database,
        "UPDATE key_t SET term_id = 70, taxonomy = 'category', description = 'dup' "
        "WHERE term_taxonomy_id = 71",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_entry,
            .sqlstate = "23000",
            .message_part = "Duplicate entry",
        }
    );
    failures += execute_error(
        database,
        "UPDATE key_t SET term_taxonomy_id = 70, description = 'dup' "
        "WHERE term_taxonomy_id = 71",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_entry,
            .sqlstate = "23000",
            .message_part = "Duplicate entry",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT term_taxonomy_id, term_id, taxonomy, description, parent "
                   "FROM key_t WHERE term_taxonomy_id = 71",
            .values = unchanged_key_after_error,
            .column_count = 5U,
            .row_count = 1U,
            .context = "failed key multiple assignment leaves row unchanged",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_independent_multiple_assignment_handles(void) {
    static const char *const first_expected[] = {"1", "44", "55"};
    static const char *const second_expected[] = {"1", "1", "2"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += open_app_database(first_path, &first);
    failures += open_app_database(second_path, &second);
    failures += reset_rows_table(first);
    failures += reset_rows_table(second);
    failures += expect_update_ok(first, "UPDATE rows_t SET a = 44, b = 55 WHERE id = 1", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, a, b FROM rows_t WHERE id = 1",
            .values = first_expected,
            .column_count = 3U,
            .row_count = 1U,
            .context = "first independent multiple assignment",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, a, b FROM rows_t WHERE id = 1",
            .values = second_expected,
            .column_count = 3U,
            .row_count = 1U,
            .context = "second independent multiple assignment unchanged",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);

    return failures;
}

static int open_app_database(const char *path, mylite_db **out_database) {
    int failures = expect_int(mylite_open(path, out_database), MYLITE_OK, "open app database");

    failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    failures += execute_ok(*out_database, "USE app", NULL);

    return failures;
}

static int reset_rows_table(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(database, "DROP TABLE IF EXISTS rows_t", NULL);
    failures += execute_ok(database, "SET timestamp = 1700000000", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE rows_t ("
        "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
        "a INT NULL, "
        "b INT NULL, "
        "nn INT NOT NULL DEFAULT 11, "
        "u_unique INT UNIQUE, "
        "s VARCHAR(20) NULL DEFAULT 'd', "
        "ts TIMESTAMP NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO rows_t(a, b, nn, u_unique, s, ts) VALUES "
        "(1, 2, 3, 4, 'one', '2025-01-01 00:00:00'), "
        "(1, 5, 3, 5, 'two', '2025-01-01 00:00:00'), "
        "(NULL, 5, 3, 6, NULL, '2025-01-01 00:00:00')",
        NULL
    );

    return failures;
}

static int reset_key_table(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(database, "DROP TABLE IF EXISTS key_t", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE key_t ("
        "term_taxonomy_id BIGINT UNSIGNED NOT NULL PRIMARY KEY, "
        "term_id BIGINT UNSIGNED NOT NULL, "
        "taxonomy VARCHAR(32) NOT NULL, "
        "description TEXT NOT NULL, "
        "parent BIGINT UNSIGNED NOT NULL DEFAULT 0, "
        "UNIQUE KEY term_id_taxonomy (term_id, taxonomy), "
        "KEY parent (parent))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO key_t(term_taxonomy_id, term_id, taxonomy, description, parent) VALUES "
        "(70, 70, 'category', 'existing', 0), "
        "(71, 0, 'nav_menu', 'pending', 1)",
        NULL
    );

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *local_result = NULL;
    mylite_result **result_slot = out_result == NULL ? &local_result : out_result;
    int rc = mylite_execute(database, sql, strlen(sql), result_slot);

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
        mylite_result_free(local_result);
        return 1;
    }

    mylite_result_free(local_result);
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int expect_update_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "update column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "update row count");
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, "update affected");
    failures += expect_size(mylite_result_warning_count(result), 0U, "update warning count");
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
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
        return expect_true(actual == NULL, context);
    }

    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_update_multiple_assignments_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
        return 1;
    }

    return 0;
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
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        perror(path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        perror("fseek");
        fclose(file);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    fclose(file);

    return read_count == size ? 0 : 1;
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

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
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

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
