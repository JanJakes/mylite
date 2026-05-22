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
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_duplicate_key_name = 1061,
    mysql_error_duplicate_key = 1062,
    mysql_error_parse = 1064,
    mysql_error_key_column_missing = 1072,
    mysql_error_cant_drop_field_or_key = 1091,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_temporary_fulltext_index = 1796,
};

struct expected_statement {
    int64_t affected_rows;
    size_t warning_count;
};

struct expected_sql_error {
    int code;
    const char *message_part;
};

static int test_temporary_create_drop_index_metadata_and_preamble(void);
static int test_temporary_alter_add_unique_enforcement_and_drop(void);
static int test_temporary_index_shadowing_cleanup_and_handles(void);
static int test_temporary_index_diagnostics(void);
static int seed_schema(mylite_db *database);
static int expect_statement(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
);
static int expect_query_single_text(mylite_db *database, const char *sql, const char *expected);
static int expect_query_contains(mylite_db *database, const char *sql, const char *needle);
static int expect_query_not_contains(mylite_db *database, const char *sql, const char *needle);
static int expect_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int result_contains(const mylite_result *result, const char *needle);
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

    failures += test_temporary_create_drop_index_metadata_and_preamble();
    failures += test_temporary_alter_add_unique_enforcement_and_drop();
    failures += test_temporary_index_shadowing_cleanup_and_handles();
    failures += test_temporary_index_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_temporary_create_drop_index_metadata_and_preamble(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "metadata") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open metadata file");
    failures += seed_schema(database);
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE t ("
        "id INT NOT NULL, v INT, name VARCHAR(10), PRIMARY KEY(id))",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO t VALUES (1,10,'a'),(2,20,'b'),(3,NULL,'c')",
        (struct expected_statement){3, 0U}
    );
    failures +=
        expect_statement(database, "CREATE INDEX k_v ON t(v)", (struct expected_statement){3, 0U});
    failures += expect_query_single_text(database, "SELECT ROW_COUNT()", "3");
    failures += expect_statement(
        database,
        "CREATE UNIQUE INDEX u_name ON t(name)",
        (struct expected_statement){3, 0U}
    );
    failures += expect_query_single_text(database, "SELECT ROW_COUNT()", "3");
    failures +=
        expect_query_contains(database, "SHOW CREATE TABLE t", "CREATE TEMPORARY TABLE `t`");
    failures += expect_query_contains(database, "SHOW CREATE TABLE t", "KEY `k_v` (`v`)");
    failures +=
        expect_query_contains(database, "SHOW CREATE TABLE t", "UNIQUE KEY `u_name` (`name`)");
    failures += expect_query_contains(database, "SHOW INDEX FROM t", "PRIMARY");
    failures += expect_query_contains(database, "SHOW INDEX FROM t", "k_v");
    failures += expect_query_contains(database, "SHOW INDEX FROM t", "u_name");
    failures += expect_query_single_text(
        database,
        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't'",
        "0"
    );
    failures +=
        expect_statement(database, "DROP INDEX k_v ON t", (struct expected_statement){3, 0U});
    failures += expect_query_single_text(database, "SELECT ROW_COUNT()", "3");
    failures += expect_statement(
        database,
        "ALTER TABLE t DROP INDEX u_name",
        (struct expected_statement){3, 0U}
    );
    failures += expect_query_single_text(database, "SELECT ROW_COUNT()", "3");
    failures += expect_query_not_contains(database, "SHOW CREATE TABLE t", "KEY `k_v`");
    failures += expect_query_not_contains(database, "SHOW CREATE TABLE t", "u_name");
    failures += expect_statement(
        database,
        "INSERT INTO t VALUES (4,40,'a')",
        (struct expected_statement){1, 0U}
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
        sizeof(expected_preamble),
        "temporary index DDL preserves MyLite preamble"
    );
    remove_related_files(path);

    return failures;
}

