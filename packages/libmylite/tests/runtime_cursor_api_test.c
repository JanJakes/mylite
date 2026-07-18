#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "sqlite3.h"

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
    mysql_error_commands_out_of_sync = 2014,
    sql_text_capacity = 128,
    diagnostic_text_capacity = 256,
};

struct expected_query_scalar_text {
    const char *sql;
    const char *expected;
    const char *context;
};

static int test_cursor_select_streams_rows_and_metadata(void);
static int test_cursor_keeps_borrowed_metadata_across_invalidation(void);
static int test_key_metadata_cache_uses_lru_replacement(void);
static int test_cursor_reuses_finalized_select_statements(void);
static int test_cursor_reset_and_value_nullability(void);
static int test_native_prepared_scalar_bindings(void);
static int test_native_prepared_owns_sql_text(void);
static int test_native_prepared_dml_bindings(void);
static int test_buffered_prepared_statement_releases_connection(void);
static int test_cursor_materializes_information_schema_selects(void);
static int test_cursor_prepare_statement_surface(void);
static int test_cursor_read_transaction_lifecycle(void);
static int test_cursor_connection_close_order(void);
static int test_materialized_cursor_does_not_overwrite_later_statement_state(void);
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
static bool key_metadata_cache_contains_table(const mylite_db *database, int64_t table_id);

int main(void) {
    int failures = 0;

    failures += test_cursor_select_streams_rows_and_metadata();
    failures += test_cursor_keeps_borrowed_metadata_across_invalidation();
    failures += test_key_metadata_cache_uses_lru_replacement();
    failures += test_cursor_reuses_finalized_select_statements();
    failures += test_cursor_reset_and_value_nullability();
    failures += test_native_prepared_scalar_bindings();
    failures += test_native_prepared_owns_sql_text();
    failures += test_native_prepared_dml_bindings();
    failures += test_buffered_prepared_statement_releases_connection();
    failures += test_cursor_materializes_information_schema_selects();
    failures += test_cursor_prepare_statement_surface();
    failures += test_cursor_read_transaction_lifecycle();
    failures += test_cursor_connection_close_order();
    failures += test_materialized_cursor_does_not_overwrite_later_statement_state();

    return failures == 0 ? 0 : 1;
}

static int test_cursor_connection_close_order(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = make_test_path(path, sizeof(path), "connection_close_order");

    if (failures != 0) {
        return failures;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open streaming close-order");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE items (id INT NOT NULL)");
    failures += execute_ok(database, "INSERT INTO items VALUES (1), (2)");
    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT id FROM items WHERE id >= ? ORDER BY id",
            strlen("SELECT id FROM items WHERE id >= ? ORDER BY id"),
            &stmt
        ),
        MYLITE_OK,
        "prepare streaming close-order cursor"
    );
    failures +=
        expect_int(mylite_stmt_bind_int64(stmt, 0U, 1), MYLITE_OK, "bind close-order cursor");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "step before connection close");
    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_MISUSE, "step detached cursor");
    failures += expect_int(mylite_stmt_reset(stmt), MYLITE_MISUSE, "reset detached cursor");
    failures += expect_int(mylite_stmt_bind_null(stmt, 0U), MYLITE_MISUSE, "bind detached cursor");
    failures += expect_int(
        mylite_stmt_clear_bindings(stmt),
        MYLITE_MISUSE,
        "clear detached cursor bindings"
    );
    failures += expect_size(mylite_stmt_parameter_count(stmt), 0U, "detached parameter count");
    failures += expect_true(mylite_stmt_affected_rows(stmt) == -1, "detached affected rows");
    failures += expect_uint64(mylite_stmt_insert_id(stmt), 0U, "detached insert id");
    failures += expect_size(mylite_stmt_column_count(stmt), 0U, "detached cursor metadata");
    failures += expect_true(mylite_stmt_column_name(stmt, 0U) == NULL, "detached column name");
    failures +=
        expect_true(mylite_stmt_column_schema_name(stmt, 0U) == NULL, "detached column schema");
    failures +=
        expect_true(mylite_stmt_column_table_name(stmt, 0U) == NULL, "detached column table");
    failures += expect_true(
        mylite_stmt_column_origin_schema_name(stmt, 0U) == NULL,
        "detached column origin schema"
    );
    failures += expect_true(
        mylite_stmt_column_origin_table_name(stmt, 0U) == NULL,
        "detached column origin table"
    );
    failures += expect_true(
        mylite_stmt_column_origin_name(stmt, 0U) == NULL,
        "detached column origin name"
    );
    failures += expect_int(
        mylite_stmt_column_type(stmt, 0U),
        MYLITE_RESULT_COLUMN_TYPE_UNKNOWN,
        "detached column type"
    );
    failures += expect_uint64(mylite_stmt_column_flags(stmt, 0U), 0U, "detached column flags");
    failures +=
        expect_uint64(mylite_stmt_column_charset_id(stmt, 0U), 0U, "detached column charset");
    failures +=
        expect_uint64(mylite_stmt_column_collation_id(stmt, 0U), 0U, "detached column collation");
    failures +=
        expect_uint64(mylite_stmt_column_display_length(stmt, 0U), 0U, "detached column length");
    failures += expect_int(mylite_stmt_column_decimals(stmt, 0U), 0, "detached decimals");
    failures += expect_int(mylite_stmt_column_nullable(stmt, 0U), 1, "detached nullable");
    failures += expect_true(mylite_stmt_value_is_null(stmt, 0U), "detached value null sentinel");
    failures += expect_true(mylite_stmt_value_text(stmt, 0U) == NULL, "detached value text");
    failures += expect_true(mylite_stmt_value_bytes(stmt, 0U) == NULL, "detached value bytes");
    failures += expect_size(mylite_stmt_value_size(stmt, 0U), 0U, "detached value size");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize detached cursor");
    stmt = NULL;
    remove_related_files(path);

    failures +=
        expect_int(mylite_open(path, &database), MYLITE_OK, "open materialized close-order");
    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES",
            strlen("SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES"),
            &stmt
        ),
        MYLITE_OK,
        "prepare materialized close-order cursor"
    );
    mylite_close(database);
    database = NULL;
    failures +=
        expect_int(mylite_stmt_step(stmt), MYLITE_MISUSE, "step detached materialized cursor");
    failures +=
        expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize detached materialized cursor");
    remove_related_files(path);

    return failures;
}

