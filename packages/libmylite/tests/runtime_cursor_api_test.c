#include <mylite/mylite.h>

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
    mysql_collation_binary_id = 63,
    mysql_collation_utf8mb4_0900_ai_ci_id = 255,
    mysql_int_display_length = 11,
    mysql_varchar_20_display_length = 80,
};

struct expected_query_scalar_text {
    const char *sql;
    const char *expected;
    const char *context;
};

static int test_cursor_select_streams_rows_and_metadata(void);
static int test_cursor_reuses_finalized_select_statements(void);
static int test_cursor_materializes_information_schema_selects(void);
static int test_cursor_prepare_rejects_unsupported_statements(void);
static int execute_ok(mylite_db *database, const char *sql);
static int expect_query_scalar_text(
    mylite_db *database,
    struct expected_query_scalar_text expected
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_cursor_text(
    const mylite_stmt *stmt,
    size_t column_index,
    const char *expected,
    const char *context
);
static int expect_cursor_null(const mylite_stmt *stmt, size_t column_index, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_uint32(uint32_t actual, uint32_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_true(int condition, const char *context);

int main(void) {
    int failures = 0;

    failures += test_cursor_select_streams_rows_and_metadata();
    failures += test_cursor_reuses_finalized_select_statements();
    failures += test_cursor_materializes_information_schema_selects();
    failures += test_cursor_prepare_rejects_unsupported_statements();

    return failures == 0 ? 0 : 1;
}

static int test_cursor_select_streams_rows_and_metadata(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "stream") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open cursor stream file");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(
        database,
        "CREATE TABLE items (id INT NOT NULL, name VARCHAR(20), note VARCHAR(20))"
    );
    failures += execute_ok(
        database,
        "INSERT INTO items VALUES (1, 'alpha', 'one'), (2, 'beta', NULL), (3, '', '')"
    );

    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT id, name, note FROM items ORDER BY id",
            strlen("SELECT id, name, note FROM items ORDER BY id"),
            &stmt
        ),
        MYLITE_OK,
        "prepare cursor select"
    );
    failures += expect_true(stmt != NULL, "prepare returns statement handle");
    failures += expect_size(mylite_stmt_column_count(stmt), 3U, "cursor column count");
    failures += expect_text(mylite_stmt_column_name(stmt, 0U), "id", "cursor id column name");
    failures += expect_text(mylite_stmt_column_name(stmt, 1U), "name", "cursor name column name");
    failures += expect_text(mylite_stmt_column_name(stmt, 2U), "note", "cursor note column name");
    failures += expect_text(mylite_stmt_column_schema_name(stmt, 0U), "app", "cursor id schema");
    failures += expect_text(mylite_stmt_column_table_name(stmt, 0U), "items", "cursor id table");
    failures += expect_text(
        mylite_stmt_column_origin_schema_name(stmt, 0U),
        "app",
        "cursor id origin schema"
    );
    failures += expect_text(
        mylite_stmt_column_origin_table_name(stmt, 0U),
        "items",
        "cursor id origin table"
    );
    failures +=
        expect_text(mylite_stmt_column_origin_name(stmt, 0U), "id", "cursor id origin name");
    failures += expect_int(
        mylite_stmt_column_type(stmt, 0U),
        MYLITE_RESULT_COLUMN_TYPE_LONG,
        "cursor id type"
    );
    failures += expect_uint32(
        mylite_stmt_column_flags(stmt, 0U),
        MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT |
            MYLITE_RESULT_COLUMN_FLAG_NUM,
        "cursor id flags"
    );
    failures += expect_uint32(
        mylite_stmt_column_charset_id(stmt, 0U),
        mysql_collation_binary_id,
        "cursor id charset"
    );
    failures += expect_uint32(
        mylite_stmt_column_collation_id(stmt, 0U),
        mysql_collation_binary_id,
        "cursor id collation"
    );
    failures += expect_uint64(
        mylite_stmt_column_display_length(stmt, 0U),
        mysql_int_display_length,
        "cursor id display length"
    );
    failures += expect_int(mylite_stmt_column_nullable(stmt, 0U), 0, "cursor id nullable");
    failures += expect_int(
        mylite_stmt_column_type(stmt, 1U),
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        "cursor name type"
    );
    failures += expect_uint32(
        mylite_stmt_column_collation_id(stmt, 1U),
        mysql_collation_utf8mb4_0900_ai_ci_id,
        "cursor name collation"
    );
    failures += expect_uint64(
        mylite_stmt_column_display_length(stmt, 1U),
        mysql_varchar_20_display_length,
        "cursor name display length"
    );
    failures += expect_int(mylite_stmt_column_nullable(stmt, 1U), 1, "cursor name nullable");
    failures += expect_int(mylite_stmt_column_decimals(stmt, 1U), 0, "cursor name decimals");
    failures +=
        expect_true(mylite_stmt_column_name(stmt, 3U) == NULL, "cursor out-of-range column name");
    failures +=
        expect_true(mylite_stmt_value_text(stmt, 0U) == NULL, "cursor value before first row");

    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "cursor first row step");
    failures += expect_cursor_text(stmt, 0U, "1", "cursor first row id");
    failures += expect_cursor_text(stmt, 1U, "alpha", "cursor first row name");
    failures += expect_cursor_text(stmt, 2U, "one", "cursor first row note");

    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "cursor second row step");
    failures += expect_cursor_text(stmt, 0U, "2", "cursor second row id");
    failures += expect_cursor_text(stmt, 1U, "beta", "cursor second row name");
    failures += expect_cursor_null(stmt, 2U, "cursor second row null note");

    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "cursor third row step");
    failures += expect_cursor_text(stmt, 0U, "3", "cursor third row id");
    failures += expect_cursor_text(stmt, 1U, "", "cursor third row empty name");
    failures += expect_cursor_text(stmt, 2U, "", "cursor third row empty note");

    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "cursor done step");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "cursor repeated done step");
    failures += expect_size(mylite_stmt_column_count(stmt), 3U, "cursor metadata after done");
    failures += expect_true(mylite_stmt_value_text(stmt, 0U) == NULL, "cursor value after done");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize cursor");
    stmt = NULL;
    failures += expect_int(mylite_stmt_finalize(NULL), MYLITE_OK, "finalize null cursor");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_cursor_reuses_finalized_select_statements(void) {
    char path[test_path_capacity];
    const char query[] = "SELECT name FROM items WHERE id = 1";
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "reuse") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open cursor reuse file");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE items (id INT NOT NULL, name VARCHAR(20))");
    failures += execute_ok(database, "INSERT INTO items VALUES (1, 'alpha')");

    failures += expect_int(
        mylite_prepare(database, query, strlen(query), &stmt),
        MYLITE_OK,
        "prepare first cached cursor"
    );
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "first cached cursor row");
    failures += expect_cursor_text(stmt, 0U, "alpha", "first cached cursor value");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "first cached cursor done");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize first cached cursor");
    stmt = NULL;

    failures += expect_int(
        mylite_prepare(database, query, strlen(query), &stmt),
        MYLITE_OK,
        "prepare reused cached cursor"
    );
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "reused cached cursor row");
    failures += expect_cursor_text(stmt, 0U, "alpha", "reused cached cursor value");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize reused cached cursor");
    stmt = NULL;

    failures += execute_ok(database, "ALTER TABLE items ADD COLUMN marker INT NULL");
    failures += execute_ok(database, "UPDATE items SET name = 'beta', marker = 2 WHERE id = 1");
    failures += expect_int(
        mylite_prepare(database, query, strlen(query), &stmt),
        MYLITE_OK,
        "prepare cursor after schema change"
    );
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "schema changed cursor row");
    failures += expect_cursor_text(stmt, 0U, "beta", "schema changed cursor value");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize schema changed cursor");
    stmt = NULL;

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_cursor_materializes_information_schema_selects(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "information_schema") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open information_schema file");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(
        database,
        "CREATE TABLE items (id INT NOT NULL, name VARCHAR(20), note VARCHAR(20))"
    );
    failures += execute_ok(
        database,
        "INSERT INTO items VALUES (1, 'alpha', 'one'), (2, 'beta', NULL), (3, '', '')"
    );

    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT COLUMN_NAME, DATA_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'items' AND COLUMN_NAME = 'id'",
            strlen("SELECT COLUMN_NAME, DATA_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'items' AND COLUMN_NAME = 'id'"),
            &stmt
        ),
        MYLITE_OK,
        "prepare information_schema cursor select"
    );
    failures +=
        expect_size(mylite_stmt_column_count(stmt), 2U, "information_schema cursor column count");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "information_schema row step");
    failures += expect_cursor_text(stmt, 0U, "id", "information_schema column name");
    failures += expect_cursor_text(stmt, 1U, "int", "information_schema data type");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "information_schema done step");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize information_schema");
    stmt = NULL;

    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT id, 1 AS marker FROM items ORDER BY id LIMIT 1 OFFSET 2",
            strlen("SELECT id, 1 AS marker FROM items ORDER BY id LIMIT 1 OFFSET 2"),
            &stmt
        ),
        MYLITE_OK,
        "prepare materialized cursor limit offset"
    );
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "materialized offset row");
    failures += expect_cursor_text(stmt, 0U, "3", "materialized offset id");
    failures += expect_cursor_text(stmt, 1U, "1", "materialized offset marker");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "materialized offset done step");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize materialized offset");
    stmt = NULL;
    failures += expect_query_scalar_text(
        database,
        (struct expected_query_scalar_text){
            .sql = "SELECT FOUND_ROWS()",
            .expected = "3",
            .context = "materialized cursor found rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_cursor_prepare_rejects_unsupported_statements(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open cursor unsupported file");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE items (id INT NOT NULL)");

    failures += expect_int(
        mylite_prepare(
            database,
            "INSERT INTO items VALUES (1)",
            strlen("INSERT INTO items VALUES (1)"),
            &stmt
        ),
        MYLITE_ERROR,
        "cursor rejects insert"
    );
    failures += expect_true(stmt == NULL, "failed prepare leaves null statement");
    failures += expect_contains(mylite_errmsg(database), "SELECT", "failed prepare diagnostic");

    failures += expect_int(
        mylite_prepare(database, NULL, 0U, &stmt),
        MYLITE_MISUSE,
        "cursor rejects null SQL"
    );
    failures += expect_true(stmt == NULL, "misuse prepare leaves null statement");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: rc=%d err=%d state=%s msg=%s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
    }
    mylite_result_free(result);
    return expect_int(rc, MYLITE_OK, sql);
}