static int test_temporary_alter_add_unique_enforcement_and_drop(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "alter") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open alter file");
    failures += seed_schema(database);
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE t (id INT, v INT, name VARCHAR(10))",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO t VALUES (1,10,'a'),(2,20,'b'),(3,NULL,NULL)",
        (struct expected_statement){3, 0U}
    );
    failures += expect_statement(
        database,
        "ALTER TABLE t ADD INDEX k_v(v)",
        (struct expected_statement){3, 0U}
    );
    failures += expect_query_single_text(database, "SELECT ROW_COUNT()", "3");
    failures += expect_statement(
        database,
        "ALTER TABLE t ADD UNIQUE KEY u_name(name)",
        (struct expected_statement){3, 0U}
    );
    failures += expect_query_single_text(database, "SELECT ROW_COUNT()", "3");
    failures += expect_query_contains(database, "SHOW CREATE TABLE t", "KEY `k_v` (`v`)");
    failures +=
        expect_query_contains(database, "SHOW CREATE TABLE t", "UNIQUE KEY `u_name` (`name`)");
    failures += expect_error(
        database,
        "INSERT INTO t VALUES (4,40,'a')",
        (struct expected_sql_error){mysql_error_duplicate_key, "Duplicate entry"}
    );
    failures +=
        expect_statement(database, "DROP INDEX u_name ON t", (struct expected_statement){3, 0U});
    failures += expect_query_single_text(database, "SELECT ROW_COUNT()", "3");
    failures += expect_statement(
        database,
        "INSERT INTO t VALUES (4,40,'a')",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        database,
        "CREATE UNIQUE INDEX u_v ON t(v)",
        (struct expected_statement){4, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO t VALUES (5,NULL,'z')",
        (struct expected_statement){1, 0U}
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_temporary_index_shadowing_cleanup_and_handles(void) {
    char path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "shadow") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &first), MYLITE_OK, "open first shadow handle");
    failures += seed_schema(first);
    failures += expect_int(mylite_open(path, &second), MYLITE_OK, "open second shadow handle");
    failures += expect_statement(second, "USE app", (struct expected_statement){0, 0U});
    failures += expect_statement(
        first,
        "CREATE TABLE shadowed (id INT, v INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        first,
        "INSERT INTO shadowed VALUES (1,10)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        first,
        "CREATE TEMPORARY TABLE shadowed (id INT, v INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        first,
        "INSERT INTO shadowed VALUES (2,20),(3,30)",
        (struct expected_statement){2, 0U}
    );
    failures += expect_statement(
        first,
        "CREATE INDEX k_v ON shadowed(v)",
        (struct expected_statement){2, 0U}
    );
    failures += expect_query_contains(first, "SHOW CREATE TABLE shadowed", "KEY `k_v` (`v`)");
    failures += expect_query_not_contains(second, "SHOW CREATE TABLE shadowed", "KEY `k_v` (`v`)");
    failures += expect_query_single_text(
        first,
        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'shadowed' AND INDEX_NAME = 'k_v'",
        "0"
    );
    failures += expect_statement(
        first,
        "DROP TEMPORARY TABLE shadowed",
        (struct expected_statement){0, 0U}
    );
    failures += expect_query_not_contains(first, "SHOW CREATE TABLE shadowed", "KEY `k_v` (`v`)");

    mylite_close(second);
    mylite_close(first);

    failures += expect_int(mylite_open(path, &first), MYLITE_OK, "reopen shadow file");
    failures += expect_statement(first, "USE app", (struct expected_statement){0, 0U});
    failures += expect_query_not_contains(first, "SHOW CREATE TABLE shadowed", "KEY `k_v` (`v`)");
    failures += expect_query_single_text(first, "SELECT COUNT(*) FROM shadowed", "1");

    mylite_close(first);
    remove_related_files(path);
    return failures;
}