static int test_materialized_cursor_does_not_overwrite_later_statement_state(void) {
    char path[test_path_capacity];
    char later_error[diagnostic_text_capacity];
    const struct mylite_session_state *session = NULL;
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    mylite_result *result = NULL;
    int later_error_code = MYLITE_OK;
    int failures = make_test_path(path, sizeof(path), "stale_cursor_state");

    if (failures != 0) {
        return failures;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open stale cursor state");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE items (id INT NOT NULL)");
    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'app'",
            strlen("SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'app'"),
            &stmt
        ),
        MYLITE_OK,
        "prepare materialized cursor before later insert"
    );
    failures += execute_ok(database, "INSERT INTO items VALUES (1)");
    session = mylite_connection_session_state(database);
    failures += expect_int((int)session->previous_row_count, 1, "later insert row count");
    failures +=
        expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize stale materialized cursor");
    stmt = NULL;
    session = mylite_connection_session_state(database);
    failures += expect_int((int)session->previous_row_count, 1, "preserve later insert row count");

    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'app'",
            strlen("SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'app'"),
            &stmt
        ),
        MYLITE_OK,
        "prepare materialized cursor before later error"
    );
    failures += expect_true(
        mylite_execute(
            database,
            "SELECT missing_column FROM missing_table",
            strlen("SELECT missing_column FROM missing_table"),
            &result
        ) != MYLITE_OK,
        "later statement fails"
    );
    mylite_result_free(result);
    later_error_code = mylite_errcode(database);
    (void)snprintf(later_error, sizeof(later_error), "%s", mylite_errmsg(database));
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize after later error");
    stmt = NULL;
    failures += expect_int(mylite_errcode(database), later_error_code, "preserve later error code");
    failures += expect_text(mylite_errmsg(database), later_error, "preserve later error message");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_cursor_read_transaction_lifecycle(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    mylite_stmt *blocked_stmt = NULL;
    mylite_result *blocked_result = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "read_transaction") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open read transaction file");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE items (id INT NOT NULL)");
    failures += execute_ok(database, "INSERT INTO items VALUES (1), (2)");
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += expect_int(sqlite3_get_autocommit(sqlite), 1, "initial SQLite autocommit");

    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT id FROM items ORDER BY id",
            strlen("SELECT id FROM items ORDER BY id"),
            &stmt
        ),
        MYLITE_OK,
        "prepare read transaction cursor"
    );
    failures += expect_int(sqlite3_get_autocommit(sqlite), 0, "cursor read transaction active");
    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT id FROM items",
            strlen("SELECT id FROM items"),
            &blocked_stmt
        ),
        MYLITE_ERROR,
        "reject second active cursor"
    );
    failures += expect_true(blocked_stmt == NULL, "rejected cursor handle");
    failures += expect_int(
        mylite_errcode(database),
        mysql_error_commands_out_of_sync,
        "active cursor error code"
    );
    failures += expect_contains(
        mylite_errmsg(database),
        "Commands out of sync",
        "active cursor error message"
    );
    failures += expect_int(
        mylite_execute(
            database,
            "UPDATE items SET id = id",
            strlen("UPDATE items SET id = id"),
            &blocked_result
        ),
        MYLITE_ERROR,
        "reject command during active cursor"
    );
    failures += expect_true(blocked_result == NULL, "rejected command result");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "read transaction first row");
    failures +=
        expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "early finalize read transaction");
    stmt = NULL;
    failures += expect_int(sqlite3_get_autocommit(sqlite), 1, "early finalize ends transaction");

    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT id FROM items ORDER BY id",
            strlen("SELECT id FROM items ORDER BY id"),
            &stmt
        ),
        MYLITE_OK,
        "prepare exhausted read transaction cursor"
    );
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "exhausted cursor first row");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "exhausted cursor second row");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "exhausted cursor done");
    failures += expect_int(sqlite3_get_autocommit(sqlite), 1, "exhaustion ends transaction");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize exhausted cursor");
    stmt = NULL;

    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'app'",
            strlen("SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'app'"),
            &stmt
        ),
        MYLITE_OK,
        "prepare materialized read transaction cursor"
    );
    failures += expect_int(
        sqlite3_get_autocommit(sqlite),
        1,
        "materialized cursor releases read transaction"
    );
    failures += execute_ok(database, "SELECT id FROM items LIMIT 1");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize materialized cursor");
    stmt = NULL;

    failures += execute_ok(database, "START TRANSACTION");
    failures += expect_int(sqlite3_get_autocommit(sqlite), 0, "user transaction active");
    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT id FROM items ORDER BY id",
            strlen("SELECT id FROM items ORDER BY id"),
            &stmt
        ),
        MYLITE_OK,
        "prepare cursor in user transaction"
    );
    failures +=
        expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize cursor in user transaction");
    stmt = NULL;
    failures +=
        expect_int(sqlite3_get_autocommit(sqlite), 0, "cursor leaves user transaction active");
    failures += execute_ok(database, "ROLLBACK");
    failures += expect_int(sqlite3_get_autocommit(sqlite), 1, "rollback ends user transaction");

    failures += execute_ok(database, "SET autocommit = 0");
    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT id FROM items ORDER BY id",
            strlen("SELECT id FROM items ORDER BY id"),
            &stmt
        ),
        MYLITE_OK,
        "prepare autocommit-disabled cursor"
    );
    failures += expect_int(
        sqlite3_get_autocommit(sqlite),
        0,
        "autocommit-disabled cursor starts user transaction"
    );
    failures +=
        expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize autocommit-disabled cursor");
    stmt = NULL;
    failures += expect_int(
        sqlite3_get_autocommit(sqlite),
        0,
        "autocommit-disabled transaction remains active"
    );
    failures += execute_ok(database, "COMMIT");
    failures += expect_int(
        sqlite3_get_autocommit(sqlite),
        1,
        "commit ends autocommit-disabled transaction"
    );
    failures += execute_ok(database, "SET autocommit = 1");

    mylite_close(database);
    remove_related_files(path);
    return failures;
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

