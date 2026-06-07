#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include <stdbool.h>
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
    sql_capacity = 128,
    suffix_capacity = 16,
    mysql_error_access_denied = 1044,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_table_exists = 1050,
    mysql_error_duplicate_column = 1060,
    mysql_error_parse = 1064,
    mysql_error_table_does_not_exist = 1146,
};

struct expected_statement {
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

static int test_temporary_create_table_select_success_and_metadata(void);
static int test_temporary_create_table_select_resolution_and_diagnostics(void);
static int test_temporary_create_table_select_source_shadowing_and_lifetime(void);
static int seed_schema(mylite_db *database, const char *name);
static int seed_source_table(mylite_db *database);
static int expect_statement(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_query_row_count(
    mylite_db *database,
    const char *sql,
    size_t expected_row_count,
    const char *context
);
static int expect_query_contains(
    mylite_db *database,
    const char *sql,
    size_t row,
    size_t column,
    const char *needle,
    const char *context
);
static int expect_error(
    mylite_db *database,
    const char *sql,
    int expected_code,
    const char *message_part
);
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
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
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

    failures += test_temporary_create_table_select_success_and_metadata();
    failures += test_temporary_create_table_select_resolution_and_diagnostics();
    failures += test_temporary_create_table_select_source_shadowing_and_lifetime();

    return failures == 0 ? 0 : 1;
}

static int test_temporary_create_table_select_success_and_metadata(void) {
    static const char *const copied_rows[] = {"3", "30", "300", "3000"};
    static const char *const row_count_one[] = {"1"};
    static const char *const zero_count[] = {"0"};
    static const char *const shadow_temp_rows[] = {"1"};
    static const char *const shadow_persistent_rows[] = {"99"};
    static const char *const memory_rows[] = {"1", "10"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation_before = 0U;
    uint64_t sqlite_generation_before = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += seed_schema(database, "app");
    failures += seed_schema(database, "other");
    failures += seed_source_table(database);

    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        catalog_generation_before = catalog->generation;
    }
    if (session != NULL) {
        sqlite_generation_before = session->sqlite_schema_generation;
    }

    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE other.tmp_as AS "
        "SELECT id, n AS nullable_alias, b, iu "
        "FROM app.src WHERE id >= 2 ORDER BY id DESC LIMIT 1",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .values = row_count_one,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary ctas row count function",
        }
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            catalog_generation_before,
            "temporary ctas does not mutate durable catalog generation"
        );
    }
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before + 1U,
            "temporary ctas increments SQLite schema generation"
        );
    }
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nullable_alias, b, iu FROM other.tmp_as",
            .values = copied_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "temporary ctas copied rows",
        }
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE other.tmp_as",
        0U,
        1U,
        "CREATE TEMPORARY TABLE `tmp_as`",
        "show create renders temporary ctas target"
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE other.tmp_as",
        0U,
        1U,
        "`nullable_alias` int DEFAULT NULL",
        "temporary ctas alias appears in show create"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'other' AND TABLE_NAME = 'tmp_as'",
            .values = zero_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary ctas omitted from information schema",
        }
    );
    failures += expect_query_row_count(
        database,
        "SHOW TABLE STATUS FROM other LIKE 'tmp_as'",
        0U,
        "temporary ctas omitted from show table status"
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE app.tmp_memory ENGINE=MEMORY "
        "SELECT id, n FROM app.src WHERE id = 1",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n FROM app.tmp_memory",
            .values = memory_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "temporary ctas table options copied rows",
        }
    );
    failures += expect_statement(
        database,
        "CREATE TABLE app.shadow_target(id INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO app.shadow_target VALUES (99)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE app.shadow_target AS SELECT id FROM app.src WHERE id = 1",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM app.shadow_target",
            .values = shadow_temp_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary ctas target shadows persistent target",
        }
    );
    failures += expect_statement(
        database,
        "DROP TEMPORARY TABLE app.shadow_target",
        (struct expected_statement){0, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM app.shadow_target",
            .values = shadow_persistent_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "persistent target visible after temporary drop",
        }
    );
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read preamble after temporary ctas"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "temporary ctas preserves preamble"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_temporary_create_table_select_resolution_and_diagnostics(void) {
    static const char *const no_as_rows[] = {"1", "10", "2", NULL};
    static const char *const empty_count[] = {"0"};
    static const char *const if_not_exists_rows[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += seed_schema(database, "app");
    failures += seed_source_table(database);

    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE app.tmp_no_as SELECT id, n FROM app.src ORDER BY id LIMIT 2",
        (struct expected_statement){2, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n FROM app.tmp_no_as ORDER BY id",
            .values = no_as_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "temporary ctas without as copied rows",
        }
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE app.tmp_empty AS SELECT id FROM app.src WHERE id = 999",
        (struct expected_statement){0, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM app.tmp_empty",
            .values = empty_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary ctas zero-row source",
        }
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE app.tmp_if AS SELECT id FROM app.src WHERE id = 1",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE IF NOT EXISTS app.tmp_if AS "
        "SELECT id FROM app.src WHERE id = 2",
        (struct expected_statement){0, 1U}
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE IF NOT EXISTS app.tmp_if AS SELECT id, id FROM app.src",
        (struct expected_statement){0, 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM app.tmp_if",
            .values = if_not_exists_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary ctas if not exists preserves existing rows",
        }
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE app.tmp_if AS SELECT id FROM app.src",
        mysql_error_table_exists,
        "Table 'tmp_if' already exists"
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE IF NOT EXISTS app.tmp_if AS SELECT id FROM app.missing_source",
        mysql_error_table_does_not_exist,
        "Table 'app.missing_source' doesn't exist"
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE no_default AS SELECT id FROM app.src",
        mysql_error_no_database_selected,
        "No database selected"
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE app.no_default_source AS SELECT id FROM src",
        mysql_error_no_database_selected,
        "No database selected"
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE nosuch_target.dst AS SELECT id FROM nosuch_source.src",
        mysql_error_unknown_database,
        "Unknown database 'nosuch_source'"
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE nosuch_target.dst AS SELECT id FROM app.src",
        mysql_error_unknown_database,
        "Unknown database 'nosuch_target'"
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE information_schema.tmp AS SELECT id FROM app.missing_source",
        mysql_error_access_denied,
        "Access denied for user"
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE app.duplicate_columns AS SELECT id, id FROM app.src",
        mysql_error_duplicate_column,
        "Duplicate column name 'id'"
    );
    failures += expect_statement(database, "USE app", (struct expected_statement){0, 0U});
    failures += expect_statement(database, "START TRANSACTION", (struct expected_statement){0, 0U});
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE tx_tmp AS SELECT id FROM src",
        (struct expected_statement){3, 0U}
    );
    failures += expect_statement(database, "ROLLBACK", (struct expected_statement){0, 0U});
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM tx_tmp",
            .values = empty_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary ctas survives rollback without copied rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_temporary_create_table_select_source_shadowing_and_lifetime(void) {
    static const char *const shadow_rows[] = {"4"};
    static const char *const source_count[] = {"3"};
    char path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "lifetime") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &first), MYLITE_OK, "open first lifetime handle");
    failures += seed_schema(first, "app");
    failures += seed_source_table(first);
    failures += expect_statement(first, "USE app", (struct expected_statement){0, 0U});
    failures += expect_statement(
        first,
        "CREATE TABLE shadow_src(id INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        first,
        "INSERT INTO shadow_src VALUES (9)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        first,
        "CREATE TEMPORARY TABLE shadow_src(id INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        first,
        "INSERT INTO shadow_src VALUES (4)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        first,
        "CREATE TEMPORARY TABLE shadow_copy AS SELECT id FROM shadow_src",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id FROM shadow_copy",
            .values = shadow_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary ctas reads temporary source shadow",
        }
    );
    failures += expect_int(mylite_open(path, &second), MYLITE_OK, "open second lifetime handle");
    failures += expect_statement(second, "USE app", (struct expected_statement){0, 0U});
    failures += expect_error(
        second,
        "SELECT id FROM shadow_copy",
        mysql_error_table_does_not_exist,
        "Table 'app.shadow_copy' doesn't exist"
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM src",
            .values = source_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "persistent source visible in independent handle",
        }
    );
    mylite_close(first);
    first = NULL;
    failures += expect_error(
        second,
        "SELECT id FROM shadow_copy",
        mysql_error_table_does_not_exist,
        "Table 'app.shadow_copy' doesn't exist"
    );
    mylite_close(second);

    failures += expect_int(mylite_open(path, &first), MYLITE_OK, "reopen lifetime file");
    failures += expect_statement(first, "USE app", (struct expected_statement){0, 0U});
    failures += expect_error(
        first,
        "SELECT id FROM shadow_copy",
        mysql_error_table_does_not_exist,
        "Table 'app.shadow_copy' doesn't exist"
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM src",
            .values = source_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "persistent source survives reopen",
        }
    );

    mylite_close(first);
    remove_related_files(path);
    return failures;
}