static int test_temporary_index_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += expect_error(
        database,
        "CREATE INDEX k_v ON no_default(v)",
        (struct expected_sql_error){mysql_error_no_database_selected, "No database selected"}
    );
    failures +=
        expect_statement(database, "CREATE DATABASE app", (struct expected_statement){1, 0U});
    failures += expect_statement(database, "USE app", (struct expected_statement){0, 0U});
    failures += expect_error(
        database,
        "CREATE INDEX k_v ON missing_schema.missing_table(v)",
        (struct expected_sql_error){mysql_error_unknown_database, "Unknown database"}
    );
    failures += expect_error(
        database,
        "CREATE INDEX k_v ON missing_table(v)",
        (struct expected_sql_error){mysql_error_table_does_not_exist, "doesn't exist"}
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE t (id INT, v INT, name VARCHAR(10))",
        (struct expected_statement){0, 0U}
    );
    failures += expect_error(
        database,
        "CREATE INDEX k_missing ON t(missing_col)",
        (struct expected_sql_error){mysql_error_key_column_missing, "doesn't exist in table"}
    );
    failures +=
        expect_statement(database, "CREATE INDEX k_v ON t(v)", (struct expected_statement){0, 0U});
    failures += expect_error(
        database,
        "ALTER TABLE t ADD INDEX k_v(name)",
        (struct expected_sql_error){mysql_error_duplicate_key_name, "Duplicate key name"}
    );
    failures += expect_error(
        database,
        "DROP INDEX missing_idx ON t",
        (struct expected_sql_error){mysql_error_cant_drop_field_or_key, "Can't DROP"}
    );
    failures += expect_error(
        database,
        "CREATE FULLTEXT INDEX ft_name ON t(name)",
        (struct expected_sql_error){
            mysql_error_temporary_fulltext_index,
            "Cannot create FULLTEXT index on temporary InnoDB table"
        }
    );
    failures += expect_error(
        database,
        "CREATE SPATIAL INDEX s_id ON t (id)",
        (struct expected_sql_error){mysql_error_parse, NULL}
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE dup (id INT, v INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO dup VALUES (1,10),(2,10)",
        (struct expected_statement){2, 0U}
    );
    failures += expect_error(
        database,
        "CREATE UNIQUE INDEX u_v ON dup(v)",
        (struct expected_sql_error){mysql_error_duplicate_key, "Duplicate entry"}
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int seed_schema(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(
        database,
        "CREATE DATABASE IF NOT EXISTS app",
        strlen("CREATE DATABASE IF NOT EXISTS app"),
        &result
    );

    failures += expect_int(rc, MYLITE_OK, "CREATE DATABASE IF NOT EXISTS app");
    if (rc != MYLITE_OK) {
        fprintf(stderr, "CREATE DATABASE IF NOT EXISTS app: %s\n", mylite_errmsg(database));
    }
    mylite_result_free(result);
    failures += expect_statement(database, "USE app", (struct expected_statement){0, 0U});
    return failures;
}

static int expect_statement(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    } else {
        fprintf(stderr, "%s: %s\n", sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_single_text(mylite_db *database, const char *sql, const char *expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_column_count(result), 1U, sql);
        failures += expect_size(mylite_result_row_count(result), 1U, sql);
        if (mylite_result_row_count(result) == 1U && mylite_result_column_count(result) == 1U) {
            failures += expect_text(mylite_result_value_text(result, 0U, 0U), expected, sql);
        }
    } else {
        fprintf(stderr, "%s: %s\n", sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_contains(mylite_db *database, const char *sql, const char *needle) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc == MYLITE_OK && !result_contains(result, needle)) {
        fprintf(stderr, "%s: expected result to contain [%s]\n", sql, needle);
        ++failures;
    } else if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_not_contains(mylite_db *database, const char *sql, const char *needle) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc == MYLITE_OK && result_contains(result, needle)) {
        fprintf(stderr, "%s: expected result not to contain [%s]\n", sql, needle);
        ++failures;
    } else if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error %d, statement succeeded\n", sql, expected.code);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    if (expected.message_part != NULL &&
        strstr(mylite_errmsg(database), expected.message_part) == NULL) {
        fprintf(
            stderr,
            "%s: expected error message to contain [%s], got [%s]\n",
            sql,
            expected.message_part,
            mylite_errmsg(database)
        );
        ++failures;
    }
    mylite_result_free(result);
    return failures;
}

static int result_contains(const mylite_result *result, const char *needle) {
    size_t row_count = mylite_result_row_count(result);
    size_t column_count = mylite_result_column_count(result);

    for (size_t row = 0U; row < row_count; ++row) {
        for (size_t column = 0U; column < column_count; ++column) {
            const char *value = mylite_result_value_text(result, row, column);

            if (value != NULL && strstr(value, needle) != NULL) {
                return 1;
            }
        }
    }
    return 0;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_temporary_index_lifecycle_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path too long\n");
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

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return;
    }
    (void)remove(buffer);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_size = 0U;

    if (file == NULL) {
        perror(path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        perror("fseek");
        (void)fclose(file);
        return 1;
    }
    read_size = fread(buffer, 1U, size, file);
    if (fclose(file) != 0) {
        perror("fclose");
        return 1;
    }
    if (read_size != size) {
        fprintf(stderr, "short read from %s\n", path);
        return 1;
    }
    return 0;
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
    if ((actual == NULL && expected == NULL) ||
        (actual != NULL && expected != NULL && strcmp(actual, expected) == 0)) {
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