static int test_cursor_keeps_borrowed_metadata_across_invalidation(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    struct loaded_table_columns_cache_entry *borrowed_entry = NULL;
    struct loaded_table_key_metadata_cache_entry *borrowed_key_entry = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "borrowed_columns") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open borrowed columns file");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(
        database,
        "CREATE TABLE items (id INT NOT NULL PRIMARY KEY, name VARCHAR(20), INDEX idx_name (name))"
    );
    failures += execute_ok(database, "INSERT INTO items VALUES (1, 'alpha')");
    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT name FROM items WHERE id = 1",
            strlen("SELECT name FROM items WHERE id = 1"),
            &stmt
        ),
        MYLITE_OK,
        "prepare borrowed columns cursor"
    );
    for (size_t index = 0U; index < database->table_columns_cache_count; ++index) {
        if (database->table_columns_cache[index].reference_count != 0U) {
            borrowed_entry = &database->table_columns_cache[index];
            break;
        }
    }
    for (size_t index = 0U; index < database->table_key_metadata_cache_count; ++index) {
        if (database->table_key_metadata_cache[index].reference_count != 0U) {
            borrowed_key_entry = &database->table_key_metadata_cache[index];
            break;
        }
    }
    failures += expect_true(borrowed_entry != NULL, "cursor pins table columns cache entry");
    failures += expect_true(borrowed_key_entry != NULL, "cursor pins table key cache entry");
    if (borrowed_entry != NULL) {
        failures += expect_size(borrowed_entry->reference_count, 1U, "borrowed column reference");
    }
    if (borrowed_key_entry != NULL) {
        failures +=
            expect_size(borrowed_key_entry->reference_count, 1U, "borrowed key metadata reference");
    }
    mylite_catalog_invalidate_descriptor_cache(database);
    if (borrowed_entry != NULL) {
        failures += expect_true(!borrowed_entry->is_valid, "invalidated borrowed column entry");
        failures +=
            expect_true(borrowed_entry->columns != NULL, "borrowed columns remain allocated");
    }
    if (borrowed_key_entry != NULL) {
        failures += expect_true(!borrowed_key_entry->is_valid, "invalidated borrowed key entry");
        failures += expect_true(
            borrowed_key_entry->metadata.primary_key.parts != NULL,
            "borrowed primary key metadata remains allocated"
        );
        failures += expect_true(
            borrowed_key_entry->metadata.indexes != NULL,
            "borrowed secondary key metadata remains allocated"
        );
    }
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "borrowed columns cursor row");
    failures += expect_cursor_text(stmt, 0U, "alpha", "borrowed columns cursor value");
    failures +=
        expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize borrowed columns cursor");
    stmt = NULL;
    if (borrowed_entry != NULL) {
        failures += expect_size(borrowed_entry->reference_count, 0U, "released column reference");
        failures += expect_true(borrowed_entry->columns == NULL, "released invalid column entry");
    }
    if (borrowed_key_entry != NULL) {
        failures +=
            expect_size(borrowed_key_entry->reference_count, 0U, "released key metadata reference");
        failures += expect_true(
            borrowed_key_entry->metadata.primary_key.parts == NULL,
            "released invalid primary key metadata"
        );
        failures += expect_true(
            borrowed_key_entry->metadata.indexes == NULL,
            "released invalid secondary key metadata"
        );
    }

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_key_metadata_cache_uses_lru_replacement(void) {
    char path[test_path_capacity];
    char sql[sql_text_capacity];
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor first_table = {0};
    struct mylite_catalog_table_descriptor second_table = {0};
    struct mylite_catalog_table_descriptor last_table = {0};
    int failures = 0;

    if (make_test_path(path, sizeof(path), "key_metadata_lru") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open key metadata LRU file");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    for (size_t index = 0U; index <= MYLITE_EXECUTION_TABLE_KEY_METADATA_CACHE_LIMIT; ++index) {
        int written = snprintf(
            sql,
            sizeof(sql),
            "CREATE TABLE table_%zu (id INT NOT NULL PRIMARY KEY)",
            index
        );

        failures += expect_true(written > 0 && (size_t)written < sizeof(sql), "format LRU table");
        if (written > 0 && (size_t)written < sizeof(sql)) {
            failures += execute_ok(database, sql);
        }
    }
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read LRU schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "table_0", &first_table),
        MYLITE_OK,
        "read first LRU table"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "table_1", &second_table),
        MYLITE_OK,
        "read second LRU table"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "table_64", &last_table),
        MYLITE_OK,
        "read last LRU table"
    );

    for (size_t index = 0U; index < MYLITE_EXECUTION_TABLE_KEY_METADATA_CACHE_LIMIT; ++index) {
        int written = snprintf(sql, sizeof(sql), "SELECT id FROM table_%zu", index);

        failures += expect_true(written > 0 && (size_t)written < sizeof(sql), "format LRU select");
        if (written > 0 && (size_t)written < sizeof(sql)) {
            failures += expect_int(
                mylite_prepare(database, sql, (size_t)written, &stmt),
                MYLITE_OK,
                "prepare LRU select"
            );
            failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize LRU select");
            stmt = NULL;
        }
    }
    failures += expect_size(
        database->table_key_metadata_cache_count,
        MYLITE_EXECUTION_TABLE_KEY_METADATA_CACHE_LIMIT,
        "filled key metadata cache"
    );
    failures += expect_int(
        mylite_prepare(database, "SELECT id FROM table_0", strlen("SELECT id FROM table_0"), &stmt),
        MYLITE_OK,
        "refresh hot LRU entry"
    );
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize hot LRU select");
    stmt = NULL;
    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT id FROM table_64",
            strlen("SELECT id FROM table_64"),
            &stmt
        ),
        MYLITE_OK,
        "prepare replacement LRU select"
    );
    failures +=
        expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize replacement LRU select");
    stmt = NULL;

    failures += expect_true(
        (int)key_metadata_cache_contains_table(database, first_table.table_id),
        "hot key metadata entry survives"
    );
    failures += expect_true(
        !key_metadata_cache_contains_table(database, second_table.table_id),
        "oldest key metadata entry evicted"
    );
    failures += expect_true(
        (int)key_metadata_cache_contains_table(database, last_table.table_id),
        "replacement key metadata entry cached"
    );

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

