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
    related_file_suffix_capacity = 16,
    schema_sql_capacity = 128,
    mysql_error_parse = 1064,
    mysql_error_duplicate_entry = 1062,
    mysql_error_column_count_mismatch = 1136,
    mysql_error_select_reduced = 1222,
    mysql_error_field_no_default = 1364,
    mysql_error_bad_null = 1048,
    mysql_error_data_truncated = 1265,
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

static int test_insert_select_union_success_persistence_and_file_safety(void);
static int test_insert_select_union_diagnostics_and_unsupported_forms(void);
static int test_insert_select_union_independent_handles(void);
static int seed_schema(mylite_db *database, const char *name);
static int seed_union_source_tables(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
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

    failures += test_insert_select_union_success_persistence_and_file_safety();
    failures += test_insert_select_union_diagnostics_and_unsupported_forms();
    failures += test_insert_select_union_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_insert_select_union_success_persistence_and_file_safety(void) {
    static const char *const scalar_rows[] = {
        "1",
        "10",
        "a",
        "2",
        NULL,
        "b",
    };
    static const char *const table_rows[] = {
        "1",
        "10",
        "a",
        "2",
        NULL,
        "b",
        "3",
        "30",
        "c",
    };
    static const char *const same_table_rows[] = {
        "1",
        "10",
        "a",
        "1",
        "10",
        "a",
        "2",
        NULL,
        "b",
    };
    static const char *const qualified_rows[] = {
        "1",
        "10",
        "a",
        "3",
        "30",
        "c",
    };
    static const char *const case_rows[] = {
        "4",
        "40",
        "a",
    };
    static const char *const mixed_rows[] = {
        "5",
        "50",
        "m",
        "5",
        "50",
        "m",
    };
    static const char *const auto_rows[] = {"1", "10", "2", "20"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "insert_select_union") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open union insert file");
    failures += seed_schema(database, "app");
    failures += seed_union_source_tables(database);
    failures += expect_dml_ok(
        database,
        "CREATE TABLE qualified_dst(id INT NOT NULL, n INT NULL, v VARCHAR(10))",
        0
    );

    failures += expect_dml_ok(
        database,
        "INSERT INTO dst(id, n, v) "
        "SELECT 1, 10, 'a' UNION SELECT 1, 10, 'a' UNION SELECT 2, NULL, 'b'",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, v FROM dst ORDER BY id",
            .values = scalar_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "scalar union source rows",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM dst", 2);

    failures += expect_dml_ok(
        database,
        "INSERT INTO dst(id, n, v) SELECT 4, 40, 'a' UNION SELECT 4, 40, 'A'",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, v FROM dst ORDER BY id",
            .values = case_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "scalar union string duplicate uses collation",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM dst", 1);

    failures += expect_dml_ok(
        database,
        "INSERT INTO dst(id, n, v) "
        "SELECT 5, 50, 'm' UNION ALL SELECT 5, 50, 'm' "
        "UNION SELECT 5, 50, 'm' UNION ALL SELECT 5, 50, 'm'",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, v FROM dst ORDER BY id",
            .values = mixed_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "mixed union all and distinct source rows",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM dst", 2);

    failures += expect_dml_ok(
        database,
        "INSERT INTO dst(id, n, v) "
        "SELECT id, n, v FROM src_a WHERE id <= 2 "
        "UNION SELECT id, n, v FROM src_b WHERE id >= 2",
        3
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, v FROM dst ORDER BY id",
            .values = table_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "table union source rows",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM dst", 3);

    failures += expect_dml_ok(
        database,
        "CREATE TABLE case_src_a(id INT NOT NULL, n INT NULL, v VARCHAR(10))",
        0
    );
    failures += expect_dml_ok(
        database,
        "CREATE TABLE case_src_b(id INT NOT NULL, n INT NULL, v VARCHAR(10))",
        0
    );
    failures += expect_dml_ok(database, "INSERT INTO case_src_a VALUES (4, 40, 'a')", 1);
    failures += expect_dml_ok(database, "INSERT INTO case_src_b VALUES (4, 40, 'A')", 1);
    failures += expect_dml_ok(
        database,
        "INSERT INTO dst SELECT * FROM case_src_a UNION SELECT * FROM case_src_b",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, v FROM dst ORDER BY id",
            .values = case_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "table union string duplicate uses collation",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM dst", 1);

    failures +=
        expect_dml_ok(database, "INSERT INTO dst SELECT * FROM src_a UNION SELECT * FROM src_b", 3);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, v FROM dst ORDER BY id",
            .values = table_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "wildcard union source rows",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM dst", 3);

    failures += expect_dml_ok(
        database,
        "INSERT INTO src_a SELECT * FROM src_a WHERE id = 1 UNION SELECT * FROM src_a WHERE id = 4",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, v FROM src_a ORDER BY id",
            .values = same_table_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "same table union source target",
        }
    );

    failures += expect_dml_ok(database, "INSERT INTO auto_dst(n) SELECT 10 UNION ALL SELECT 20", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n FROM auto_dst ORDER BY id",
            .values = auto_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "union source auto increment rows",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(
        read_file_at(path, 0, actual_preamble, sizeof(actual_preamble)),
        0,
        "read preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "union insert preserves preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen union insert file");
    failures += expect_dml_ok(
        database,
        "INSERT INTO app.qualified_dst(id, n, v) "
        "SELECT id, n, v FROM app.src_a WHERE id = 1 "
        "UNION SELECT id, n, v FROM app.src_b WHERE id = 3",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, v FROM app.qualified_dst ORDER BY id",
            .values = qualified_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "schema-qualified union source without selected schema",
        }
    );
    failures += expect_dml_ok(database, "USE app", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n FROM auto_dst ORDER BY id",
            .values = auto_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "union source auto increment persists",
        }
    );

    if (database != NULL) {
        mylite_close(database);
    }
    remove_related_files(path);
    return failures;
}

static int test_insert_select_union_diagnostics_and_unsupported_forms(void) {
    static const char *const zero_count[] = {"0"};
    static const char *const rollback_rows[] = {"9", "90"};
    static const char *const odku_rows[] = {"1", "10", "a", "2", "20", "b"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "insert_select_union_diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += seed_schema(database, "diag");
    failures += seed_union_source_tables(database);
    failures += expect_dml_ok(
        database,
        "CREATE TABLE required_target(id INT NOT NULL, must INT NOT NULL)",
        0
    );
    failures +=
        expect_dml_ok(database, "CREATE TABLE short_strings(id INT NOT NULL, v VARCHAR(3))", 0);
    failures +=
        expect_dml_ok(database, "CREATE TABLE wide_strings(id INT NOT NULL, v VARCHAR(8))", 0);
    failures += expect_dml_ok(database, "INSERT INTO wide_strings VALUES (1, 'abcd')", 1);
    failures += expect_dml_ok(
        database,
        "INSERT INTO required_target(id) "
        "SELECT id FROM src_a WHERE id > 9 UNION SELECT id FROM src_b WHERE id < 0",
        0
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM required_target",
            .values = zero_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "zero row compound source",
        }
    );

    failures += execute_error(
        database,
        "INSERT INTO dst(id, n, v) SELECT 1 UNION SELECT 2",
        (struct expected_sql_error){
            .code = mysql_error_column_count_mismatch,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO dst(id, n, v) SELECT 1, 2, 'a' UNION SELECT 3",
        (struct expected_sql_error){
            .code = mysql_error_select_reduced,
            .sqlstate = "21000",
            .message_part = "different number of columns",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO dst(id, n, v) SELECT 1 UNION SELECT 2, 3",
        (struct expected_sql_error){
            .code = mysql_error_select_reduced,
            .sqlstate = "21000",
            .message_part = "different number of columns",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO required_target(id) SELECT 1 UNION SELECT 2",
        (struct expected_sql_error){
            .code = mysql_error_field_no_default,
            .sqlstate = "HY000",
            .message_part = "doesn't have a default value",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO required_target(id, must) SELECT 1, NULL UNION SELECT 2, 2",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "cannot be null",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO short_strings "
        "SELECT id, v FROM wide_strings WHERE id = 1 "
        "UNION ALL SELECT id, v FROM wide_strings WHERE id = 99",
        (struct expected_sql_error){
            .code = mysql_error_data_truncated,
            .sqlstate = "01000",
            .message_part = "Data truncated for column 'v' at row 1",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM short_strings",
            .values = zero_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "compound varchar truncation rollback",
        }
    );

    failures +=
        expect_dml_ok(database, "CREATE TABLE pk_target(id INT PRIMARY KEY, n INT NOT NULL)", 0);
    failures += expect_dml_ok(database, "INSERT INTO pk_target VALUES (9, 90)", 1);
    failures += execute_error(
        database,
        "INSERT INTO pk_target SELECT 1, 10 UNION ALL SELECT 1, 20",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_entry,
            .sqlstate = "23000",
            .message_part = "Duplicate entry",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n FROM pk_target ORDER BY id",
            .values = rollback_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "duplicate union source rollback",
        }
    );

    failures += execute_error(
        database,
        "INSERT IGNORE INTO dst SELECT 1, 10, 'a' UNION SELECT 2, 20, 'b'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UNION sources",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO dst SELECT id, n, v FROM src_a ORDER BY id UNION SELECT id, n, v FROM src_b",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UNION branch ORDER BY",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO dst SELECT id, n, v FROM src_a UNION SELECT id, n, v FROM src_b LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UNION branch LIMIT",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO dst SELECT 1, 10, 'a' UNION SELECT 2, 20, 'b' "
        "ON DUPLICATE KEY UPDATE v = 'x'",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, v FROM dst ORDER BY id",
            .values = odku_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "union source duplicate tail rows",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM dst", 2);
    failures += execute_error(
        database,
        "INSERT INTO dst SELECT src_a.id, src_a.n, src_a.v FROM src_a JOIN src_b",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "joined SELECT",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO dst SELECT 1, 10, 'a' UNION SELECT 2, 20, 'b'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );

    if (database != NULL) {
        mylite_close(database);
    }
    remove_related_files(path);
    return failures;
}

static int test_insert_select_union_independent_handles(void) {
    static const char *const first_rows[] = {"1", "10", "2", "20"};
    static const char *const second_rows[] = {"3", "30"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "insert_select_union_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "insert_select_union_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += seed_schema(first, "first_app");
    failures += seed_schema(second, "second_app");
    failures += expect_dml_ok(first, "CREATE TABLE dst(id INT NOT NULL, n INT NOT NULL)", 0);
    failures += expect_dml_ok(second, "CREATE TABLE dst(id INT NOT NULL, n INT NOT NULL)", 0);
    failures += expect_dml_ok(first, "INSERT INTO dst SELECT 1, 10 UNION ALL SELECT 2, 20", 2);
    failures += expect_dml_ok(second, "INSERT INTO dst SELECT 3, 30 UNION SELECT 3, 30", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, n FROM dst ORDER BY id",
            .values = first_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "first independent union rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, n FROM dst ORDER BY id",
            .values = second_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second independent union rows",
        }
    );

    if (first != NULL) {
        mylite_close(first);
    }
    if (second != NULL) {
        mylite_close(second);
    }
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int seed_schema(mylite_db *database, const char *name) {
    char sql[schema_sql_capacity];
    mylite_result *result = NULL;
    int written = snprintf(sql, sizeof(sql), "CREATE DATABASE %s", name);
    int rc = MYLITE_OK;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        return 1;
    }
    rc = execute_ok(database, sql, &result);
    mylite_result_free(result);
    result = NULL;
    if (rc != 0) {
        return rc;
    }

    written = snprintf(sql, sizeof(sql), "USE %s", name);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        return 1;
    }
    rc = execute_ok(database, sql, &result);
    mylite_result_free(result);
    return rc;
}

static int seed_union_source_tables(mylite_db *database) {
    int failures = 0;

    failures +=
        expect_dml_ok(database, "CREATE TABLE dst(id INT NOT NULL, n INT NULL, v VARCHAR(10))", 0);
    failures += expect_dml_ok(
        database,
        "CREATE TABLE src_a(id INT NOT NULL, n INT NULL, v VARCHAR(10))",
        0
    );
    failures += expect_dml_ok(
        database,
        "CREATE TABLE src_b(id INT NOT NULL, n INT NULL, v VARCHAR(10))",
        0
    );
    failures += expect_dml_ok(database, "INSERT INTO src_a VALUES (1, 10, 'a'), (2, NULL, 'b')", 2);
    failures += expect_dml_ok(database, "INSERT INTO src_b VALUES (2, NULL, 'b'), (3, 30, 'c')", 2);
    failures += expect_dml_ok(
        database,
        "CREATE TABLE auto_dst(id INT AUTO_INCREMENT PRIMARY KEY, n INT NOT NULL)",
        0
    );

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s failed: %s (%s)\n",
            sql,
            mylite_errmsg(database),
            mylite_sqlstate(database)
        );
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s unexpectedly succeeded\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, expected.message_part);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, expected.message_part);
    failures +=
        expect_contains(mylite_errmsg(database), expected.message_part, expected.message_part);
    mylite_result_free(result);

    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (failures == 0) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                size_t index = (row * query.column_count) + column;

                failures +=
                    expect_result_value(result, row, column, query.values[index], query.context);
            }
        }
    }
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

    if (expected == NULL && actual == NULL) {
        return 0;
    }
    if (expected != NULL && actual != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(
        stderr,
        "%s value mismatch at row %zu column %zu: expected %s, got %s\n",
        context,
        row,
        column,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written =
        snprintf(path, path_size, "/tmp/mylite_runtime_%s_%d.mylite", name, current_process_id());

    return (written < 0 || (size_t)written >= path_size) ? 1 : 0;
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
    char related[test_path_capacity + related_file_suffix_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related)) {
        remove(related);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    fclose(file);

    return read_count == size ? 0 : 1;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "NULL" : actual,
        needle == NULL ? "NULL" : needle
    );
    return 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) == 0) {
        return 0;
    }

    fprintf(stderr, "%s: byte comparison failed\n", context);
    return 1;
}
