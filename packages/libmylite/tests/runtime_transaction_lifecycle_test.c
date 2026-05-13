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
    mysql_error_duplicate_key = 1062,
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_transaction_control_and_dml(void);
static int test_file_close_rolls_back_transaction(void);
static int seed_schema(mylite_db *database);
static int expect_nonquery(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_error(mylite_db *database, const char *sql, int expected_code);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_row_count_zero(mylite_db *database);
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
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_transaction_control_and_dml();
    failures += test_file_close_rolls_back_transaction();

    return failures == 0 ? 0 : 1;
}

static int test_transaction_control_and_dml(void) {
    static const char *const one_committed[] = {"1", "10"};
    static const char *const nested_rows[] = {"10", "100"};
    static const char *const ddl_rows[] = {"10", "100", "30", "300"};
    static const char *const unique_rows[] = {"1", "2", "3"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "control") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open control file");
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE t (id INT PRIMARY KEY, v INT)", 0);

    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_row_count_zero(database);
    failures += expect_nonquery(database, "ROLLBACK WORK", 0);
    failures += expect_row_count_zero(database);

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_row_count_zero(database);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (1, 10)", 1);
    failures += expect_nonquery(database, "COMMIT WORK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = one_committed,
            .column_count = 2U,
            .row_count = 1U,
            .context = "committed insert",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (2, 20)", 1);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = one_committed,
            .column_count = 2U,
            .row_count = 1U,
            .context = "rolled back insert",
        }
    );

    failures += expect_nonquery(database, "BEGIN WORK", 0);
    failures += expect_nonquery(database, "UPDATE t SET v = 11 WHERE id = 1", 1);
    failures += expect_nonquery(database, "ROLLBACK WORK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = one_committed,
            .column_count = 2U,
            .row_count = 1U,
            .context = "rolled back update",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "DELETE FROM t WHERE id = 1", 1);
    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY id",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "committed delete",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (10, 100)", 1);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (20, 200)", 1);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = nested_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "nested START TRANSACTION commits previous transaction",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (30, 300)", 1);
    failures += expect_nonquery(database, "CREATE TABLE ddl_commit (id INT)", 0);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = ddl_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "DDL implicitly commits active transaction",
        }
    );
    failures += expect_nonquery(database, "INSERT INTO ddl_commit VALUES (1)", 1);

    failures += expect_nonquery(database, "CREATE TABLE unique_t (id INT PRIMARY KEY)", 0);
    failures += expect_nonquery(database, "INSERT INTO unique_t VALUES (1)", 1);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO unique_t VALUES (2)", 1);
    failures +=
        expect_error(database, "INSERT INTO unique_t VALUES (1)", mysql_error_duplicate_key);
    failures += expect_nonquery(database, "INSERT INTO unique_t VALUES (3)", 1);
    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM unique_t ORDER BY id",
            .values = unique_rows,
            .column_count = 1U,
            .row_count = 3U,
            .context = "statement rollback inside active transaction",
        }
    );

    failures += expect_nonquery(database, "CREATE TABLE paths (id INT, v INT)", 0);
    failures += expect_nonquery(database, "CREATE TABLE src (id INT PRIMARY KEY, v INT)", 0);
    failures += expect_nonquery(database, "INSERT INTO src VALUES (3, 30), (5, 50)", 2);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO paths VALUES (1, 10)", 1);
    failures += expect_nonquery(database, "INSERT INTO paths SET id = 2, v = 20", 1);
    failures +=
        expect_nonquery(database, "INSERT INTO paths SELECT id, v FROM src WHERE id = 3", 1);
    failures += expect_nonquery(database, "REPLACE INTO paths VALUES (2, 22)", 1);
    failures += expect_nonquery(database, "REPLACE INTO paths SET id = 4, v = 40", 1);
    failures +=
        expect_nonquery(database, "REPLACE INTO paths SELECT id, v FROM src WHERE id = 5", 1);
    failures += expect_nonquery(database, "UPDATE paths SET v = 99 WHERE id = 1", 1);
    failures += expect_nonquery(database, "DELETE FROM paths WHERE id = 3", 1);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM paths ORDER BY id",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "all DML paths roll back",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_file_close_rolls_back_transaction(void) {
    static const char *const committed_rows[] = {"1", "10"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "close_rollback") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE persisted (id INT PRIMARY KEY, v INT)", 0);
    failures += expect_nonquery(database, "INSERT INTO persisted VALUES (1, 10)", 1);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO persisted VALUES (2, 20)", 1);
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen file");
    failures += expect_nonquery(database, "USE app", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM persisted ORDER BY id",
            .values = committed_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "close rolls back active transaction",
        }
    );
    mylite_close(database);

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "transaction lifecycle preserves MyLite preamble"
    );

    remove_related_files(path);
    return failures;
}

static int seed_schema(mylite_db *database) {
    int failures = 0;

    failures += expect_nonquery(database, "CREATE DATABASE app", 1);
    failures += expect_nonquery(database, "USE app", 0);
    return failures;
}

static int expect_nonquery(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_column_count(result), 0U, "nonquery column count");
        failures += expect_size(mylite_result_row_count(result), 0U, "nonquery row count");
        failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, "nonquery warning count");
    } else {
        fprintf(stderr, "%s failed: %s\n", sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_error(mylite_db *database, const char *sql, int expected_code) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected_code, "diagnostic code");
    failures += expect_size((size_t)(result != NULL), 0U, "error result");
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);

    failures += expect_int(rc, MYLITE_OK, query.context);
    if (rc == MYLITE_OK) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                const size_t value_index = (row * query.column_count) + column;

                failures += expect_result_value(
                    result,
                    row,
                    column,
                    query.values == NULL ? NULL : query.values[value_index],
                    query.context
                );
            }
        }
    } else {
        fprintf(stderr, "%s failed: %s\n", query.sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_row_count_zero(mylite_db *database) {
    static const char *const values[] = {"0"};

    return expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .values = values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "transaction ROW_COUNT()",
        }
    );
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
        return expect_size((size_t)(actual != NULL), 0U, context);
    }
    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_transaction_lifecycle_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path truncated\n");
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
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-wal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    (void)remove(related);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    return bytes_read == size ? 0 : 1;
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
    if (actual != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", context, expected, actual);
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

    fprintf(stderr, "%s: bytes differ\n", context);
    return 1;
}