static int test_cursor_reset_and_value_nullability(void) {
    char path[test_path_capacity];
    static const char query[] = "SELECT nullable_value, empty_value FROM items";
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "reset_nullability") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open reset file");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(
        database,
        "CREATE TABLE items (nullable_value VARCHAR(8), empty_value VARCHAR(8))"
    );
    failures += execute_ok(database, "INSERT INTO items VALUES (NULL, '')");
    failures += expect_int(
        mylite_prepare(database, query, strlen(query), &stmt),
        MYLITE_OK,
        "prepare reset cursor"
    );
    failures += expect_size(mylite_stmt_parameter_count(stmt), 0U, "reset parameter count");
    failures += expect_true(mylite_stmt_affected_rows(stmt) == -1, "reset affected rows default");
    failures += expect_uint64(mylite_stmt_insert_id(stmt), 0U, "reset insert id default");
    failures +=
        expect_int(mylite_stmt_bind_null(stmt, 0U), MYLITE_MISUSE, "reject nonexistent parameter");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "reset first row");
    failures += expect_true(mylite_stmt_value_is_null(stmt, 0U), "cursor NULL value");
    failures += expect_true(!mylite_stmt_value_is_null(stmt, 1U), "cursor empty value is not NULL");
    failures += expect_size(mylite_stmt_value_size(stmt, 1U), 0U, "cursor empty value size");
    failures +=
        expect_true(mylite_stmt_value_bytes(stmt, 1U) != NULL, "cursor empty value has storage");
    failures += expect_int(
        mylite_stmt_clear_bindings(stmt),
        MYLITE_MISUSE,
        "reject clearing bindings while row is active"
    );
    failures += expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset active cursor");
    failures += expect_true(mylite_stmt_value_is_null(stmt, 0U), "reset clears current row");
    failures += expect_int(mylite_stmt_clear_bindings(stmt), MYLITE_OK, "clear zero bindings");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "reset repeated row");
    failures += expect_true(mylite_stmt_value_is_null(stmt, 0U), "repeated cursor NULL value");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "reset repeated done");
    failures += expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset exhausted cursor");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "exhausted reset row");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize reset cursor");
    stmt = NULL;

    failures += expect_int(
        mylite_execute(database, query, strlen(query), &result),
        MYLITE_OK,
        "materialize nullable result"
    );
    failures += expect_true(mylite_result_value_is_null(result, 0U, 0U), "result NULL value");
    failures +=
        expect_true(!mylite_result_value_is_null(result, 0U, 1U), "result empty value is not NULL");
    failures +=
        expect_size(mylite_result_value_size(result, 0U, 1U), 0U, "result empty value size");
    failures += expect_true(
        mylite_result_value_bytes(result, 0U, 1U) != NULL,
        "result empty value has storage"
    );

    mylite_result_free(result);
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_native_prepared_scalar_bindings(void) {
    char path[test_path_capacity];
    static const unsigned char blob[] = {'a', 0U, 'b', '\'', '-', '-'};
    static const char injection_text[] = "x' OR 1=1 /*";
    mylite_db *database = NULL;
    mylite_db *other_database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "native_bindings") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open native bindings file");
    failures += execute_ok(database, "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE items (id INT NOT NULL, name VARCHAR(20))");
    failures += execute_ok(database, "INSERT INTO items VALUES (1, 'alpha'), (2, 'beta')");
    failures += expect_int(
        mylite_prepare(database, "SELECT ? AS value", strlen("SELECT ? AS value"), &stmt),
        MYLITE_OK,
        "prepare native scalar parameter"
    );
    failures += expect_size(mylite_stmt_parameter_count(stmt), 1U, "native parameter count");
    failures += expect_int(
        mylite_stmt_step(stmt),
        MYLITE_MISUSE,
        "native missing binding fails before execution"
    );
    failures +=
        expect_int(mylite_stmt_bind_int64(stmt, 0U, -42), MYLITE_OK, "bind native signed integer");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native signed integer row");
    failures += expect_cursor_text(stmt, 0U, "-42", "native signed integer value");
    failures += expect_int(
        mylite_stmt_bind_int64(stmt, 0U, 7),
        MYLITE_MISUSE,
        "reject native rebind while row is active"
    );

    failures += expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset native text binding");
    failures += expect_int(
        mylite_stmt_bind_text(stmt, 0U, injection_text, strlen(injection_text)),
        MYLITE_OK,
        "bind native injection text"
    );
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native injection text row");
    failures += expect_cursor_text(stmt, 0U, injection_text, "native injection text value");

    failures += expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset native blob binding");
    failures += expect_int(
        mylite_stmt_bind_blob(stmt, 0U, blob, sizeof(blob)),
        MYLITE_OK,
        "bind native embedded NUL blob"
    );
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native blob row");
    failures += expect_true(!mylite_stmt_value_is_null(stmt, 0U), "native blob is not NULL");
    failures += expect_size(mylite_stmt_value_size(stmt, 0U), sizeof(blob), "native blob size");
    failures += expect_true(
        mylite_stmt_value_bytes(stmt, 0U) != NULL &&
            memcmp(mylite_stmt_value_bytes(stmt, 0U), blob, sizeof(blob)) == 0,
        "native blob bytes"
    );

    failures += expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset native NULL binding");
    failures += expect_int(mylite_stmt_bind_null(stmt, 0U), MYLITE_OK, "bind native NULL");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native NULL row");
    failures += expect_true(mylite_stmt_value_is_null(stmt, 0U), "native NULL value");

    failures += expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset native unsigned binding");
    failures += expect_int(
        mylite_stmt_bind_uint64(stmt, 0U, UINT64_MAX),
        MYLITE_OK,
        "bind native maximum unsigned integer"
    );
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native unsigned row");
    failures +=
        expect_cursor_text(stmt, 0U, "18446744073709551615", "native maximum unsigned value");

    failures += expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset native double binding");
    failures +=
        expect_int(mylite_stmt_bind_double(stmt, 0U, 1.25), MYLITE_OK, "bind native double");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native double row");
    failures += expect_cursor_text(stmt, 0U, "1.25", "native double value");

    failures += expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset native empty text");
    failures +=
        expect_int(mylite_stmt_bind_text(stmt, 0U, NULL, 0U), MYLITE_OK, "bind native empty text");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native empty text row");
    failures += expect_true(!mylite_stmt_value_is_null(stmt, 0U), "native empty text is not NULL");
    failures += expect_size(mylite_stmt_value_size(stmt, 0U), 0U, "native empty text size");

    failures += expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset native cleared binding");
    failures += expect_int(mylite_stmt_clear_bindings(stmt), MYLITE_OK, "clear native binding");
    failures += expect_int(
        mylite_stmt_step(stmt),
        MYLITE_MISUSE,
        "cleared native binding fails before execution"
    );
    failures += expect_int(
        mylite_stmt_bind_text(stmt, 1U, "bad", 3U),
        MYLITE_MISUSE,
        "reject native binding index"
    );
    failures += expect_int(
        mylite_stmt_bind_blob(stmt, 0U, NULL, 1U),
        MYLITE_MISUSE,
        "reject native nonempty NULL blob pointer"
    );

    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize native binding");
    stmt = NULL;

    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT '?' AS string_marker, \"?\" AS double_string_marker, "
            "1 AS `?`, ? AS bound_value /* ? */ -- ?\n",
            strlen(
                "SELECT '?' AS string_marker, \"?\" AS double_string_marker, "
                "1 AS `?`, ? AS bound_value /* ? */ -- ?\n"
            ),
            &stmt
        ),
        MYLITE_OK,
        "prepare native quoted and commented markers"
    );
    failures +=
        expect_size(mylite_stmt_parameter_count(stmt), 1U, "quoted and commented marker count");
    failures +=
        expect_int(mylite_stmt_bind_int64(stmt, 0U, 31), MYLITE_OK, "bind classified marker");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "classified marker row");
    failures += expect_cursor_text(stmt, 0U, "?", "quoted string marker value");
    failures += expect_cursor_text(stmt, 1U, "?", "double-quoted string marker value");
    failures += expect_cursor_text(stmt, 2U, "1", "quoted identifier marker value");
    failures += expect_cursor_text(stmt, 3U, "31", "classified bound marker value");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize classified markers");
    stmt = NULL;

    failures += execute_ok(database, "SET SESSION sql_mode = 'ANSI_QUOTES'");
    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT 1 AS \"?\", ? AS bound_value",
            strlen("SELECT 1 AS \"?\", ? AS bound_value"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native ANSI_QUOTES marker classification"
    );
    failures += expect_size(mylite_stmt_parameter_count(stmt), 1U, "ANSI_QUOTES marker count");
    failures +=
        expect_int(mylite_stmt_bind_int64(stmt, 0U, 32), MYLITE_OK, "bind ANSI_QUOTES marker");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "ANSI_QUOTES marker row");
    failures += expect_cursor_text(stmt, 0U, "1", "ANSI_QUOTES identifier marker value");
    failures += expect_cursor_text(stmt, 1U, "32", "ANSI_QUOTES bound marker value");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize ANSI_QUOTES markers");
    stmt = NULL;

    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT /*!80000 ? AS executable_value, */ ? AS ordinary_value "
            "/*!99999 , ? AS skipped_value */",
            strlen(
                "SELECT /*!80000 ? AS executable_value, */ ? AS ordinary_value "
                "/*!99999 , ? AS skipped_value */"
            ),
            &stmt
        ),
        MYLITE_OK,
        "prepare native executable-comment markers"
    );
    failures +=
        expect_size(mylite_stmt_parameter_count(stmt), 2U, "version-gated executable marker count");
    failures += expect_int(
        mylite_stmt_bind_int64(stmt, 0U, 41),
        MYLITE_OK,
        "bind executable-comment marker"
    );
    failures += expect_int(
        mylite_stmt_bind_int64(stmt, 1U, 42),
        MYLITE_OK,
        "bind ordinary marker after executable comment"
    );
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "executable-comment marker row");
    failures += expect_cursor_text(stmt, 0U, "41", "executable-comment marker value");
    failures += expect_cursor_text(stmt, 1U, "42", "ordinary marker value");
    failures +=
        expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize executable-comment markers");
    stmt = NULL;

    failures += expect_int(
        mylite_prepare(database, "SELECT * FROM ?", strlen("SELECT * FROM ?"), &stmt),
        MYLITE_ERROR,
        "reject native identifier marker"
    );
    failures +=
        expect_contains(mylite_errmsg(database), "syntax", "native identifier marker diagnostic");
    (void)mylite_stmt_finalize(stmt);
    stmt = NULL;

    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT name FROM items WHERE id = ? ORDER BY id LIMIT ? OFFSET ?",
            strlen("SELECT name FROM items WHERE id = ? ORDER BY id LIMIT ? OFFSET ?"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native table query"
    );
    failures += expect_size(mylite_stmt_parameter_count(stmt), 3U, "native table parameter count");
    failures += expect_int(mylite_stmt_bind_int64(stmt, 0U, 2), MYLITE_OK, "bind native row id");
    failures += expect_int(mylite_stmt_bind_uint64(stmt, 1U, 1U), MYLITE_OK, "bind native limit");
    failures += expect_int(mylite_stmt_bind_uint64(stmt, 2U, 0U), MYLITE_OK, "bind native offset");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native table row");
    failures += expect_cursor_text(stmt, 0U, "beta", "native table value");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "native table done");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize native table query");
    stmt = NULL;

    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT ? AS a_value, ? AS b_value",
            strlen("SELECT ? AS a_value, ? AS b_value"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native parameter order query"
    );
    failures += expect_size(mylite_stmt_parameter_count(stmt), 2U, "native ordered parameters");
    failures +=
        expect_int(mylite_stmt_bind_int64(stmt, 0U, 11), MYLITE_OK, "bind native first slot");
    failures +=
        expect_int(mylite_stmt_bind_int64(stmt, 1U, 22), MYLITE_OK, "bind native second slot");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native ordered parameter row");
    failures += expect_cursor_text(stmt, 0U, "11", "native first parameter order");
    failures += expect_cursor_text(stmt, 1U, "22", "native second parameter order");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "native ordered parameter done");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize native order query");
    stmt = NULL;

    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT name FROM items WHERE id = ?",
            strlen("SELECT name FROM items WHERE id = ?"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native schema-reprepare query"
    );
    failures += expect_int(mylite_open(path, &other_database), MYLITE_OK, "open native DDL handle");
    failures += execute_ok(other_database, "USE app");
    failures += execute_ok(other_database, "ALTER TABLE items ADD COLUMN note VARCHAR(20)");
    failures += expect_int(
        mylite_stmt_bind_int64(stmt, 0U, 2),
        MYLITE_OK,
        "bind native query after compatible DDL"
    );
    failures +=
        expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native query reparses plan after DDL");
    failures += expect_cursor_text(stmt, 0U, "beta", "native query value after compatible DDL");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "native query done after DDL");
    failures += expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset native query before rename");
    failures += execute_ok(other_database, "ALTER TABLE items RENAME COLUMN name TO label");
    failures += expect_int(
        mylite_stmt_step(stmt),
        MYLITE_ERROR,
        "native query reports incompatible schema change"
    );
    failures += expect_contains(
        mylite_errmsg(database),
        "Unknown column",
        "native reprepare schema diagnostic"
    );
    failures += expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset failed native reprepare");
    failures +=
        expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize native reprepare query");
    mylite_close(other_database);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_native_prepared_owns_sql_text(void) {
    char path[test_path_capacity];
    static const char prepared_sql[] = "SELECT ? AS retained_value";
    char *caller_sql = NULL;
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "native_sql_lifetime") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open SQL lifetime file");
    caller_sql = malloc(sizeof(prepared_sql));
    if (caller_sql == NULL) {
        mylite_close(database);
        remove_related_files(path);
        return failures + 1;
    }
    memcpy(caller_sql, prepared_sql, sizeof(prepared_sql));
    failures += expect_int(
        mylite_prepare(database, caller_sql, sizeof(prepared_sql) - 1U, &stmt),
        MYLITE_OK,
        "prepare from caller-owned SQL"
    );
    memset(caller_sql, 'x', sizeof(prepared_sql) - 1U);
    free(caller_sql);
    caller_sql = NULL;

    failures += expect_int(mylite_stmt_bind_int64(stmt, 0U, 17), MYLITE_OK, "bind retained SQL");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "execute retained SQL");
    failures += expect_cursor_text(stmt, 0U, "17", "retained SQL result");
    failures += expect_text(
        mylite_stmt_column_name(stmt, 0U),
        "retained_value",
        "retained SQL column name"
    );
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize retained SQL");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_native_prepared_dml_bindings(void) {
    char path[test_path_capacity];
    static const unsigned char first_payload[] = {'a', 0U, 'b'};
    static const char injection_text[] = "x' OR 1=1 /*";
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "native_dml_bindings") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open native DML file");
    failures += execute_ok(database, "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(
        database,
        "CREATE TABLE items (id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
        "name VARCHAR(64) NOT NULL, payload VARBINARY(64))"
    );

    failures += expect_int(
        mylite_prepare(
            database,
            "INSERT INTO items (name, payload) VALUES (?, ?)",
            strlen("INSERT INTO items (name, payload) VALUES (?, ?)"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native INSERT"
    );
    failures += expect_size(mylite_stmt_parameter_count(stmt), 2U, "native INSERT parameters");
    failures += expect_int(
        mylite_stmt_bind_text(stmt, 0U, injection_text, strlen(injection_text)),
        MYLITE_OK,
        "bind native INSERT text"
    );
    failures += expect_int(
        mylite_stmt_step(stmt),
        MYLITE_MISUSE,
        "native INSERT rejects missing payload before execution"
    );
    failures += expect_query_scalar_text(
        database,
        (struct expected_query_scalar_text){
            .sql = "SELECT COUNT(*) FROM items",
            .expected = "0",
            .context = "missing native INSERT binding has no side effects",
        }
    );
    failures += expect_int(
        mylite_stmt_bind_blob(stmt, 1U, first_payload, sizeof(first_payload)),
        MYLITE_OK,
        "bind native INSERT blob"
    );
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "execute native INSERT");
    failures += expect_true(mylite_stmt_affected_rows(stmt) == 1, "native INSERT affected rows");
    failures += expect_uint64(mylite_stmt_insert_id(stmt), 1U, "native INSERT id");
    failures += expect_query_scalar_text(
        database,
        (struct expected_query_scalar_text){
            .sql = "SELECT COUNT(*) FROM items WHERE name = 'x'' OR 1=1 /*'",
            .expected = "1",
            .context = "native INSERT text remains data",
        }
    );
    failures += expect_query_scalar_text(
        database,
        (struct expected_query_scalar_text){
            .sql = "SELECT HEX(payload) FROM items WHERE id = 1",
            .expected = "610062",
            .context = "native INSERT preserves embedded NUL",
        }
    );

    failures += expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset native INSERT");
    failures += expect_int(
        mylite_stmt_bind_text(stmt, 0U, "second", strlen("second")),
        MYLITE_OK,
        "rebind native INSERT text"
    );
    failures += expect_int(
        mylite_stmt_bind_blob(stmt, 1U, NULL, 0U),
        MYLITE_OK,
        "rebind native INSERT empty blob"
    );
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "repeat native INSERT");
    failures += expect_true(mylite_stmt_affected_rows(stmt) == 1, "repeated INSERT affected rows");
    failures += expect_uint64(mylite_stmt_insert_id(stmt), 2U, "repeated INSERT id");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize native INSERT");
    stmt = NULL;

    failures += expect_int(
        mylite_prepare(
            database,
            "INSERT INTO items (id, name) VALUES (?, ?)",
            strlen("INSERT INTO items (id, name) VALUES (?, ?)"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native constraint INSERT"
    );
    failures += expect_int(mylite_stmt_bind_int64(stmt, 0U, 2), MYLITE_OK, "bind duplicate id");
    failures += expect_int(
        mylite_stmt_bind_text(stmt, 1U, "duplicate", strlen("duplicate")),
        MYLITE_OK,
        "bind duplicate row value"
    );
    failures +=
        expect_int(mylite_stmt_step(stmt), MYLITE_ERROR, "native INSERT reports constraint error");
    failures += expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset failed native INSERT");
    failures += expect_int(mylite_stmt_bind_int64(stmt, 0U, 5), MYLITE_OK, "rebind recovered id");
    failures += expect_int(
        mylite_stmt_bind_text(stmt, 1U, "recovered", strlen("recovered")),
        MYLITE_OK,
        "rebind recovered row value"
    );
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "execute recovered native INSERT");
    failures += expect_true(mylite_stmt_affected_rows(stmt) == 1, "recovered INSERT affected rows");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize constraint INSERT");
    stmt = NULL;

    failures += expect_int(
        mylite_prepare(
            database,
            "UPDATE items SET name = ? WHERE id = ?",
            strlen("UPDATE items SET name = ? WHERE id = ?"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native UPDATE"
    );
    failures += expect_int(
        mylite_stmt_bind_text(stmt, 0U, "changed", strlen("changed")),
        MYLITE_OK,
        "bind native UPDATE value"
    );
    failures += expect_int(mylite_stmt_bind_int64(stmt, 1U, 2), MYLITE_OK, "bind native UPDATE id");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "execute native UPDATE");
    failures += expect_true(mylite_stmt_affected_rows(stmt) == 1, "native UPDATE affected rows");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize native UPDATE");
    stmt = NULL;

    failures += expect_int(
        mylite_prepare(
            database,
            "DELETE FROM items WHERE id = ?",
            strlen("DELETE FROM items WHERE id = ?"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native DELETE"
    );
    failures += expect_int(mylite_stmt_bind_int64(stmt, 0U, 1), MYLITE_OK, "bind native DELETE id");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "execute native DELETE");
    failures += expect_true(mylite_stmt_affected_rows(stmt) == 1, "native DELETE affected rows");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize native DELETE");
    stmt = NULL;

    failures += expect_int(
        mylite_prepare(
            database,
            "INSERT INTO items (name) VALUES ('constant')",
            strlen("INSERT INTO items (name) VALUES ('constant')"),
            &stmt
        ),
        MYLITE_OK,
        "prepare zero-parameter INSERT"
    );
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "execute zero-parameter INSERT");
    failures +=
        expect_true(mylite_stmt_affected_rows(stmt) == 1, "zero-parameter INSERT affected rows");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize zero-parameter INSERT");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_buffered_prepared_statement_releases_connection(void) {
    char path[test_path_capacity];
    const struct mylite_session_state *session = NULL;
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "buffered_prepared") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open buffered prepared");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE items (id INT NOT NULL, name VARCHAR(20))");
    failures += execute_ok(database, "INSERT INTO items VALUES (1, 'alpha'), (2, 'beta')");
    failures += expect_int(
        mylite_prepare_buffered(
            database,
            "SELECT name FROM items WHERE id >= ? ORDER BY id",
            strlen("SELECT name FROM items WHERE id >= ? ORDER BY id"),
            &stmt
        ),
        MYLITE_OK,
        "prepare buffered parameter query"
    );
    failures += expect_int(mylite_stmt_bind_int64(stmt, 0U, 1), MYLITE_OK, "bind buffered id");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "buffered first row");
    failures += expect_cursor_text(stmt, 0U, "alpha", "buffered first value");
    session = mylite_connection_session_state(database);
    failures += expect_int((int)session->previous_row_count, -1, "publish buffered row count");
    failures += execute_ok(database, "INSERT INTO items VALUES (3, 'gamma')");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "buffered unread row");
    failures += expect_cursor_text(stmt, 0U, "beta", "buffered unread value");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "buffered result done");
    session = mylite_connection_session_state(database);
    failures +=
        expect_int((int)session->previous_row_count, 1, "preserve later statement row count");

    failures += expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset buffered statement");
    failures += expect_int(mylite_stmt_bind_int64(stmt, 0U, 3), MYLITE_OK, "rebind buffered id");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "reexecuted buffered row");
    failures += expect_cursor_text(stmt, 0U, "gamma", "reexecuted buffered value");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize buffered statement");

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
            strlen(
                "SELECT COLUMN_NAME, DATA_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
                "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'items' AND COLUMN_NAME = 'id'"
            ),
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