static int expect_query_scalar_text(
    mylite_db *database,
    struct expected_query_scalar_text expected
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: rc=%d err=%d state=%s msg=%s\n",
            expected.sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
    }
    failures += expect_int(rc, MYLITE_OK, expected.context);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_column_count(result), 1U, expected.context);
        failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
        failures += expect_text(
            mylite_result_value_text(result, 0U, 0U),
            expected.expected,
            expected.context
        );
    }
    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_runtime_cursor_api_%d_%s.mylite",
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
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int expect_cursor_text(
    const mylite_stmt *stmt,
    size_t column_index,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_stmt_value_text(stmt, column_index);
    int failures = 0;

    failures += expect_text(actual, expected, context);
    failures += expect_size(mylite_stmt_value_size(stmt, column_index), strlen(expected), context);
    return failures;
}

static int expect_cursor_null(const mylite_stmt *stmt, size_t column_index, const char *context) {
    int failures = 0;

    failures += expect_true(mylite_stmt_value_text(stmt, column_index) == NULL, context);
    failures += expect_true(mylite_stmt_value_bytes(stmt, column_index) == NULL, context);
    failures += expect_size(mylite_stmt_value_size(stmt, column_index), 0U, context);
    return failures;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(stderr, "%s: expected \"%s\" to contain \"%s\"\n", context, actual, needle);
        return 1;
    }

    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
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

static int expect_uint32(uint32_t actual, uint32_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %u, got %u\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %llu, got %llu\n",
            context,
            (unsigned long long)expected,
            (unsigned long long)actual
        );
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", context, expected, actual);
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
