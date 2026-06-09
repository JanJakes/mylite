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
    test_path_suffix_capacity = 16,
    mysql_error_parse = 1064,
    mysql_error_duplicate_key = 1062,
    mysql_error_wrong_usage = 1221,
    mysql_error_algorithm_not_supported = 1845,
    mysql_error_algorithm_not_supported_reason = 1846,
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

static int test_algorithm_lock_success_persistence_and_preamble(void);
static int test_algorithm_lock_diagnostics(void);
static int test_algorithm_lock_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int execute_utility_noop(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
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

    failures += test_algorithm_lock_success_persistence_and_preamble();
    failures += test_algorithm_lock_diagnostics();
    failures += test_algorithm_lock_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_algorithm_lock_success_persistence_and_preamble(void) {
    static const char *const add_col_rows[] = {"1", "10", "2", NULL};
    static const char *const duplicate_pk_rows[] = {"2"};
    static const char *const drop_col_rows[] = {"1"};
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
    failures += execute_statement_ok(database, "CREATE DATABASE app", 1);
    failures += execute_statement_ok(database, "USE app", 0);

    failures += execute_statement_ok(database, "CREATE TABLE add_col (id INT)", 0);
    failures += execute_statement_ok(
        database,
        "ALTER TABLE add_col ADD COLUMN v INT, ALGORITHM=INSTANT, LOCK=DEFAULT",
        0
    );
    failures += execute_statement_ok(database, "INSERT INTO add_col VALUES (1, 10), (2, NULL)", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM add_col ORDER BY id",
            .values = add_col_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "add column option rows",
        }
    );

    failures +=
        execute_statement_ok(database, "CREATE TABLE idx (id INT, a INT, b INT, KEY k_b (b))", 0);
    failures += execute_statement_ok(
        database,
        "ALTER TABLE idx DROP INDEX k_b, ALGORITHM=INPLACE, LOCK=NONE",
        0
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE idx ADD INDEX k_a (a), ALGORITHM=INPLACE, LOCK=NONE",
        0
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE idx RENAME INDEX k_a TO k_renamed, LOCK=NONE",
        0
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE idx DROP INDEX k_renamed, ALGORITHM=INPLACE, LOCK=NONE",
        0
    );

    failures += execute_statement_ok(database, "CREATE TABLE p (id INT PRIMARY KEY)", 0);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE c (id INT, pid INT, CONSTRAINT fk_c_p FOREIGN KEY (pid) REFERENCES p (id))",
        0
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE c DROP FOREIGN KEY fk_c_p, ALGORITHM=INPLACE, LOCK=NONE",
        0
    );
    failures += execute_statement_ok(database, "INSERT INTO c VALUES (1, 99)", 1);

    failures += execute_statement_ok(database, "CREATE TABLE force_t (id INT, v INT)", 0);
    failures += execute_statement_ok(database, "INSERT INTO force_t VALUES (1, 10), (2, 20)", 2);
    failures += execute_statement_ok(database, "ALTER TABLE force_t FORCE, ALGORITHM=COPY", 0);

    failures += execute_statement_ok(database, "CREATE TABLE pk_t (id INT PRIMARY KEY, v INT)", 0);
    failures += execute_statement_ok(database, "INSERT INTO pk_t VALUES (1, 10), (2, 20)", 2);
    failures += execute_statement_ok(
        database,
        "ALTER TABLE pk_t DROP PRIMARY KEY, ALGORITHM=COPY, LOCK=EXCLUSIVE",
        2
    );
    failures += execute_statement_ok(database, "INSERT INTO pk_t VALUES (1, 30)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM pk_t WHERE id = 1",
            .values = duplicate_pk_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "drop primary key option permits duplicate key",
        }
    );

    failures += execute_statement_ok(database, "CREATE TABLE drop_col (id INT, v INT)", 0);
    failures += execute_statement_ok(database, "INSERT INTO drop_col VALUES (1, 10)", 1);
    failures += expect_int(
        read_file_at(path, 0L, expected_preamble, sizeof(expected_preamble)),
        0,
        "read preamble before option rebuild"
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE drop_col DROP COLUMN v, ALGORITHM=DEFAULT, LOCK=NONE",
        0
    );
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read preamble after option rebuild"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "preamble after option rebuild"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM drop_col",
            .values = drop_col_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "drop column option row",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += execute_statement_ok(database, "USE app", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM drop_col",
            .values = drop_col_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "reopened drop column option row",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_algorithm_lock_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_statement_ok(database, "CREATE DATABASE app", 1);
    failures += execute_statement_ok(database, "USE app", 0);
    failures += execute_statement_ok(database, "CREATE TABLE bad_lock (id INT, v INT)", 0);
    failures += execute_error(
        database,
        "ALTER TABLE bad_lock DROP COLUMN v, ALGORITHM=INSTANT, LOCK=NONE",
        (struct expected_sql_error){
            .code = mysql_error_wrong_usage,
            .sqlstate = "HY000",
            .message_part = "Incorrect usage of ALGORITHM=INSTANT",
        }
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE bad_algorithm (id INT, v INT, KEY k_v (v))",
        0
    );
    failures += execute_error(
        database,
        "ALTER TABLE bad_algorithm DROP INDEX k_v, ALGORITHM=INSTANT",
        (struct expected_sql_error){
            .code = mysql_error_algorithm_not_supported,
            .sqlstate = "0A000",
            .message_part = "ALGORITHM=INSTANT is not supported for this operation",
        }
    );
    failures += execute_statement_ok(database, "CREATE TABLE parent (id INT PRIMARY KEY)", 0);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE child (id INT PRIMARY KEY, pid INT, KEY k_pid (pid))",
        0
    );
    failures += execute_error(
        database,
        "ALTER TABLE child ADD FOREIGN KEY (pid) REFERENCES parent (id), ALGORITHM=INPLACE",
        (struct expected_sql_error){
            .code = mysql_error_algorithm_not_supported_reason,
            .sqlstate = "0A000",
            .message_part = "Adding foreign keys needs foreign_key_checks=OFF",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE child ADD FOREIGN KEY (pid) REFERENCES parent (id), LOCK=NONE",
        (struct expected_sql_error){
            .code = mysql_error_algorithm_not_supported_reason,
            .sqlstate = "0A000",
            .message_part = "Adding foreign keys needs foreign_key_checks=OFF",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE child ADD FOREIGN KEY (pid) REFERENCES parent (id), ALGORITHM=COPY, "
        "LOCK=NONE",
        (struct expected_sql_error){
            .code = mysql_error_algorithm_not_supported_reason,
            .sqlstate = "0A000",
            .message_part = "COPY algorithm requires a lock",
        }
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE child ADD FOREIGN KEY (pid) REFERENCES parent (id), ALGORITHM=COPY, "
        "LOCK=SHARED",
        0
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE fulltext_t (id INT, body TEXT, FULLTEXT KEY ft_existing (body))",
        0
    );
    failures += execute_error(
        database,
        "ALTER TABLE fulltext_t ADD FULLTEXT KEY ft_body (body), LOCK=NONE",
        (struct expected_sql_error){
            .code = mysql_error_algorithm_not_supported_reason,
            .sqlstate = "0A000",
            .message_part = "Fulltext index creation requires a lock",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE fulltext_t ADD FULLTEXT KEY ft_body (body), ALGORITHM=COPY, LOCK=NONE",
        (struct expected_sql_error){
            .code = mysql_error_algorithm_not_supported_reason,
            .sqlstate = "0A000",
            .message_part = "COPY algorithm requires a lock",
        }
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE fulltext_t ADD FULLTEXT KEY ft_body (body), ALGORITHM=INPLACE, LOCK=SHARED",
        0
    );
    failures += execute_error(
        database,
        "ALTER TABLE bad_algorithm DROP INDEX k_v, ALGORITHM=BOGUS",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE bad_algorithm DROP INDEX k_v, ALGORITHM=BOGUS, ALGORITHM=COPY",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE bad_algorithm DROP INDEX k_v, LOCK=BOGUS",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE bad_algorithm DROP INDEX k_v, LOCK=BOGUS, LOCK=NONE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE bad_algorithm DROP INDEX k_v LOCK=NONE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_utility_noop(database, "ALTER TABLE bad_algorithm ALGORITHM=INSTANT");
    failures += execute_utility_noop(database, "ALTER TABLE bad_algorithm LOCK=NONE");
    failures += execute_error(
        database,
        "ALTER TABLE bad_algorithm ALGORITHM=BOGUS",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE bad_algorithm LOCK=BOGUS",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_algorithm_lock_independent_handles(void) {
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += execute_statement_ok(first, "CREATE DATABASE app", 1);
    failures += execute_statement_ok(first, "USE app", 0);
    failures += execute_statement_ok(second, "CREATE DATABASE app", 1);
    failures += execute_statement_ok(second, "USE app", 0);
    failures +=
        execute_statement_ok(first, "CREATE TABLE t (id INT, v INT, UNIQUE KEY u_v (v))", 0);
    failures +=
        execute_statement_ok(second, "CREATE TABLE t (id INT, v INT, UNIQUE KEY u_v (v))", 0);
    failures += execute_statement_ok(first, "INSERT INTO t VALUES (1, 10)", 1);
    failures += execute_statement_ok(second, "INSERT INTO t VALUES (1, 10)", 1);
    failures += execute_statement_ok(first, "ALTER TABLE t DROP INDEX u_v, ALGORITHM=INPLACE", 0);
    failures += execute_statement_ok(first, "INSERT INTO t VALUES (2, 10)", 1);
    failures += execute_error(
        second,
        "INSERT INTO t VALUES (2, 10)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
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

static int execute_statement_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
    failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    mylite_result_free(result);

    return failures;
}

static int execute_utility_noop(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), 0, sql);
    failures += expect_size(mylite_result_warning_count(result), 1U, sql);
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
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
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
        "%s/mylite_alter_table_algorithm_lock_%d_%s.mylite",
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
    char full_path[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(full_path, sizeof(full_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(full_path)) {
        return;
    }
    (void)remove(full_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        perror(path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        perror(path);
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
            "%s: expected '%s', got '%s'\n",
            context,
            expected,
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