static int test_cursor_prepare_statement_surface(void) {
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
            "SELECT missing_column FROM items",
            strlen("SELECT missing_column FROM items"),
            &stmt
        ),
        MYLITE_ERROR,
        "preserve cursor planning error"
    );
    failures += expect_true(stmt == NULL, "failed cursor planning statement is null");
    failures += expect_contains(
        mylite_errmsg(database),
        "Unknown column 'missing_column'",
        "cursor planning diagnostic"
    );

    failures += expect_int(
        mylite_prepare(
            database,
            "CREATE TABLE another (id INT)",
            strlen("CREATE TABLE another (id INT)"),
            &stmt
        ),
        MYLITE_OK,
        "prepare DDL"
    );
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "execute prepared DDL");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize prepared DDL");
    stmt = NULL;

    failures += expect_int(
        mylite_prepare_buffered(database, "SHOW TABLES", strlen("SHOW TABLES"), &stmt),
        MYLITE_OK,
        "prepare SHOW"
    );
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "prepared SHOW first row");
    failures += expect_cursor_text(stmt, 0U, "another", "prepared SHOW first table");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "prepared SHOW second row");
    failures += expect_cursor_text(stmt, 0U, "items", "prepared SHOW second table");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "prepared SHOW done");
    failures += expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset prepared SHOW");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "reexecute prepared SHOW");
    failures += expect_cursor_text(stmt, 0U, "another", "reexecuted prepared SHOW table");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize prepared SHOW");
    stmt = NULL;

    failures += expect_int(
        mylite_prepare(
            database,
            "PREPARE nested FROM 'SELECT 1'",
            strlen("PREPARE nested FROM 'SELECT 1'"),
            &stmt
        ),
        MYLITE_ERROR,
        "prepare rejects nested PREPARE"
    );
    failures += expect_true(stmt == NULL, "nested prepare leaves null statement");
    failures +=
        expect_contains(mylite_errmsg(database), "not supported", "nested prepare diagnostic");

    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT ? FROM missing_items",
            strlen("SELECT ? FROM missing_items"),
            &stmt
        ),
        MYLITE_ERROR,
        "prepared parameter resolves table at prepare time"
    );
    failures += expect_true(stmt == NULL, "missing prepared table leaves null statement");
    failures += expect_contains(mylite_errmsg(database), "doesn't exist", "missing prepared table");

    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT missing_column FROM items WHERE id = ?",
            strlen("SELECT missing_column FROM items WHERE id = ?"),
            &stmt
        ),
        MYLITE_ERROR,
        "prepared parameter resolves column at prepare time"
    );
    failures += expect_true(stmt == NULL, "missing prepared column leaves null statement");
    failures +=
        expect_contains(mylite_errmsg(database), "Unknown column", "missing prepared column");

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

static bool key_metadata_cache_contains_table(const mylite_db *database, int64_t table_id) {
    for (size_t index = 0U; index < database->table_key_metadata_cache_count; ++index) {
        const struct loaded_table_key_metadata_cache_entry *entry =
            &database->table_key_metadata_cache[index];

        if (entry->is_valid && entry->table_id == table_id) {
            return true;
        }
    }
    return false;
}
