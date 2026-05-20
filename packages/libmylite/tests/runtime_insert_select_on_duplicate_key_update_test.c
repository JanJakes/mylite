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
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
    mysql_error_bad_null = 1048,
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

struct expected_dml {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_insert_select_duplicate_success_and_persistence(void);
static int test_insert_select_duplicate_source_shapes(void);
static int test_insert_select_duplicate_diagnostics(void);
static int test_insert_select_duplicate_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, struct expected_dml expected);
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

    failures += test_insert_select_duplicate_success_and_persistence();
    failures += test_insert_select_duplicate_source_shapes();
    failures += test_insert_select_duplicate_diagnostics();
    failures += test_insert_select_duplicate_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_insert_select_duplicate_success_and_persistence(void) {
    static const char *const warning_row[] = {
        "Warning",
        "1287",
        "'VALUES function' is deprecated and will be removed in a future release. Please use "
        "an alias (INSERT INTO ... VALUES (...) AS alias) and replace VALUES(col) in the ON "
        "DUPLICATE KEY UPDATE clause with alias.col instead",
    };
    static const char *const double_warning_rows[] = {
        "Warning",
        "1287",
        "'VALUES function' is deprecated and will be removed in a future release. Please use "
        "an alias (INSERT INTO ... VALUES (...) AS alias) and replace VALUES(col) in the ON "
        "DUPLICATE KEY UPDATE clause with alias.col instead",
        "Warning",
        "1287",
        "'VALUES function' is deprecated and will be removed in a future release. Please use "
        "an alias (INSERT INTO ... VALUES (...) AS alias) and replace VALUES(col) in the ON "
        "DUPLICATE KEY UPDATE clause with alias.col instead",
    };
    static const char *const changed_rows[] = {"1", "20", "200"};
    static const char *const mixed_rows[] = {"1", "40", "400", "2", "30", "300"};
    static const char *const no_key_rows[] = {"1", "10"};
    static const char *const default_rows[] = {"1", "7", NULL};
    static const char *const count_zero[] = {"0"};
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE t(id INT PRIMARY KEY, v INT, n INT)");
    failures += expect_statement_ok(database, "CREATE TABLE s(id INT, v INT, n INT)");
    failures += expect_statement_ok(database, "INSERT INTO t VALUES (1, 10, 100)");
    failures += expect_statement_ok(database, "INSERT INTO s VALUES (1, 20, 200)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO t SELECT id, v, n FROM s "
        "ON DUPLICATE KEY UPDATE v = VALUES(v), n = VALUES(n)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 2U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = double_warning_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "changed duplicate warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, n FROM t",
            .values = changed_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "changed duplicate rows",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO t SELECT id, v, n FROM s ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = warning_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "no-op duplicate warning",
        }
    );

    failures += expect_statement_ok(database, "DELETE FROM s");
    failures += expect_statement_ok(database, "INSERT INTO s VALUES (1, 40, 400), (2, 30, 300)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO t SELECT id, v, n FROM s ORDER BY v "
        "ON DUPLICATE KEY UPDATE v = VALUES(v), n = VALUES(n)",
        (struct expected_dml){.affected_rows = 3, .warning_count = 2U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, n FROM t ORDER BY id",
            .values = mixed_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "mixed insert update rows",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE no_key(id INT, v INT)");
    failures += expect_statement_ok(database, "CREATE TABLE no_key_source(id INT, v INT)");
    failures += expect_statement_ok(database, "INSERT INTO no_key_source VALUES (1, 10)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO no_key SELECT id, v FROM no_key_source "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM no_key",
            .values = no_key_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "no-key insert rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE default_t(id INT PRIMARY KEY, v INT NOT NULL DEFAULT 7, n INT NULL)"
    );
    failures += expect_statement_ok(database, "CREATE TABLE default_s(id INT, v INT, n INT)");
    failures += expect_statement_ok(database, "INSERT INTO default_t VALUES (1, 10, 20)");
    failures += expect_statement_ok(database, "INSERT INTO default_s VALUES (1, 99, 88)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO default_t SELECT id, v, n FROM default_s "
        "ON DUPLICATE KEY UPDATE v = DEFAULT, n = NULL",
        (struct expected_dml){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, n FROM default_t",
            .values = default_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "default null duplicate rows",
        }
    );

    failures += expect_dml_ok(
        database,
        "INSERT INTO default_t SELECT id, v, n FROM default_s WHERE id = 99 "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM default_t WHERE id = 99",
            .values = count_zero,
            .column_count = 1U,
            .row_count = 1U,
            .context = "zero-row duplicate count",
        }
    );

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures +=
        expect_bytes(actual_preamble, expected_preamble, sizeof(expected_preamble), "preamble");
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, n FROM t ORDER BY id",
            .values = mixed_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "reopened updated rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_insert_select_duplicate_source_shapes(void) {
    static const char *const row_scalar_rows[] = {"1", "20"};
    static const char *const union_rows[] = {"1", "20", "2", "30"};
    static const char *const same_table_rows[] = {"1", "10", "2", "20"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "source_shapes") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open source shape file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");

    failures += expect_statement_ok(database, "CREATE TABLE row_scalar(id INT PRIMARY KEY, v INT)");
    failures += expect_statement_ok(database, "INSERT INTO row_scalar VALUES (1, 10)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO row_scalar SELECT 1, 20 ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM row_scalar",
            .values = row_scalar_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "row-scalar duplicate rows",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE union_t(id INT PRIMARY KEY, v INT)");
    failures += expect_statement_ok(database, "CREATE TABLE union_s(id INT, v INT)");
    failures += expect_statement_ok(database, "INSERT INTO union_t VALUES (1, 10)");
    failures += expect_statement_ok(database, "INSERT INTO union_s VALUES (1, 20), (2, 30)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO union_t "
        "SELECT id, v FROM union_s WHERE id = 1 UNION ALL "
        "SELECT id, v FROM union_s WHERE id = 2 "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 3, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM union_t ORDER BY id",
            .values = union_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "union duplicate rows",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE same_t(id INT PRIMARY KEY, v INT)");
    failures += expect_statement_ok(database, "INSERT INTO same_t VALUES (1, 10), (2, 20)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO same_t SELECT id, v FROM same_t ORDER BY id "
        "ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM same_t ORDER BY id",
            .values = same_table_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "same-table materialized rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_insert_select_duplicate_diagnostics(void) {
    static const char *const rollback_rows[] = {"1", "1", "10"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE t(id INT PRIMARY KEY, v INT)");
    failures += expect_statement_ok(database, "CREATE TABLE s(id INT, v INT)");
    failures += expect_statement_ok(database, "INSERT INTO t VALUES (1, 10)");
    failures += expect_statement_ok(database, "INSERT INTO s VALUES (1, 20)");
    failures += execute_error(
        database,
        "INSERT INTO t SELECT id, v FROM s ON DUPLICATE KEY UPDATE missing = VALUES(v)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing'",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t SELECT id, v FROM s ON DUPLICATE KEY UPDATE v = VALUES(missing)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing'",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE nn_t(id INT PRIMARY KEY, v INT NOT NULL)");
    failures += expect_statement_ok(database, "CREATE TABLE nn_s(id INT, v INT)");
    failures += expect_statement_ok(database, "INSERT INTO nn_t VALUES (1, 10)");
    failures += expect_statement_ok(database, "INSERT INTO nn_s VALUES (1, NULL)");
    failures += execute_error(
        database,
        "INSERT INTO nn_t SELECT id, v FROM nn_s ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'v' cannot be null",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE range_t(id INT PRIMARY KEY, ti TINYINT NOT NULL, v INT)"
    );
    failures += expect_statement_ok(database, "CREATE TABLE range_s(id INT, ti TINYINT, v INT)");
    failures += expect_statement_ok(database, "INSERT INTO range_t VALUES (1, 1, 10)");
    failures += expect_statement_ok(database, "INSERT INTO range_s VALUES (1, 2, 20), (2, 3, 30)");
    failures += execute_error(
        database,
        "INSERT INTO range_t SELECT id, ti, v FROM range_s ORDER BY id "
        "ON DUPLICATE KEY UPDATE ti = 128",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'ti' at row 1",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, ti, v FROM range_t ORDER BY id",
            .values = rollback_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "range error rollback rows",
        }
    );

    failures += execute_error(
        database,
        "INSERT IGNORE INTO t SELECT id, v FROM s ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INSERT IGNORE ... ON DUPLICATE KEY UPDATE is not supported",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE auto_t(id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_ok(database, "CREATE TABLE auto_s(v INT)");
    failures += expect_statement_ok(database, "INSERT INTO auto_s VALUES (10)");
    failures += execute_error(
        database,
        "INSERT INTO auto_t(v) SELECT v FROM auto_s ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "INSERT ... SELECT ... ON DUPLICATE KEY UPDATE does not support AUTO_INCREMENT "
                "targets",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_insert_select_duplicate_independent_handles(void) {
    static const char *const first_rows[] = {"1", "20"};
    static const char *const second_rows[] = {"1", "10"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first independent");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second independent");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t(id INT PRIMARY KEY, v INT)");
    failures += expect_statement_ok(first, "CREATE TABLE s(id INT, v INT)");
    failures += expect_statement_ok(second, "CREATE TABLE t(id INT PRIMARY KEY, v INT)");
    failures += expect_statement_ok(second, "CREATE TABLE s(id INT, v INT)");
    failures += expect_statement_ok(first, "INSERT INTO t VALUES (1, 10)");
    failures += expect_statement_ok(first, "INSERT INTO s VALUES (1, 20)");
    failures += expect_statement_ok(second, "INSERT INTO t VALUES (1, 10)");
    failures += expect_statement_ok(second, "INSERT INTO s VALUES (1, 20)");
    failures += expect_dml_ok(
        first,
        "INSERT INTO t SELECT id, v FROM s ON DUPLICATE KEY UPDATE v = VALUES(v)",
        (struct expected_dml){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, v FROM t",
            .values = first_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "first independent rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, v FROM t",
            .values = second_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second independent rows",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc == MYLITE_OK) {
        *out_result = result;
    } else {
        mylite_result_free(result);
        *out_result = NULL;
    }
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, struct expected_dml expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
    failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);
    size_t value_index = 0U;

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
            ++value_index;
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

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s: expected NULL at %zu,%zu, got [%s]\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
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
        "%s/mylite_insert_select_on_duplicate_key_update_%d_%s.mylite",
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
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return;
    }
    (void)remove(buffer);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 || fread(buffer, 1U, size, file) != size) {
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
