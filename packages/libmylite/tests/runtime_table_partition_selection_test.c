#include "mylite_test_support.h"

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

static int test_partition_selection_is_base_table_noop(void);
static int execute_ok(mylite_db *database, const char *sql);
static int expect_query_value(
    mylite_db *database,
    const char *sql,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    return test_partition_selection_is_base_table_noop() == 0 ? 0 : 1;
}

static int test_partition_selection_is_base_table_noop(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_make_path(path, sizeof(path), "base_table_noop");
    remove_related_files(path);
    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(
        database,
        "CREATE TABLE sales (id INT NOT NULL, region INT NOT NULL, label VARCHAR(16)) "
        "PARTITION BY RANGE (region) ("
        "PARTITION p0 VALUES LESS THAN (10), "
        "PARTITION p1 VALUES LESS THAN MAXVALUE)"
    );
    failures += execute_ok(database, "INSERT INTO sales VALUES (1, 3, 'west'), (2, 12, 'east')");
    failures += expect_query_value(
        database,
        "SELECT COUNT(*) FROM sales PARTITION (p0)",
        0U,
        0U,
        "2",
        "partition selection does not prune reads"
    );
    failures += execute_ok(database, "UPDATE sales PARTITION (p0) SET label = 'hit' WHERE id = 2");
    failures += expect_query_value(
        database,
        "SELECT label FROM sales WHERE id = 2",
        0U,
        0U,
        "hit",
        "partition selection does not prune updates"
    );
    failures += execute_ok(database, "DELETE FROM sales PARTITION (p1) WHERE id = 1");
    failures += expect_query_value(
        database,
        "SELECT COUNT(*) FROM sales WHERE id = 1",
        0U,
        0U,
        "0",
        "partition selection does not prune deletes"
    );
    failures += execute_ok(database, "INSERT INTO sales PARTITION (p0) VALUES (3, 99, 'far')");
    failures += expect_query_value(
        database,
        "SELECT label FROM sales WHERE id = 3",
        0U,
        0U,
        "far",
        "partition selection does not reject inserts outside partition set"
    );
    failures += execute_ok(database, "REPLACE INTO sales PARTITION (p0) VALUES (4, 5, 'replace')");
    failures += expect_query_value(
        database,
        "SELECT label FROM sales WHERE id = 4",
        0U,
        0U,
        "replace",
        "partition selection does not reject replace rows"
    );
    failures += expect_query_value(
        database,
        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PARTITIONS "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'sales' AND PARTITION_NAME IS NULL",
        0U,
        0U,
        "1",
        "partition selection leaves nonpartitioned metadata placeholder"
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
    failures += mylite_test_expect_size(mylite_result_row_count(result), row + 1U, context);
    failures += mylite_test_expect_size(mylite_result_column_count(result), column + 1U, context);
    if (failures == 0) {
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, row, column),
            expected,
            context
        );
    }
    mylite_result_free(result);
    return failures;
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
