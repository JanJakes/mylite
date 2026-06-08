#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
};

static int test_partitioned_create_table_is_base_table(void);
static int execute_ok(mylite_db *database, const char *sql);
static int expect_query_value(
    mylite_db *database,
    const char *sql,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_query_row_count(
    mylite_db *database,
    const char *sql,
    size_t expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);

int main(void) {
    return test_partitioned_create_table_is_base_table() == 0 ? 0 : 1;
}

static int test_partitioned_create_table_is_base_table(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += make_test_path(path, sizeof(path), "base_table");
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(
        database,
        "CREATE TABLE sales (id INT PRIMARY KEY, region INT NOT NULL) "
        "PARTITION BY RANGE (region) ("
        "PARTITION p0 VALUES LESS THAN (10), "
        "PARTITION p1 VALUES LESS THAN MAXVALUE)"
    );
    failures += execute_ok(database, "INSERT INTO sales VALUES (1, 3), (2, 12)");
    failures += expect_query_value(
        database,
        "SELECT COUNT(*) FROM sales",
        0U,
        0U,
        "2",
        "partitioned create table stores rows in base table"
    );
    failures += expect_query_row_count(
        database,
        "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.PARTITIONS "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'sales'",
        1U,
        "partitioned create table has base partitions row"
    );
    failures += expect_query_value(
        database,
        "SELECT PARTITION_NAME FROM INFORMATION_SCHEMA.PARTITIONS "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'sales'",
        0U,
        0U,
        NULL,
        "partition metadata remains nonpartitioned"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
        mylite_result_free(result);
        return 1;
    }
    mylite_result_free(result);
    return 0;
}

static int expect_query_value(
    mylite_db *database,
    const char *sql,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
        mylite_result_free(result);
        return 1;
    }
    failures += expect_size(mylite_result_row_count(result), row + 1U, context);
    failures += expect_size(mylite_result_column_count(result), column + 1U, context);
    if (failures == 0) {
        failures += expect_text(mylite_result_value_text(result, row, column), expected, context);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_row_count(
    mylite_db *database,
    const char *sql,
    size_t expected,
    const char *context
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
        mylite_result_free(result);
        return 1;
    }
    failures += expect_size(mylite_result_row_count(result), expected, context);
    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_create_table_partition_options_%s_%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path for %s\n", name);
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
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written > 0 && (size_t)written < sizeof(related)) {
        remove(related);
    }
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
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
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %s, got %s\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}