static int seed_schema(mylite_db *database, const char *name) {
    char sql[sql_capacity];
    int written = snprintf(sql, sizeof(sql), "CREATE DATABASE %s", name);

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "schema name too long: %s\n", name);
        return 1;
    }
    return expect_statement(database, sql, (struct expected_statement){1, 0U});
}

static int seed_source_table(mylite_db *database) {
    int failures = 0;

    failures += expect_statement(
        database,
        "CREATE TABLE app.src("
        "id INT NOT NULL DEFAULT 7, "
        "n INT NULL DEFAULT NULL, "
        "b BIGINT NOT NULL, "
        "iu INT UNSIGNED NOT NULL DEFAULT 6)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO app.src(id, n, b, iu) VALUES "
        "(1, 10, 100, 1000), "
        "(2, NULL, 200, 2000), "
        "(3, 30, 300, 3000)",
        (struct expected_statement){3, 0U}
    );
    return failures;
}

static int expect_statement(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_column_count(result), 0U, "statement columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "statement rows");
        failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    } else {
        fprintf(stderr, "%s failed: %s\n", sql, mylite_errmsg(database));
    }
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
        failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                size_t value_index = (row * query.column_count) + column;

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

static int expect_query_row_count(
    mylite_db *database,
    const char *sql,
    size_t expected_row_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_OK, context);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_row_count(result), expected_row_count, context);
        failures += expect_size(mylite_result_warning_count(result), 0U, context);
    } else {
        fprintf(stderr, "%s failed: %s\n", sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_contains(
    mylite_db *database,
    const char *sql,
    size_t row,
    size_t column,
    const char *needle,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_OK, context);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_row_count(result) > row ? 1U : 0U, 1U, context);
        failures += expect_size(mylite_result_column_count(result) > column ? 1U : 0U, 1U, context);
        if (mylite_result_row_count(result) > row && mylite_result_column_count(result) > column) {
            failures +=
                expect_contains(mylite_result_value_text(result, row, column), needle, context);
        }
    } else {
        fprintf(stderr, "%s failed: %s\n", sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_error(
    mylite_db *database,
    const char *sql,
    int expected_code,
    const char *message_part
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected_code, sql);
    failures += expect_size((size_t)(result != NULL), 0U, "error result");
    if (message_part != NULL) {
        failures += expect_contains(mylite_errmsg(database), message_part, sql);
    }
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
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_temporary_create_table_select_%s_%d.mylite",
        name,
        current_process_id()
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
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return;
    }
    (void)remove(buffer);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        return -1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }
    read_count = fread(buffer, 1U, size, file);
    fclose(file);
    return read_count == size ? 0 : -1;
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

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected %llu, got %llu\n",
        context,
        (unsigned long long)expected,
        (unsigned long long)actual
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

static int expect_true(int condition, const char *context) {
    if (condition) {
        return 0;
    }

    fprintf(stderr, "%s: expected true\n", context);
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
        expected == NULL ? "(null)" : expected,
        actual == NULL ? "(null)" : actual
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
        actual == NULL ? "(null)" : actual,
        needle == NULL ? "(null)" : needle
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

    fprintf(stderr, "%s: bytes differ\n", context);
    return 1;
}
