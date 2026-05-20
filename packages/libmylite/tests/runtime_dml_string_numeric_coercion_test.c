#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

#include <stdint.h>
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
    numeric_full_column_count = 8,
    numeric_update_column_count = 5,
    mysql_error_data_out_of_range = 1264,
    mysql_error_parse = 1064,
    mysql_error_truncated_wrong_value = 1366,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_strict_quoted_numeric_dml_and_persistence(void);
static int test_insert_ignore_clipping_and_diagnostics(void);
static int test_independent_handles(void);
static int seed_schema(mylite_db *database);
static int create_numeric_table(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
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

    failures += test_strict_quoted_numeric_dml_and_persistence();
    failures += test_insert_ignore_clipping_and_diagnostics();
    failures += test_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_strict_quoted_numeric_dml_and_persistence(void) {
    static const char *const insert_warnings[] = {
        "Note",
        "1265",
        "Data truncated for column 'd' at row 1",
    };
    static const char *const inserted_rows[] = {
        "1",
        "123",
        "4294967295",
        "-9223372036854775808",
        "9223372036854775807",
        "12.35",
        "125",
        "3.5",
    };
    static const char *const updated_row[] = {"1", "-10", "20", "10.00", "-25"};
    static const char *const duplicate_row[] = {"1", "42", "3.46", "4e01"};
    static const char *const replace_rows[] = {
        "1",
        "42",
        "3.46",
        "4e01",
        "2",
        "5",
        "1.25",
        "2.5",
    };
    static const char *const reopened_rows[] = {
        "1",
        "42",
        "3.46",
        "4e01",
        "2",
        "5",
        "1.25",
        "2.5",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "strict") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open strict file");
    failures += seed_schema(database);
    failures += create_numeric_table(database);

    failures += expect_dml_result(
        database,
        "INSERT INTO nums VALUES "
        "(1, '123', '4294967295', '-9223372036854775808', '9223372036854775807', "
        "'12.345', '1.25e2', '3.5')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = insert_warnings,
            .column_count = 3U,
            .row_count = 1U,
            .context = "strict insert warning rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, u, bi, bu, d, f, fl FROM nums",
            .values = inserted_rows,
            .column_count = numeric_full_column_count,
            .row_count = 1U,
            .context = "strict inserted quoted numeric row",
        }
    );

    failures += expect_dml_result(
        database,
        "UPDATE nums SET i = '-10', u = '+20', d = '9.999', f = '-2.5e1' WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, u, d, f FROM nums",
            .values = updated_row,
            .column_count = numeric_update_column_count,
            .row_count = 1U,
            .context = "strict updated quoted numeric row",
        }
    );

    failures += expect_dml_result(
        database,
        "INSERT INTO nums(id, i, u, d, f) VALUES (1, 1, 2, 3.00, 4.00) "
        "ON DUPLICATE KEY UPDATE i = '42', d = '3.456', f = '4e1'",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, d, f FROM nums WHERE id = 1",
            .values = duplicate_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "duplicate update quoted numeric row",
        }
    );

    failures += expect_dml_result(
        database,
        "REPLACE INTO nums VALUES (2, '5', '6', '7', '8', '1.25', '2.5', '4.5')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, d, f FROM nums ORDER BY id",
            .values = replace_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "replace quoted numeric rows",
        }
    );

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "quoted numeric DML preserves preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen strict file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, d, f FROM nums ORDER BY id",
            .values = reopened_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "reopened quoted numeric rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_insert_ignore_clipping_and_diagnostics(void) {
    static const char *const clipped_warnings[] = {
        "Warning",
        "1264",
        "Out of range value for column 'i' at row 1",
        "Warning",
        "1264",
        "Out of range value for column 'u' at row 1",
        "Warning",
        "1264",
        "Out of range value for column 'd' at row 1",
    };
    static const char *const clipped_row[] = {"3", "2147483647", "0", "999.99"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open diagnostics db");
    failures += seed_schema(database);
    failures += create_numeric_table(database);

    failures += execute_error(
        database,
        "INSERT INTO nums(id, i) VALUES (2, 'abc')",
        (struct expected_sql_error){
            .code = mysql_error_truncated_wrong_value,
            .sqlstate = "HY000",
            .message_part = "Incorrect integer value: 'abc' for column 'i' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO nums(id, i) VALUES (2, '999999999999999999999')",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO nums(id, f) VALUES (2, ' 1')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "without whitespace",
        }
    );

    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO nums(id, i, u, d) VALUES "
        "(3, '999999999999999999999', '-1', '9999.99')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 3U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = clipped_warnings,
            .column_count = 3U,
            .row_count = 3U,
            .context = "INSERT IGNORE quoted numeric warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, u, d FROM nums WHERE id = 3",
            .values = clipped_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "INSERT IGNORE clipped quoted numeric row",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_independent_handles(void) {
    static const char *const first_rows[] = {"5"};
    static const char *const second_rows[] = {"2"};
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += seed_schema(first);
    failures += seed_schema(second);
    failures += create_numeric_table(first);
    failures += create_numeric_table(second);
    failures += expect_dml_result(
        first,
        "INSERT INTO nums(id, i) VALUES (1, '1')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_result(
        second,
        "INSERT INTO nums(id, i) VALUES (1, '2')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_result(
        first,
        "UPDATE nums SET i = '5' WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT i FROM nums",
            .values = first_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first handle quoted numeric row",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT i FROM nums",
            .values = second_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second handle quoted numeric row",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int seed_schema(mylite_db *database) {
    int failures = expect_statement_ok(database, "CREATE DATABASE app");

    failures += expect_statement_ok(database, "USE app");
    return failures;
}

static int create_numeric_table(mylite_db *database) {
    return expect_statement_ok(
        database,
        "CREATE TABLE nums("
        "id INT PRIMARY KEY, i INT, u INT UNSIGNED, bi BIGINT, bu BIGINT UNSIGNED, "
        "d DECIMAL(5,2), f DOUBLE, fl FLOAT)"
    );
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

static int expect_statement_ok(mylite_db *database, const char *sql) {
    return expect_statement_result(
        database,
        sql,
        (struct expected_dml_result){
            .affected_rows = 0,
            .warning_count = 0U,
        }
    );
}

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, "DML warnings");
    mylite_result_free(result);

    (void)expected.affected_rows;
    return failures;
}

static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "DML column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "DML row count");
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, "DML affected");
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, "DML warnings");
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
        "%s/mylite_dml_string_numeric_%d_%s.mylite",
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
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %s\n", path);
        fclose(file);
        return 1;
    }
    fclose(file);

    return 0;
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
            "%s: expected text '%s', got '%s'\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected '%s' to contain '%s'\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
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
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
