#include "mylite_test_support.h"

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
    cache_budget_sql_capacity = 4096,
    cache_budget_table_count = 40,
    cache_budget_column_count = 16,
    diagnostic_text_capacity = 256,
    native_signed_binding_value = -42,
    native_rebind_value = 7,
    classified_marker_value = 31,
    ansi_marker_value = 32,
    executable_marker_value = 41,
    ordinary_marker_value = 42,
    ordered_first_value = 11,
    ordered_second_value = 22,
    retained_sql_value = 17,
    recovered_insert_id = 5,
    update_arithmetic_delta = 10,
    mysql_error_duplicate_key = 1062,
};

static const double native_double_binding_value = 1.25;

struct expected_query_scalar_text {
    const char *sql;
    const char *expected;
    const char *context;
};

enum native_prepared_binding_kind {
    NATIVE_PREPARED_BINDING_INT64 = 0,
    NATIVE_PREPARED_BINDING_TEXT,
};

struct native_prepared_binding {
    enum native_prepared_binding_kind kind;
    int64_t integer;
    const char *text;
};

static int test_cursor_select_streams_rows_and_metadata(void);
static int test_cursor_integer_text_boundaries(void);
static int test_cursor_keeps_borrowed_metadata_across_invalidation(void);
static int test_key_metadata_cache_uses_lru_replacement(void);
static int test_metadata_caches_enforce_byte_budgets(void);
static int create_cache_budget_tables(mylite_db *database, char *sql, size_t sql_capacity);
static bool append_cache_budget_column_list(
    char *sql,
    size_t sql_capacity,
    size_t *length,
    bool include_types
);
static int prepare_cache_budget_selects(mylite_db *database, char *sql, size_t sql_capacity);
static int test_cursor_reuses_finalized_select_statements(void);
static int test_cursor_reset_and_value_nullability(void);
static int test_native_prepared_scalar_bindings(void);
static int expect_native_prepared_row(
    mylite_db *database,
    const char *sql,
    const struct native_prepared_binding *bindings,
    size_t binding_count,
    const char *const *expected_values,
    size_t expected_value_count,
    const char *context
);
static int test_native_prepared_owns_resolution_context(void);
static int test_native_prepared_owns_sql_text(void);
static int test_native_prepared_dml_bindings(void);
static int test_native_prepared_multirow_dml(void);
static int test_buffered_prepared_statement_releases_connection(void);
static int test_cursor_materializes_information_schema_selects(void);
static int test_cursor_prepare_statement_surface(void);
static int test_cursor_read_transaction_lifecycle(void);
static int test_streaming_cursor_reports_select_row_count(void);
static int test_cursor_connection_close_order(void);
static int test_materialized_cursor_does_not_overwrite_later_statement_state(void);
static int execute_ok(mylite_db *database, const char *sql);
static int expect_query_scalar_text(
    mylite_db *database,
    struct expected_query_scalar_text expected
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_cursor_text(
    const mylite_stmt *stmt,
    size_t column_index,
    const char *expected,
    const char *context
);
static int expect_cursor_null(const mylite_stmt *stmt, size_t column_index, const char *context);
static bool key_metadata_cache_contains_table(const mylite_db *database, int64_t table_id);

int main(void) {
    int failures = 0;

    failures += test_cursor_select_streams_rows_and_metadata();
    failures += test_cursor_integer_text_boundaries();
    failures += test_cursor_keeps_borrowed_metadata_across_invalidation();
    failures += test_key_metadata_cache_uses_lru_replacement();
    failures += test_metadata_caches_enforce_byte_budgets();
    failures += test_cursor_reuses_finalized_select_statements();
    failures += test_cursor_reset_and_value_nullability();
    failures += test_native_prepared_scalar_bindings();
    failures += test_native_prepared_owns_resolution_context();
    failures += test_native_prepared_owns_sql_text();
    failures += test_native_prepared_dml_bindings();
    failures += test_native_prepared_multirow_dml();
    failures += test_buffered_prepared_statement_releases_connection();
    failures += test_cursor_materializes_information_schema_selects();
    failures += test_cursor_prepare_statement_surface();
    failures += test_cursor_read_transaction_lifecycle();
    failures += test_streaming_cursor_reports_select_row_count();
    failures += test_cursor_connection_close_order();
    failures += test_materialized_cursor_does_not_overwrite_later_statement_state();

    return failures == 0 ? 0 : 1;
}

static int test_cursor_integer_text_boundaries(void) {
    static const char *const expected_values[] = {
        "0",
        "1",
        "-1",
        "9223372036854775807",
        "-9223372036854775808",
    };
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open cursor integer boundaries"
    );
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE integer_values (id INT, v BIGINT)");
    failures += execute_ok(
        database,
        "INSERT INTO integer_values VALUES "
        "(1, 0), (2, 1), (3, -1), "
        "(4, 9223372036854775807), (5, -9223372036854775808)"
    );
    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT v FROM integer_values ORDER BY id",
            strlen("SELECT v FROM integer_values ORDER BY id"),
            &stmt
        ),
        MYLITE_OK,
        "prepare cursor integer boundaries"
    );
    for (size_t index = 0U; index < sizeof(expected_values) / sizeof(expected_values[0]); ++index) {
        failures += mylite_test_expect_int(
            mylite_stmt_step(stmt),
            MYLITE_ROW,
            "step cursor integer boundary"
        );
        failures +=
            expect_cursor_text(stmt, 0U, expected_values[index], "cursor integer boundary text");
        failures += mylite_test_expect_size(
            mylite_stmt_value_size(stmt, 0U),
            strlen(expected_values[index]),
            "cursor integer boundary size"
        );
    }
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "finish integer boundaries");
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize integer boundaries"
    );

    mylite_close(database);
    return failures;
}

static int test_streaming_cursor_reports_select_row_count(void) {
    const struct mylite_session_state *session = NULL;
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open cursor row count");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE items (id INT NOT NULL)");
    failures += execute_ok(database, "INSERT INTO items VALUES (1), (2)");

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT id FROM items ORDER BY id",
            strlen("SELECT id FROM items ORDER BY id"),
            &stmt
        ),
        MYLITE_OK,
        "prepare early-finalized cursor row count"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "step early-finalized cursor");
    failures +=
        mylite_test_expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize partial cursor");
    stmt = NULL;
    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_true(
        session != NULL && session->previous_row_count == -1,
        "partial SELECT publishes ROW_COUNT -1"
    );

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT id FROM items ORDER BY id",
            strlen("SELECT id FROM items ORDER BY id"),
            &stmt
        ),
        MYLITE_OK,
        "prepare exhausted cursor row count"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "step exhausted cursor row one");
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "step exhausted cursor row two");
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "finish exhausted cursor");
    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_true(
        session != NULL && session->previous_row_count == -1,
        "exhausted SELECT publishes ROW_COUNT -1"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize exhausted cursor");

    mylite_close(database);
    return failures;
}

static int test_cursor_connection_close_order(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = mylite_test_make_path(path, sizeof(path), "connection_close_order");

    if (failures != 0) {
        return failures;
    }
    remove_related_files(path);
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open streaming close-order"
    );
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE items (id INT NOT NULL)");
    failures += execute_ok(database, "INSERT INTO items VALUES (1), (2)");
    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT id FROM items WHERE id >= ? ORDER BY id",
            strlen("SELECT id FROM items WHERE id >= ? ORDER BY id"),
            &stmt
        ),
        MYLITE_OK,
        "prepare streaming close-order cursor"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 0U, 1),
        MYLITE_OK,
        "bind close-order cursor"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "step before connection close");
    mylite_close(database);
    database = NULL;
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_MISUSE, "step detached cursor");
    failures +=
        mylite_test_expect_int(mylite_stmt_reset(stmt), MYLITE_MISUSE, "reset detached cursor");
    failures += mylite_test_expect_int(
        mylite_stmt_bind_null(stmt, 0U),
        MYLITE_MISUSE,
        "bind detached cursor"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_clear_bindings(stmt),
        MYLITE_MISUSE,
        "clear detached cursor bindings"
    );
    failures +=
        mylite_test_expect_size(mylite_stmt_parameter_count(stmt), 0U, "detached parameter count");
    failures +=
        mylite_test_expect_true(mylite_stmt_affected_rows(stmt) == -1, "detached affected rows");
    failures += mylite_test_expect_uint64(mylite_stmt_insert_id(stmt), 0U, "detached insert id");
    failures +=
        mylite_test_expect_size(mylite_stmt_column_count(stmt), 0U, "detached cursor metadata");
    failures +=
        mylite_test_expect_true(mylite_stmt_column_name(stmt, 0U) == NULL, "detached column name");
    failures += mylite_test_expect_true(
        mylite_stmt_column_schema_name(stmt, 0U) == NULL,
        "detached column schema"
    );
    failures += mylite_test_expect_true(
        mylite_stmt_column_table_name(stmt, 0U) == NULL,
        "detached column table"
    );
    failures += mylite_test_expect_true(
        mylite_stmt_column_origin_schema_name(stmt, 0U) == NULL,
        "detached column origin schema"
    );
    failures += mylite_test_expect_true(
        mylite_stmt_column_origin_table_name(stmt, 0U) == NULL,
        "detached column origin table"
    );
    failures += mylite_test_expect_true(
        mylite_stmt_column_origin_name(stmt, 0U) == NULL,
        "detached column origin name"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_column_type(stmt, 0U),
        MYLITE_RESULT_COLUMN_TYPE_UNKNOWN,
        "detached column type"
    );
    failures +=
        mylite_test_expect_uint64(mylite_stmt_column_flags(stmt, 0U), 0U, "detached column flags");
    failures += mylite_test_expect_uint64(
        mylite_stmt_column_charset_id(stmt, 0U),
        0U,
        "detached column charset"
    );
    failures += mylite_test_expect_uint64(
        mylite_stmt_column_collation_id(stmt, 0U),
        0U,
        "detached column collation"
    );
    failures += mylite_test_expect_uint64(
        mylite_stmt_column_display_length(stmt, 0U),
        0U,
        "detached column length"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_column_decimals(stmt, 0U), 0, "detached decimals");
    failures +=
        mylite_test_expect_int(mylite_stmt_column_nullable(stmt, 0U), 1, "detached nullable");
    failures += mylite_test_expect_true(
        mylite_stmt_value_is_null(stmt, 0U),
        "detached value null sentinel"
    );
    failures +=
        mylite_test_expect_true(mylite_stmt_value_text(stmt, 0U) == NULL, "detached value text");
    failures +=
        mylite_test_expect_true(mylite_stmt_value_bytes(stmt, 0U) == NULL, "detached value bytes");
    failures +=
        mylite_test_expect_size(mylite_stmt_value_size(stmt, 0U), 0U, "detached value size");
    failures +=
        mylite_test_expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize detached cursor");
    stmt = NULL;
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open materialized close-order"
    );
    failures += mylite_test_expect_int(
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
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_MISUSE,
        "step detached materialized cursor"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize detached materialized cursor"
    );
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
    int failures = mylite_test_make_path(path, sizeof(path), "stale_cursor_state");

    if (failures != 0) {
        return failures;
    }
    remove_related_files(path);
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open stale cursor state");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE items (id INT NOT NULL)");
    failures += mylite_test_expect_int(
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
    failures +=
        mylite_test_expect_int((int)session->previous_row_count, 1, "later insert row count");
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize stale materialized cursor"
    );
    stmt = NULL;
    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_int(
        (int)session->previous_row_count,
        1,
        "preserve later insert row count"
    );

    failures += execute_ok(database, "INSERT INTO items VALUES (2), (3)");
    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'app'",
            strlen("SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'app'"),
            &stmt
        ),
        MYLITE_OK,
        "prepare materialized cursor before later found rows"
    );
    failures += mylite_test_expect_int(
        mylite_execute(
            database,
            "SELECT SQL_CALC_FOUND_ROWS id FROM items ORDER BY id LIMIT 1",
            strlen("SELECT SQL_CALC_FOUND_ROWS id FROM items ORDER BY id LIMIT 1"),
            &result
        ),
        MYLITE_OK,
        "execute later found rows statement"
    );
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), 1U, "later warning count");
    mylite_result_free(result);
    result = NULL;
    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_uint64(session->found_rows, 3U, "later found rows state");
    failures += mylite_test_expect_size(
        mylite_diagnostics_warning_total_count(&database->previous_diagnostics),
        1U,
        "later warning snapshot"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize after later found rows"
    );
    stmt = NULL;
    session = mylite_connection_session_state(database);
    failures +=
        mylite_test_expect_uint64(session->found_rows, 3U, "preserve later found rows state");
    failures += mylite_test_expect_size(
        mylite_diagnostics_warning_total_count(&database->previous_diagnostics),
        1U,
        "preserve later warning snapshot"
    );

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'app'",
            strlen("SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'app'"),
            &stmt
        ),
        MYLITE_OK,
        "prepare materialized cursor before later error"
    );
    failures += mylite_test_expect_true(
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
    failures +=
        mylite_test_expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize after later error");
    stmt = NULL;
    failures += mylite_test_expect_int(
        mylite_errcode(database),
        later_error_code,
        "preserve later error code"
    );
    failures += mylite_test_expect_text(
        mylite_errmsg(database),
        later_error,
        "preserve later error message"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_cursor_read_transaction_lifecycle(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_db *writer_database = NULL;
    mylite_stmt *constant_stmt = NULL;
    mylite_stmt *stmt = NULL;
    mylite_stmt *blocked_stmt = NULL;
    mylite_result *blocked_result = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "read_transaction") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open read transaction file"
    );
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE items (id INT NOT NULL)");
    failures += execute_ok(database, "INSERT INTO items VALUES (1), (2)");
    sqlite = mylite_connection_sqlite_for_test(database);
    failures +=
        mylite_test_expect_int(sqlite3_get_autocommit(sqlite), 1, "initial SQLite autocommit");

    failures += mylite_test_expect_int(
        mylite_prepare(database, "SELECT 7", strlen("SELECT 7"), &constant_stmt),
        MYLITE_OK,
        "prepare lazy constant cursor"
    );
    failures += mylite_test_expect_int(
        sqlite3_get_autocommit(sqlite),
        1,
        "constant prepare leaves SQLite autocommit active"
    );
    failures += execute_ok(database, "SET @after_constant_prepare = 1");
    failures += mylite_test_expect_int(
        mylite_stmt_step(constant_stmt),
        MYLITE_ROW,
        "lazy constant cursor first row"
    );
    failures += expect_cursor_text(constant_stmt, 0U, "7", "lazy constant cursor value");
    failures += mylite_test_expect_int(
        mylite_stmt_step(constant_stmt),
        MYLITE_DONE,
        "lazy constant cursor done"
    );
    failures += mylite_test_expect_int(
        sqlite3_get_autocommit(sqlite),
        1,
        "constant cursor exhaustion leaves SQLite autocommit active"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(constant_stmt),
        MYLITE_OK,
        "finalize lazy constant cursor"
    );
    constant_stmt = NULL;

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT id FROM items ORDER BY id",
            strlen("SELECT id FROM items ORDER BY id"),
            &stmt
        ),
        MYLITE_OK,
        "prepare read transaction cursor"
    );
    failures += mylite_test_expect_int(
        sqlite3_get_autocommit(sqlite),
        1,
        "table prepare leaves SQLite autocommit active"
    );
    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT id FROM items",
            strlen("SELECT id FROM items"),
            &blocked_stmt
        ),
        MYLITE_OK,
        "allow second prepare before first step"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(blocked_stmt),
        MYLITE_OK,
        "finalize second unexecuted cursor"
    );
    blocked_stmt = NULL;
    failures += execute_ok(database, "UPDATE items SET id = id");
    failures += mylite_test_expect_int(
        mylite_open(path, &writer_database),
        MYLITE_OK,
        "open writer during lazy prepare"
    );
    failures += execute_ok(writer_database, "USE app");
    failures += execute_ok(writer_database, "INSERT INTO items VALUES (3)");
    failures += execute_ok(writer_database, "ALTER TABLE items ADD COLUMN marker INT NULL");
    failures += mylite_test_expect_int(
        sqlite3_get_autocommit(sqlite),
        1,
        "concurrent writer leaves prepared reader idle"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "read transaction first row");
    failures += expect_cursor_text(stmt, 0U, "1", "read transaction first value");
    failures +=
        mylite_test_expect_int(sqlite3_get_autocommit(sqlite), 0, "cursor read transaction active");
    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT id FROM items",
            strlen("SELECT id FROM items"),
            &blocked_stmt
        ),
        MYLITE_ERROR,
        "reject second prepare after first step"
    );
    failures += mylite_test_expect_true(blocked_stmt == NULL, "rejected active cursor handle");
    failures += mylite_test_expect_int(
        mylite_errcode(database),
        mysql_error_commands_out_of_sync,
        "active cursor error code"
    );
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        "Commands out of sync",
        "active cursor error message"
    );
    failures += mylite_test_expect_int(
        mylite_execute(
            database,
            "UPDATE items SET id = id",
            strlen("UPDATE items SET id = id"),
            &blocked_result
        ),
        MYLITE_ERROR,
        "reject command during active cursor"
    );
    failures += mylite_test_expect_true(blocked_result == NULL, "rejected command result");
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "early finalize read transaction"
    );
    stmt = NULL;
    failures += mylite_test_expect_int(
        sqlite3_get_autocommit(sqlite),
        1,
        "early finalize ends transaction"
    );

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT id FROM items ORDER BY id",
            strlen("SELECT id FROM items ORDER BY id"),
            &stmt
        ),
        MYLITE_OK,
        "prepare exhausted read transaction cursor"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "exhausted cursor first row");
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "exhausted cursor second row");
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "exhausted cursor third row");
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "exhausted cursor done");
    failures +=
        mylite_test_expect_int(sqlite3_get_autocommit(sqlite), 1, "exhaustion ends transaction");
    failures +=
        mylite_test_expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize exhausted cursor");
    stmt = NULL;

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'app'",
            strlen("SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'app'"),
            &stmt
        ),
        MYLITE_OK,
        "prepare materialized read transaction cursor"
    );
    failures += mylite_test_expect_int(
        sqlite3_get_autocommit(sqlite),
        1,
        "materialized cursor releases read transaction"
    );
    failures += execute_ok(database, "SELECT id FROM items LIMIT 1");
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize materialized cursor"
    );
    stmt = NULL;

    failures += execute_ok(database, "START TRANSACTION");
    failures +=
        mylite_test_expect_int(sqlite3_get_autocommit(sqlite), 0, "user transaction active");
    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT id FROM items ORDER BY id",
            strlen("SELECT id FROM items ORDER BY id"),
            &stmt
        ),
        MYLITE_OK,
        "prepare cursor in user transaction"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize cursor in user transaction"
    );
    stmt = NULL;
    failures += mylite_test_expect_int(
        sqlite3_get_autocommit(sqlite),
        0,
        "cursor leaves user transaction active"
    );
    failures += execute_ok(database, "ROLLBACK");
    failures +=
        mylite_test_expect_int(sqlite3_get_autocommit(sqlite), 1, "rollback ends user transaction");

    failures += execute_ok(database, "SET autocommit = 0");
    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT id FROM items ORDER BY id",
            strlen("SELECT id FROM items ORDER BY id"),
            &stmt
        ),
        MYLITE_OK,
        "prepare autocommit-disabled cursor"
    );
    failures += mylite_test_expect_int(
        sqlite3_get_autocommit(sqlite),
        1,
        "autocommit-disabled prepare defers user transaction"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_ROW,
        "autocommit-disabled first step starts user transaction"
    );
    failures += mylite_test_expect_int(
        sqlite3_get_autocommit(sqlite),
        0,
        "autocommit-disabled execution owns user transaction"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize autocommit-disabled cursor"
    );
    stmt = NULL;
    failures += mylite_test_expect_int(
        sqlite3_get_autocommit(sqlite),
        0,
        "autocommit-disabled transaction remains active"
    );
    failures += execute_ok(database, "COMMIT");
    failures += mylite_test_expect_int(
        sqlite3_get_autocommit(sqlite),
        1,
        "commit ends autocommit-disabled transaction"
    );
    failures += execute_ok(database, "SET autocommit = 1");

    mylite_close(writer_database);
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_cursor_select_streams_rows_and_metadata(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "stream") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open cursor stream file");
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

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT id, name, note FROM items ORDER BY id",
            strlen("SELECT id, name, note FROM items ORDER BY id"),
            &stmt
        ),
        MYLITE_OK,
        "prepare cursor select"
    );
    failures += mylite_test_expect_true(stmt != NULL, "prepare returns statement handle");
    failures += mylite_test_expect_size(mylite_stmt_column_count(stmt), 3U, "cursor column count");
    failures +=
        mylite_test_expect_text(mylite_stmt_column_name(stmt, 0U), "id", "cursor id column name");
    failures += mylite_test_expect_text(
        mylite_stmt_column_name(stmt, 1U),
        "name",
        "cursor name column name"
    );
    failures += mylite_test_expect_text(
        mylite_stmt_column_name(stmt, 2U),
        "note",
        "cursor note column name"
    );
    failures += mylite_test_expect_text(
        mylite_stmt_column_schema_name(stmt, 0U),
        "app",
        "cursor id schema"
    );
    failures += mylite_test_expect_text(
        mylite_stmt_column_table_name(stmt, 0U),
        "items",
        "cursor id table"
    );
    failures += mylite_test_expect_text(
        mylite_stmt_column_origin_schema_name(stmt, 0U),
        "app",
        "cursor id origin schema"
    );
    failures += mylite_test_expect_text(
        mylite_stmt_column_origin_table_name(stmt, 0U),
        "items",
        "cursor id origin table"
    );
    failures += mylite_test_expect_text(
        mylite_stmt_column_origin_name(stmt, 0U),
        "id",
        "cursor id origin name"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_column_type(stmt, 0U),
        MYLITE_RESULT_COLUMN_TYPE_LONG,
        "cursor id type"
    );
    failures += mylite_test_expect_uint32(
        mylite_stmt_column_flags(stmt, 0U),
        MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT |
            MYLITE_RESULT_COLUMN_FLAG_NUM,
        "cursor id flags"
    );
    failures += mylite_test_expect_uint32(
        mylite_stmt_column_charset_id(stmt, 0U),
        mysql_collation_binary_id,
        "cursor id charset"
    );
    failures += mylite_test_expect_uint32(
        mylite_stmt_column_collation_id(stmt, 0U),
        mysql_collation_binary_id,
        "cursor id collation"
    );
    failures += mylite_test_expect_uint64(
        mylite_stmt_column_display_length(stmt, 0U),
        mysql_int_display_length,
        "cursor id display length"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_column_nullable(stmt, 0U), 0, "cursor id nullable");
    failures += mylite_test_expect_int(
        mylite_stmt_column_type(stmt, 1U),
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        "cursor name type"
    );
    failures += mylite_test_expect_uint32(
        mylite_stmt_column_collation_id(stmt, 1U),
        mysql_collation_utf8mb4_0900_ai_ci_id,
        "cursor name collation"
    );
    failures += mylite_test_expect_uint64(
        mylite_stmt_column_display_length(stmt, 1U),
        mysql_varchar_20_display_length,
        "cursor name display length"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_column_nullable(stmt, 1U), 1, "cursor name nullable");
    failures +=
        mylite_test_expect_int(mylite_stmt_column_decimals(stmt, 1U), 0, "cursor name decimals");
    failures += mylite_test_expect_true(
        mylite_stmt_column_name(stmt, 3U) == NULL,
        "cursor out-of-range column name"
    );
    failures += mylite_test_expect_true(
        mylite_stmt_value_text(stmt, 0U) == NULL,
        "cursor value before first row"
    );

    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "cursor first row step");
    failures += expect_cursor_text(stmt, 0U, "1", "cursor first row id");
    failures += expect_cursor_text(stmt, 1U, "alpha", "cursor first row name");
    failures += expect_cursor_text(stmt, 2U, "one", "cursor first row note");

    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "cursor second row step");
    failures += expect_cursor_text(stmt, 0U, "2", "cursor second row id");
    failures += expect_cursor_text(stmt, 1U, "beta", "cursor second row name");
    failures += expect_cursor_null(stmt, 2U, "cursor second row null note");

    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "cursor third row step");
    failures += expect_cursor_text(stmt, 0U, "3", "cursor third row id");
    failures += expect_cursor_text(stmt, 1U, "", "cursor third row empty name");
    failures += expect_cursor_text(stmt, 2U, "", "cursor third row empty note");

    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "cursor done step");
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "cursor repeated done step");
    failures +=
        mylite_test_expect_size(mylite_stmt_column_count(stmt), 3U, "cursor metadata after done");
    failures += mylite_test_expect_true(
        mylite_stmt_value_text(stmt, 0U) == NULL,
        "cursor value after done"
    );
    failures += mylite_test_expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize cursor");
    stmt = NULL;
    failures +=
        mylite_test_expect_int(mylite_stmt_finalize(NULL), MYLITE_OK, "finalize null cursor");

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

    if (mylite_test_make_path(path, sizeof(path), "borrowed_columns") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open borrowed columns file"
    );
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(
        database,
        "CREATE TABLE items (id INT NOT NULL PRIMARY KEY, name VARCHAR(20), INDEX idx_name (name))"
    );
    failures += execute_ok(database, "INSERT INTO items VALUES (1, 'alpha')");
    failures += mylite_test_expect_int(
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
    failures +=
        mylite_test_expect_true(borrowed_entry != NULL, "cursor pins table columns cache entry");
    failures +=
        mylite_test_expect_true(borrowed_key_entry != NULL, "cursor pins table key cache entry");
    if (borrowed_entry != NULL) {
        failures += mylite_test_expect_size(
            borrowed_entry->reference_count,
            1U,
            "borrowed column reference"
        );
    }
    if (borrowed_key_entry != NULL) {
        failures += mylite_test_expect_size(
            borrowed_key_entry->reference_count,
            1U,
            "borrowed key metadata reference"
        );
    }
    mylite_catalog_invalidate_descriptor_cache(database);
    if (borrowed_entry != NULL) {
        failures +=
            mylite_test_expect_true(!borrowed_entry->is_valid, "invalidated borrowed column entry");
        failures += mylite_test_expect_true(
            borrowed_entry->columns != NULL,
            "borrowed columns remain allocated"
        );
    }
    if (borrowed_key_entry != NULL) {
        failures += mylite_test_expect_true(
            !borrowed_key_entry->is_valid,
            "invalidated borrowed key entry"
        );
        failures += mylite_test_expect_true(
            borrowed_key_entry->metadata.primary_key.parts != NULL,
            "borrowed primary key metadata remains allocated"
        );
        failures += mylite_test_expect_true(
            borrowed_key_entry->metadata.indexes != NULL,
            "borrowed secondary key metadata remains allocated"
        );
    }
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "borrowed columns cursor row");
    failures += expect_cursor_text(stmt, 0U, "alpha", "borrowed columns cursor value");
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize borrowed columns cursor"
    );
    stmt = NULL;
    if (borrowed_entry != NULL) {
        failures += mylite_test_expect_size(
            borrowed_entry->reference_count,
            0U,
            "released column reference"
        );
        failures += mylite_test_expect_true(
            borrowed_entry->columns == NULL,
            "released invalid column entry"
        );
    }
    if (borrowed_key_entry != NULL) {
        failures += mylite_test_expect_size(
            borrowed_key_entry->reference_count,
            0U,
            "released key metadata reference"
        );
        failures += mylite_test_expect_true(
            borrowed_key_entry->metadata.primary_key.parts == NULL,
            "released invalid primary key metadata"
        );
        failures += mylite_test_expect_true(
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

    if (mylite_test_make_path(path, sizeof(path), "key_metadata_lru") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open key metadata LRU file"
    );
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    for (size_t index = 0U; index <= MYLITE_EXECUTION_TABLE_KEY_METADATA_CACHE_LIMIT; ++index) {
        int written = snprintf(
            sql,
            sizeof(sql),
            "CREATE TABLE table_%zu (id INT NOT NULL PRIMARY KEY)",
            index
        );

        failures += mylite_test_expect_true(
            written > 0 && (size_t)written < sizeof(sql),
            "format LRU table"
        );
        if (written > 0 && (size_t)written < sizeof(sql)) {
            failures += execute_ok(database, sql);
        }
    }
    failures += mylite_test_expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read LRU schema"
    );
    failures += mylite_test_expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "table_0", &first_table),
        MYLITE_OK,
        "read first LRU table"
    );
    failures += mylite_test_expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "table_1", &second_table),
        MYLITE_OK,
        "read second LRU table"
    );
    failures += mylite_test_expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "table_64", &last_table),
        MYLITE_OK,
        "read last LRU table"
    );

    for (size_t index = 0U; index < MYLITE_EXECUTION_TABLE_KEY_METADATA_CACHE_LIMIT; ++index) {
        int written = snprintf(sql, sizeof(sql), "SELECT id FROM table_%zu", index);

        failures += mylite_test_expect_true(
            written > 0 && (size_t)written < sizeof(sql),
            "format LRU select"
        );
        if (written > 0 && (size_t)written < sizeof(sql)) {
            failures += mylite_test_expect_int(
                mylite_prepare(database, sql, (size_t)written, &stmt),
                MYLITE_OK,
                "prepare LRU select"
            );
            failures += mylite_test_expect_int(
                mylite_stmt_finalize(stmt),
                MYLITE_OK,
                "finalize LRU select"
            );
            stmt = NULL;
        }
    }
    failures += mylite_test_expect_size(
        database->table_key_metadata_cache_count,
        MYLITE_EXECUTION_TABLE_KEY_METADATA_CACHE_LIMIT,
        "filled key metadata cache"
    );
    failures += mylite_test_expect_int(
        mylite_prepare(database, "SELECT id FROM table_0", strlen("SELECT id FROM table_0"), &stmt),
        MYLITE_OK,
        "refresh hot LRU entry"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize hot LRU select");
    stmt = NULL;
    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT id FROM table_64",
            strlen("SELECT id FROM table_64"),
            &stmt
        ),
        MYLITE_OK,
        "prepare replacement LRU select"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize replacement LRU select"
    );
    stmt = NULL;

    failures += mylite_test_expect_true(
        (int)key_metadata_cache_contains_table(database, first_table.table_id),
        "hot key metadata entry survives"
    );
    failures += mylite_test_expect_true(
        !key_metadata_cache_contains_table(database, second_table.table_id),
        "oldest key metadata entry evicted"
    );
    failures += mylite_test_expect_true(
        (int)key_metadata_cache_contains_table(database, last_table.table_id),
        "replacement key metadata entry cached"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_metadata_caches_enforce_byte_budgets(void) {
    char path[test_path_capacity];
    char sql[cache_budget_sql_capacity];
    mylite_db *database = NULL;
    size_t column_cache_bytes = 0U;
    size_t key_cache_bytes = 0U;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "metadata_cache_budget") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open cache budget file");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += create_cache_budget_tables(database, sql, sizeof(sql));
    failures += prepare_cache_budget_selects(database, sql, sizeof(sql));

    for (size_t index = 0U; index < database->table_columns_cache_count; ++index) {
        column_cache_bytes += database->table_columns_cache[index].byte_count;
    }
    for (size_t index = 0U; index < database->table_key_metadata_cache_count; ++index) {
        key_cache_bytes += database->table_key_metadata_cache[index].byte_count;
    }
    failures +=
        mylite_test_expect_true(column_cache_bytes > 0U, "column cache retains budgeted entries");
    failures += mylite_test_expect_true(key_cache_bytes > 0U, "key cache retains budgeted entries");
    failures += mylite_test_expect_true(
        column_cache_bytes <= MYLITE_EXECUTION_TABLE_COLUMNS_CACHE_BYTE_LIMIT,
        "column cache byte budget"
    );
    failures += mylite_test_expect_true(
        key_cache_bytes <= MYLITE_EXECUTION_TABLE_KEY_METADATA_CACHE_BYTE_LIMIT,
        "key cache byte budget"
    );
    failures += mylite_test_expect_true(
        database->table_columns_cache_count == cache_budget_table_count,
        "compact column metadata retains every budgeted table"
    );
    failures += mylite_test_expect_true(
        database->table_key_metadata_cache_count == cache_budget_table_count,
        "compact key metadata retains every budgeted table"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int create_cache_budget_tables(mylite_db *database, char *sql, size_t sql_capacity) {
    int failures = 0;

    for (size_t table_index = 0U; table_index < cache_budget_table_count; ++table_index) {
        int written = snprintf(sql, sql_capacity, "CREATE TABLE budget_%zu (", table_index);
        size_t length = written > 0 ? (size_t)written : sql_capacity;

        if (!append_cache_budget_column_list(sql, sql_capacity, &length, true) ||
            length >= sql_capacity) {
            length = sql_capacity;
        }
        if (length < sql_capacity) {
            written = snprintf(sql + length, sql_capacity - length, ",PRIMARY KEY (");
            length = written > 0 && (size_t)written < sql_capacity - length
                         ? length + (size_t)written
                         : sql_capacity;
        }
        if (!append_cache_budget_column_list(sql, sql_capacity, &length, false) ||
            length >= sql_capacity) {
            length = sql_capacity;
        }
        if (length < sql_capacity) {
            written = snprintf(sql + length, sql_capacity - length, "))");
            length = written > 0 && (size_t)written < sql_capacity - length
                         ? length + (size_t)written
                         : sql_capacity;
        }
        failures += mylite_test_expect_true(length < sql_capacity, "format cache budget table");
        if (length < sql_capacity) {
            failures += execute_ok(database, sql);
        }
    }
    return failures;
}

static bool append_cache_budget_column_list(
    char *sql,
    size_t sql_capacity,
    size_t *length,
    bool include_types
) {
    for (size_t index = 0U; *length < sql_capacity && index < cache_budget_column_count; ++index) {
        int written = snprintf(
            sql + *length,
            sql_capacity - *length,
            include_types ? "%sc%zu INT NOT NULL" : "%sc%zu",
            index == 0U ? "" : ",",
            index
        );

        if (written <= 0 || (size_t)written >= sql_capacity - *length) {
            return false;
        }
        *length += (size_t)written;
    }
    return *length < sql_capacity;
}

static int prepare_cache_budget_selects(mylite_db *database, char *sql, size_t sql_capacity) {
    mylite_stmt *stmt = NULL;
    int failures = 0;

    for (size_t table_index = 0U; table_index < cache_budget_table_count; ++table_index) {
        int written =
            snprintf(sql, sql_capacity, "SELECT c0 FROM budget_%zu WHERE c0 = 0", table_index);

        failures += mylite_test_expect_true(
            written > 0 && (size_t)written < sql_capacity,
            "format cache budget select"
        );
        if (written > 0 && (size_t)written < sql_capacity) {
            failures += mylite_test_expect_int(
                mylite_prepare(database, sql, (size_t)written, &stmt),
                MYLITE_OK,
                "prepare cache budget select"
            );
            failures += mylite_test_expect_int(
                mylite_stmt_finalize(stmt),
                MYLITE_OK,
                "finalize budget select"
            );
            stmt = NULL;
        }
    }
    return failures;
}

static int test_cursor_reuses_finalized_select_statements(void) {
    char path[test_path_capacity];
    const char query[] = "SELECT name FROM items WHERE id = 1";
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "reuse") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open cursor reuse file");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE items (id INT NOT NULL, name VARCHAR(20))");
    failures += execute_ok(database, "INSERT INTO items VALUES (1, 'alpha')");

    failures += mylite_test_expect_int(
        mylite_prepare(database, query, strlen(query), &stmt),
        MYLITE_OK,
        "prepare first cached cursor"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "first cached cursor row");
    failures += expect_cursor_text(stmt, 0U, "alpha", "first cached cursor value");
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "first cached cursor done");
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize first cached cursor"
    );
    stmt = NULL;

    failures += mylite_test_expect_int(
        mylite_prepare(database, query, strlen(query), &stmt),
        MYLITE_OK,
        "prepare reused cached cursor"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "reused cached cursor row");
    failures += expect_cursor_text(stmt, 0U, "alpha", "reused cached cursor value");
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize reused cached cursor"
    );
    stmt = NULL;

    failures += execute_ok(database, "ALTER TABLE items ADD COLUMN marker INT NULL");
    failures += execute_ok(database, "UPDATE items SET name = 'beta', marker = 2 WHERE id = 1");
    failures += mylite_test_expect_int(
        mylite_prepare(database, query, strlen(query), &stmt),
        MYLITE_OK,
        "prepare cursor after schema change"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "schema changed cursor row");
    failures += expect_cursor_text(stmt, 0U, "beta", "schema changed cursor value");
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize schema changed cursor"
    );
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

    if (mylite_test_make_path(path, sizeof(path), "reset_nullability") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open reset file");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(
        database,
        "CREATE TABLE items (nullable_value VARCHAR(8), empty_value VARCHAR(8))"
    );
    failures += execute_ok(database, "INSERT INTO items VALUES (NULL, '')");
    failures += mylite_test_expect_int(
        mylite_prepare(database, query, strlen(query), &stmt),
        MYLITE_OK,
        "prepare reset cursor"
    );
    failures +=
        mylite_test_expect_size(mylite_stmt_parameter_count(stmt), 0U, "reset parameter count");
    failures += mylite_test_expect_true(
        mylite_stmt_affected_rows(stmt) == -1,
        "reset affected rows default"
    );
    failures +=
        mylite_test_expect_uint64(mylite_stmt_insert_id(stmt), 0U, "reset insert id default");
    failures += mylite_test_expect_int(
        mylite_stmt_bind_null(stmt, 0U),
        MYLITE_MISUSE,
        "reject nonexistent parameter"
    );
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "reset first row");
    failures += mylite_test_expect_true(mylite_stmt_value_is_null(stmt, 0U), "cursor NULL value");
    failures += mylite_test_expect_true(
        !mylite_stmt_value_is_null(stmt, 1U),
        "cursor empty value is not NULL"
    );
    failures +=
        mylite_test_expect_size(mylite_stmt_value_size(stmt, 1U), 0U, "cursor empty value size");
    failures += mylite_test_expect_true(
        mylite_stmt_value_bytes(stmt, 1U) != NULL,
        "cursor empty value has storage"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_clear_bindings(stmt),
        MYLITE_MISUSE,
        "reject clearing bindings while row is active"
    );
    failures += mylite_test_expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset active cursor");
    failures +=
        mylite_test_expect_true(mylite_stmt_value_is_null(stmt, 0U), "reset clears current row");
    failures +=
        mylite_test_expect_int(mylite_stmt_clear_bindings(stmt), MYLITE_OK, "clear zero bindings");
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "reset repeated row");
    failures +=
        mylite_test_expect_true(mylite_stmt_value_is_null(stmt, 0U), "repeated cursor NULL value");
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "reset repeated done");
    failures +=
        mylite_test_expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset exhausted cursor");
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "exhausted reset row");
    failures +=
        mylite_test_expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize reset cursor");
    stmt = NULL;

    failures += mylite_test_expect_int(
        mylite_execute(database, query, strlen(query), &result),
        MYLITE_OK,
        "materialize nullable result"
    );
    failures +=
        mylite_test_expect_true(mylite_result_value_is_null(result, 0U, 0U), "result NULL value");
    failures += mylite_test_expect_true(
        !mylite_result_value_is_null(result, 0U, 1U),
        "result empty value is not NULL"
    );
    failures += mylite_test_expect_size(
        mylite_result_value_size(result, 0U, 1U),
        0U,
        "result empty value size"
    );
    failures += mylite_test_expect_true(
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
    static const struct native_prepared_binding concat_bindings[] = {
        {.kind = NATIVE_PREPARED_BINDING_TEXT, .text = "a"},
        {.kind = NATIVE_PREPARED_BINDING_TEXT, .text = "b"},
        {.kind = NATIVE_PREPARED_BINDING_TEXT, .text = "c"},
    };
    static const struct native_prepared_binding concat_ws_bindings[] = {
        {.kind = NATIVE_PREPARED_BINDING_TEXT, .text = "left"},
        {.kind = NATIVE_PREPARED_BINDING_TEXT, .text = "right"},
    };
    static const struct native_prepared_binding convert_bindings[] = {
        {.kind = NATIVE_PREPARED_BINDING_TEXT, .text = "converted"},
    };
    static const struct native_prepared_binding least_bindings[] = {
        {.kind = NATIVE_PREPARED_BINDING_INT64, .integer = 9},
        {.kind = NATIVE_PREPARED_BINDING_INT64, .integer = 4},
    };
    static const struct native_prepared_binding count_bindings[] = {
        {.kind = NATIVE_PREPARED_BINDING_INT64, .integer = 3},
    };
    static const char *const concat_expected[] = {"abc"};
    static const char *const concat_ws_expected[] = {"left, right"};
    static const char *const convert_expected[] = {"converted"};
    static const char *const least_expected[] = {"4"};
    static const char *const count_expected[] = {"5"};
    mylite_db *database = NULL;
    mylite_db *other_database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "native_bindings") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open native bindings file"
    );
    failures += execute_ok(database, "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE items (id INT NOT NULL, name VARCHAR(20))");
    failures += execute_ok(database, "INSERT INTO items VALUES (1, 'alpha'), (2, 'beta')");
    failures += mylite_test_expect_int(
        mylite_prepare(database, "SELECT ? AS value", strlen("SELECT ? AS value"), &stmt),
        MYLITE_OK,
        "prepare native scalar parameter"
    );
    failures +=
        mylite_test_expect_size(mylite_stmt_parameter_count(stmt), 1U, "native parameter count");
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_MISUSE,
        "native missing binding fails before execution"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 0U, native_signed_binding_value),
        MYLITE_OK,
        "bind native signed integer"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native signed integer row");
    failures += expect_cursor_text(stmt, 0U, "-42", "native signed integer value");
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 0U, native_rebind_value),
        MYLITE_MISUSE,
        "reject native rebind while row is active"
    );

    failures +=
        mylite_test_expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset native text binding");
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 0U, injection_text, strlen(injection_text)),
        MYLITE_OK,
        "bind native injection text"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native injection text row");
    failures += expect_cursor_text(stmt, 0U, injection_text, "native injection text value");

    failures +=
        mylite_test_expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset native blob binding");
    failures += mylite_test_expect_int(
        mylite_stmt_bind_blob(stmt, 0U, blob, sizeof(blob)),
        MYLITE_OK,
        "bind native embedded NUL blob"
    );
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native blob row");
    failures +=
        mylite_test_expect_true(!mylite_stmt_value_is_null(stmt, 0U), "native blob is not NULL");
    failures +=
        mylite_test_expect_size(mylite_stmt_value_size(stmt, 0U), sizeof(blob), "native blob size");
    failures += mylite_test_expect_true(
        mylite_stmt_value_bytes(stmt, 0U) != NULL &&
            memcmp(mylite_stmt_value_bytes(stmt, 0U), blob, sizeof(blob)) == 0,
        "native blob bytes"
    );

    failures +=
        mylite_test_expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset native NULL binding");
    failures +=
        mylite_test_expect_int(mylite_stmt_bind_null(stmt, 0U), MYLITE_OK, "bind native NULL");
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native NULL row");
    failures += mylite_test_expect_true(mylite_stmt_value_is_null(stmt, 0U), "native NULL value");

    failures +=
        mylite_test_expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset native unsigned binding");
    failures += mylite_test_expect_int(
        mylite_stmt_bind_uint64(stmt, 0U, UINT64_MAX),
        MYLITE_OK,
        "bind native maximum unsigned integer"
    );
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native unsigned row");
    failures +=
        expect_cursor_text(stmt, 0U, "18446744073709551615", "native maximum unsigned value");

    failures +=
        mylite_test_expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset native double binding");
    failures += mylite_test_expect_int(
        mylite_stmt_bind_double(stmt, 0U, native_double_binding_value),
        MYLITE_OK,
        "bind native double"
    );
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native double row");
    failures += expect_cursor_text(stmt, 0U, "1.25", "native double value");

    failures +=
        mylite_test_expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset native empty text");
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 0U, NULL, 0U),
        MYLITE_OK,
        "bind native empty text"
    );
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native empty text row");
    failures += mylite_test_expect_true(
        !mylite_stmt_value_is_null(stmt, 0U),
        "native empty text is not NULL"
    );
    failures +=
        mylite_test_expect_size(mylite_stmt_value_size(stmt, 0U), 0U, "native empty text size");

    failures +=
        mylite_test_expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset native cleared binding");
    failures +=
        mylite_test_expect_int(mylite_stmt_clear_bindings(stmt), MYLITE_OK, "clear native binding");
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_MISUSE,
        "cleared native binding fails before execution"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 1U, "bad", 3U),
        MYLITE_MISUSE,
        "reject native binding index"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_blob(stmt, 0U, NULL, 1U),
        MYLITE_MISUSE,
        "reject native nonempty NULL blob pointer"
    );

    failures +=
        mylite_test_expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize native binding");
    stmt = NULL;

    failures += execute_ok(
        database,
        "CREATE TABLE config (collection VARCHAR(255), name VARCHAR(255), data BLOB, "
        "PRIMARY KEY (collection, name))"
    );
    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "INSERT INTO config (collection, name, data) VALUES (?, ?, ?)",
            strlen("INSERT INTO config (collection, name, data) VALUES (?, ?, ?)"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native empty string key insert"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 0U, NULL, 0U),
        MYLITE_OK,
        "bind native empty string key"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 1U, "core.extension", strlen("core.extension")),
        MYLITE_OK,
        "bind native string key name"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 2U, "serialized", strlen("serialized")),
        MYLITE_OK,
        "bind native string key payload"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_DONE,
        "execute native empty string key insert"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize native empty string key insert"
    );
    stmt = NULL;
    failures += expect_query_scalar_text(
        database,
        (struct expected_query_scalar_text){
            .sql = "SELECT COUNT(*) FROM config WHERE collection = ''",
            .expected = "1",
            .context = "native empty string key inserted row",
        }
    );

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT '?' AS string_marker, \"?\" AS double_string_marker, "
            "1 AS `?`, ? AS bound_value /* ? */ -- ?\n",
            strlen("SELECT '?' AS string_marker, \"?\" AS double_string_marker, "
                   "1 AS `?`, ? AS bound_value /* ? */ -- ?\n"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native quoted and commented markers"
    );
    failures += mylite_test_expect_size(
        mylite_stmt_parameter_count(stmt),
        1U,
        "quoted and commented marker count"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 0U, classified_marker_value),
        MYLITE_OK,
        "bind classified marker"
    );
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "classified marker row");
    failures += expect_cursor_text(stmt, 0U, "?", "quoted string marker value");
    failures += expect_cursor_text(stmt, 1U, "?", "double-quoted string marker value");
    failures += expect_cursor_text(stmt, 2U, "1", "quoted identifier marker value");
    failures += expect_cursor_text(stmt, 3U, "31", "classified bound marker value");
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize classified markers"
    );
    stmt = NULL;

    failures += execute_ok(database, "SET SESSION sql_mode = 'ANSI_QUOTES'");
    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT 1 AS \"?\", ? AS bound_value",
            strlen("SELECT 1 AS \"?\", ? AS bound_value"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native ANSI_QUOTES marker classification"
    );
    failures +=
        mylite_test_expect_size(mylite_stmt_parameter_count(stmt), 1U, "ANSI_QUOTES marker count");
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 0U, ansi_marker_value),
        MYLITE_OK,
        "bind ANSI_QUOTES marker"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "ANSI_QUOTES marker row");
    failures += expect_cursor_text(stmt, 0U, "1", "ANSI_QUOTES identifier marker value");
    failures += expect_cursor_text(stmt, 1U, "32", "ANSI_QUOTES bound marker value");
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize ANSI_QUOTES markers"
    );
    stmt = NULL;

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT /*!80000 ? AS executable_value, */ ? AS ordinary_value "
            "/*!99999 , ? AS skipped_value */",
            strlen("SELECT /*!80000 ? AS executable_value, */ ? AS ordinary_value "
                   "/*!99999 , ? AS skipped_value */"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native executable-comment markers"
    );
    failures += mylite_test_expect_size(
        mylite_stmt_parameter_count(stmt),
        2U,
        "version-gated executable marker count"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 0U, executable_marker_value),
        MYLITE_OK,
        "bind executable-comment marker"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 1U, ordinary_marker_value),
        MYLITE_OK,
        "bind ordinary marker after executable comment"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "executable-comment marker row");
    failures += expect_cursor_text(stmt, 0U, "41", "executable-comment marker value");
    failures += expect_cursor_text(stmt, 1U, "42", "ordinary marker value");
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize executable-comment markers"
    );
    stmt = NULL;

    failures += mylite_test_expect_int(
        mylite_prepare(database, "SELECT * FROM ?", strlen("SELECT * FROM ?"), &stmt),
        MYLITE_ERROR,
        "reject native identifier marker"
    );
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        "syntax",
        "native identifier marker diagnostic"
    );
    (void)mylite_stmt_finalize(stmt);
    stmt = NULL;

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT name FROM items WHERE id = ? ORDER BY id LIMIT ? OFFSET ?",
            strlen("SELECT name FROM items WHERE id = ? ORDER BY id LIMIT ? OFFSET ?"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native table query"
    );
    failures += mylite_test_expect_size(
        mylite_stmt_parameter_count(stmt),
        3U,
        "native table parameter count"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 0U, 2),
        MYLITE_OK,
        "bind native row id"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_uint64(stmt, 1U, 1U),
        MYLITE_OK,
        "bind native limit"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_uint64(stmt, 2U, 0U),
        MYLITE_OK,
        "bind native offset"
    );
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native table row");
    failures += expect_cursor_text(stmt, 0U, "beta", "native table value");
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "native table done");
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize native table query"
    );
    stmt = NULL;

    failures += expect_native_prepared_row(
        database,
        "SELECT CONCAT(?, CONCAT(?, ?))",
        concat_bindings,
        sizeof(concat_bindings) / sizeof(concat_bindings[0]),
        concat_expected,
        sizeof(concat_expected) / sizeof(concat_expected[0]),
        "native nested CONCAT parameters"
    );
    failures += expect_native_prepared_row(
        database,
        "SELECT CONCAT_WS(', ', ?, NULL, ?)",
        concat_ws_bindings,
        sizeof(concat_ws_bindings) / sizeof(concat_ws_bindings[0]),
        concat_ws_expected,
        sizeof(concat_ws_expected) / sizeof(concat_ws_expected[0]),
        "native CONCAT_WS parameters"
    );
    failures += expect_native_prepared_row(
        database,
        "SELECT CONVERT(? USING utf8mb4) FROM items LIMIT 1",
        convert_bindings,
        sizeof(convert_bindings) / sizeof(convert_bindings[0]),
        convert_expected,
        sizeof(convert_expected) / sizeof(convert_expected[0]),
        "native table-backed CONVERT parameter"
    );
    failures += expect_native_prepared_row(
        database,
        "SELECT LEAST(?, ?)",
        least_bindings,
        sizeof(least_bindings) / sizeof(least_bindings[0]),
        least_expected,
        sizeof(least_expected) / sizeof(least_expected[0]),
        "native LEAST parameters"
    );
    failures += expect_native_prepared_row(
        database,
        "SELECT COUNT(*) + ? FROM items",
        count_bindings,
        sizeof(count_bindings) / sizeof(count_bindings[0]),
        count_expected,
        sizeof(count_expected) / sizeof(count_expected[0]),
        "native COUNT arithmetic parameter"
    );

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT COUNT(*) FROM items WHERE name REGEXP ? AND id BETWEEN ? AND ?",
            strlen("SELECT COUNT(*) FROM items WHERE name REGEXP ? AND id BETWEEN ? AND ?"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native REGEXP and BETWEEN parameters"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 0U, "a$", 2U),
        MYLITE_OK,
        "bind native REGEXP parameter"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 1U, 1),
        MYLITE_OK,
        "bind native BETWEEN lower"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 2U, 2),
        MYLITE_OK,
        "bind native BETWEEN upper"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_ROW,
        "native predicate parameter row"
    );
    failures += expect_cursor_text(stmt, 0U, "2", "native REGEXP and BETWEEN value");
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_DONE,
        "native predicate parameter done"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize native REGEXP and BETWEEN query"
    );
    stmt = NULL;

    failures += execute_ok(database, "CREATE TABLE decimal_items (value DECIMAL(10, 2))");
    failures += execute_ok(database, "INSERT INTO decimal_items VALUES (1), (1.00)");
    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT COUNT(*) FROM decimal_items WHERE value <> ?",
            strlen("SELECT COUNT(*) FROM decimal_items WHERE value <> ?"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native decimal predicate"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 0U, 1),
        MYLITE_OK,
        "bind native decimal predicate"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native decimal predicate row");
    failures += expect_cursor_text(stmt, 0U, "0", "native decimal predicate canonical value");
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_DONE,
        "native decimal predicate done"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize native decimal predicate"
    );
    stmt = NULL;

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT ? AS a_value, ? AS b_value",
            strlen("SELECT ? AS a_value, ? AS b_value"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native parameter order query"
    );
    failures +=
        mylite_test_expect_size(mylite_stmt_parameter_count(stmt), 2U, "native ordered parameters");
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 0U, ordered_first_value),
        MYLITE_OK,
        "bind native first slot"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 1U, ordered_second_value),
        MYLITE_OK,
        "bind native second slot"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "native ordered parameter row");
    failures += expect_cursor_text(stmt, 0U, "11", "native first parameter order");
    failures += expect_cursor_text(stmt, 1U, "22", "native second parameter order");
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_DONE,
        "native ordered parameter done"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize native order query"
    );
    stmt = NULL;

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT name FROM items WHERE id = ?",
            strlen("SELECT name FROM items WHERE id = ?"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native schema-reprepare query"
    );
    failures += mylite_test_expect_int(
        mylite_open(path, &other_database),
        MYLITE_OK,
        "open native DDL handle"
    );
    failures += execute_ok(other_database, "USE app");
    failures += execute_ok(other_database, "ALTER TABLE items ADD COLUMN note VARCHAR(20)");
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 0U, 2),
        MYLITE_OK,
        "bind native query after compatible DDL"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_ROW,
        "native query reparses plan after DDL"
    );
    failures += expect_cursor_text(stmt, 0U, "beta", "native query value after compatible DDL");
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "native query done after DDL");
    failures += mylite_test_expect_int(
        mylite_stmt_reset(stmt),
        MYLITE_OK,
        "reset native query before rename"
    );
    failures += execute_ok(other_database, "ALTER TABLE items RENAME COLUMN name TO label");
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_ERROR,
        "native query reports incompatible schema change"
    );
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        "Unknown column",
        "native reprepare schema diagnostic"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset failed native reprepare");
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize native reprepare query"
    );
    mylite_close(other_database);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int expect_native_prepared_row(
    mylite_db *database,
    const char *sql,
    const struct native_prepared_binding *bindings,
    size_t binding_count,
    const char *const *expected_values,
    size_t expected_value_count,
    const char *context
) {
    mylite_stmt *stmt = NULL;
    int failures = 0;
    int rc = mylite_prepare(database, sql, strlen(sql), &stmt);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s prepare: %s\n", context, mylite_errmsg(database));
        return 1;
    }
    failures += mylite_test_expect_size(mylite_stmt_parameter_count(stmt), binding_count, context);
    for (size_t index = 0U; index < binding_count; ++index) {
        if (bindings[index].kind == NATIVE_PREPARED_BINDING_TEXT) {
            rc = mylite_stmt_bind_text(
                stmt,
                index,
                bindings[index].text,
                strlen(bindings[index].text)
            );
        } else {
            rc = mylite_stmt_bind_int64(stmt, index, bindings[index].integer);
        }
        failures += mylite_test_expect_int(rc, MYLITE_OK, context);
    }
    rc = mylite_stmt_step(stmt);
    if (rc != MYLITE_ROW) {
        fprintf(stderr, "%s step: %s\n", context, mylite_errmsg(database));
        ++failures;
    } else {
        failures +=
            mylite_test_expect_size(mylite_stmt_column_count(stmt), expected_value_count, context);
        for (size_t index = 0U; index < expected_value_count; ++index) {
            failures += expect_cursor_text(stmt, index, expected_values[index], context);
        }
        failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, context);
    }
    failures += mylite_test_expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, context);
    return failures;
}

static int test_native_prepared_owns_resolution_context(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = mylite_test_make_path(path, sizeof(path), "native_resolution_context");

    if (failures != 0) {
        return failures;
    }
    remove_related_files(path);
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open native resolution context file"
    );
    failures += execute_ok(database, "CREATE DATABASE schema_a");
    failures += execute_ok(database, "CREATE DATABASE schema_b");
    failures += execute_ok(database, "USE schema_a");
    failures += execute_ok(database, "CREATE TABLE items (id INT NOT NULL, value VARCHAR(20))");
    failures += execute_ok(database, "INSERT INTO items VALUES (1, 'from-a')");
    failures += execute_ok(database, "USE schema_b");
    failures += execute_ok(database, "CREATE TABLE items (id INT NOT NULL, value VARCHAR(20))");
    failures += execute_ok(database, "INSERT INTO items VALUES (1, 'from-b')");
    failures += execute_ok(database, "USE schema_a");
    failures += execute_ok(database, "SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci");
    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT value FROM items WHERE id = ?",
            strlen("SELECT value FROM items WHERE id = ?"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native statement under schema A"
    );
    failures += execute_ok(database, "USE schema_b");
    failures += execute_ok(database, "SET NAMES latin1 COLLATE latin1_swedish_ci");
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 0U, 1),
        MYLITE_OK,
        "bind native statement after context change"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_ROW,
        "execute native statement after context change"
    );
    failures += expect_cursor_text(stmt, 0U, "from-a", "prepared schema remains schema A");
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "prepared schema result done");
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize schema context query"
    );
    stmt = NULL;
    failures += expect_query_scalar_text(
        database,
        (struct expected_query_scalar_text){
            .sql = "SELECT DATABASE()",
            .expected = "schema_b",
            .context = "native execution restores live selected schema",
        }
    );

    failures += execute_ok(database, "USE schema_a");
    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "UPDATE items SET value = ? WHERE id = ?",
            strlen("UPDATE items SET value = ? WHERE id = ?"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native DML under schema A"
    );
    failures += execute_ok(database, "USE schema_b");
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 0U, "updated-a", strlen("updated-a")),
        MYLITE_OK,
        "bind native DML value after schema change"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 1U, 1),
        MYLITE_OK,
        "bind native DML key after schema change"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_DONE,
        "execute native DML after schema change"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize schema context DML"
    );
    stmt = NULL;
    failures += expect_query_scalar_text(
        database,
        (struct expected_query_scalar_text){
            .sql = "SELECT value FROM items WHERE id = 1",
            .expected = "from-b",
            .context = "native DML leaves live schema B unchanged",
        }
    );
    failures += execute_ok(database, "USE schema_a");
    failures += expect_query_scalar_text(
        database,
        (struct expected_query_scalar_text){
            .sql = "SELECT value FROM items WHERE id = 1",
            .expected = "updated-a",
            .context = "native DML resolves against prepared schema A",
        }
    );
    failures += execute_ok(database, "USE schema_b");

    failures += execute_ok(database, "SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci");
    failures += mylite_test_expect_int(
        mylite_prepare_buffered(
            database,
            "SELECT MD5('abc') AS prepared_text",
            strlen("SELECT MD5('abc') AS prepared_text"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native statement under utf8mb4"
    );
    failures += execute_ok(database, "SET NAMES latin1 COLLATE latin1_swedish_ci");
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_ROW,
        "execute native charset context query"
    );
    failures += expect_cursor_text(
        stmt,
        0U,
        "900150983cd24fb0d6963f7d28e17f72",
        "native charset context value"
    );
    failures += mylite_test_expect_uint32(
        mylite_stmt_column_charset_id(stmt, 0U),
        mysql_collation_utf8mb4_0900_ai_ci_id,
        "native statement retains prepare-time charset"
    );
    failures += mylite_test_expect_uint32(
        mylite_stmt_column_collation_id(stmt, 0U),
        mysql_collation_utf8mb4_0900_ai_ci_id,
        "native statement retains prepare-time collation"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize charset context query"
    );

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

    if (mylite_test_make_path(path, sizeof(path), "native_sql_lifetime") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open SQL lifetime file");
    caller_sql = malloc(sizeof(prepared_sql));
    if (caller_sql == NULL) {
        mylite_close(database);
        remove_related_files(path);
        return failures + 1;
    }
    memcpy(caller_sql, prepared_sql, sizeof(prepared_sql));
    failures += mylite_test_expect_int(
        mylite_prepare(database, caller_sql, sizeof(prepared_sql) - 1U, &stmt),
        MYLITE_OK,
        "prepare from caller-owned SQL"
    );
    memset(caller_sql, 'x', sizeof(prepared_sql) - 1U);
    free(caller_sql);
    caller_sql = NULL;

    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 0U, retained_sql_value),
        MYLITE_OK,
        "bind retained SQL"
    );
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "execute retained SQL");
    failures += expect_cursor_text(stmt, 0U, "17", "retained SQL result");
    failures += mylite_test_expect_text(
        mylite_stmt_column_name(stmt, 0U),
        "retained_value",
        "retained SQL column name"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize retained SQL");

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

    failures += mylite_test_expect_int(
        mylite_set_client_found_rows(NULL, 1),
        MYLITE_MISUSE,
        "reject null client found rows database"
    );
    if (mylite_test_make_path(path, sizeof(path), "native_dml_bindings") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open native DML file");
    failures += execute_ok(database, "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(
        database,
        "CREATE TABLE items (id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
        "name VARCHAR(64) NOT NULL, payload VARBINARY(64), score INT NOT NULL DEFAULT 0)"
    );

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "INSERT INTO items (name, payload) VALUES (?, ?)",
            strlen("INSERT INTO items (name, payload) VALUES (?, ?)"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native INSERT"
    );
    failures +=
        mylite_test_expect_size(mylite_stmt_parameter_count(stmt), 2U, "native INSERT parameters");
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 0U, injection_text, strlen(injection_text)),
        MYLITE_OK,
        "bind native INSERT text"
    );
    failures += mylite_test_expect_int(
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
    failures += mylite_test_expect_int(
        mylite_stmt_bind_blob(stmt, 1U, first_payload, sizeof(first_payload)),
        MYLITE_OK,
        "bind native INSERT blob"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "execute native INSERT");
    failures += mylite_test_expect_true(
        mylite_stmt_affected_rows(stmt) == 1,
        "native INSERT affected rows"
    );
    failures += mylite_test_expect_uint64(mylite_stmt_insert_id(stmt), 1U, "native INSERT id");
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

    failures += mylite_test_expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset native INSERT");
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 0U, "second", strlen("second")),
        MYLITE_OK,
        "rebind native INSERT text"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_blob(stmt, 1U, NULL, 0U),
        MYLITE_OK,
        "rebind native INSERT empty blob"
    );
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "repeat native INSERT");
    failures += mylite_test_expect_true(
        mylite_stmt_affected_rows(stmt) == 1,
        "repeated INSERT affected rows"
    );
    failures += mylite_test_expect_uint64(mylite_stmt_insert_id(stmt), 2U, "repeated INSERT id");
    failures +=
        mylite_test_expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize native INSERT");
    stmt = NULL;

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "INSERT INTO items (id, name) VALUES (?, ?)",
            strlen("INSERT INTO items (id, name) VALUES (?, ?)"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native constraint INSERT"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_bind_int64(stmt, 0U, 2), MYLITE_OK, "bind duplicate id");
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 1U, "duplicate", strlen("duplicate")),
        MYLITE_OK,
        "bind duplicate row value"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_ERROR,
        "native INSERT reports constraint error"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset failed native INSERT");
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 0U, recovered_insert_id),
        MYLITE_OK,
        "rebind recovered id"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 1U, "recovered", strlen("recovered")),
        MYLITE_OK,
        "rebind recovered row value"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_DONE,
        "execute recovered native INSERT"
    );
    failures += mylite_test_expect_true(
        mylite_stmt_affected_rows(stmt) == 1,
        "recovered INSERT affected rows"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize constraint INSERT");
    stmt = NULL;

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "UPDATE items SET name = ? WHERE id = ?",
            strlen("UPDATE items SET name = ? WHERE id = ?"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native UPDATE"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 0U, "changed", strlen("changed")),
        MYLITE_OK,
        "bind native UPDATE value"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 1U, 2),
        MYLITE_OK,
        "bind native UPDATE id"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "execute native UPDATE");
    failures += mylite_test_expect_true(
        mylite_stmt_affected_rows(stmt) == 1,
        "native UPDATE affected rows"
    );
    failures += mylite_test_expect_text(
        mylite_stmt_info(stmt),
        "Rows matched: 1  Changed: 1  Warnings: 0",
        "native UPDATE info"
    );
    failures += mylite_test_expect_int(
        mylite_set_client_found_rows(database, 1),
        MYLITE_OK,
        "enable native UPDATE found rows"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_reset(stmt),
        MYLITE_OK,
        "reset native UPDATE found rows"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_DONE,
        "execute native UPDATE found rows"
    );
    failures += mylite_test_expect_true(
        mylite_stmt_affected_rows(stmt) == 1,
        "native UPDATE matched affected rows"
    );
    failures += mylite_test_expect_text(
        mylite_stmt_info(stmt),
        "Rows matched: 1  Changed: 0  Warnings: 0",
        "native UPDATE found rows info"
    );
    failures += expect_query_scalar_text(
        database,
        (struct expected_query_scalar_text){
            .sql = "SELECT ROW_COUNT()",
            .expected = "1",
            .context = "native UPDATE found rows row count",
        }
    );
    failures += mylite_test_expect_int(
        mylite_set_client_found_rows(database, 0),
        MYLITE_OK,
        "disable native UPDATE found rows"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize native UPDATE");
    stmt = NULL;

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "UPDATE items SET score = score + ? WHERE id = ?",
            strlen("UPDATE items SET score = score + ? WHERE id = ?"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native UPDATE arithmetic parameters"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 0U, update_arithmetic_delta),
        MYLITE_OK,
        "bind native UPDATE arithmetic delta"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 1U, 2),
        MYLITE_OK,
        "bind native UPDATE arithmetic predicate"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_DONE,
        "execute native UPDATE arithmetic"
    );
    failures += mylite_test_expect_true(
        mylite_stmt_affected_rows(stmt) == 1,
        "native UPDATE arithmetic affected rows"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize native UPDATE arithmetic"
    );
    stmt = NULL;
    failures += expect_query_scalar_text(
        database,
        (struct expected_query_scalar_text){
            .sql = "SELECT score FROM items WHERE id = 2",
            .expected = "10",
            .context = "native UPDATE arithmetic stored value",
        }
    );

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "DELETE FROM items WHERE id = ?",
            strlen("DELETE FROM items WHERE id = ?"),
            &stmt
        ),
        MYLITE_OK,
        "prepare native DELETE"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 0U, 1),
        MYLITE_OK,
        "bind native DELETE id"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "execute native DELETE");
    failures += mylite_test_expect_true(
        mylite_stmt_affected_rows(stmt) == 1,
        "native DELETE affected rows"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize native DELETE");
    stmt = NULL;

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "INSERT INTO items (name) VALUES ('constant')",
            strlen("INSERT INTO items (name) VALUES ('constant')"),
            &stmt
        ),
        MYLITE_OK,
        "prepare zero-parameter INSERT"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_DONE,
        "execute zero-parameter INSERT"
    );
    failures += mylite_test_expect_true(
        mylite_stmt_affected_rows(stmt) == 1,
        "zero-parameter INSERT affected rows"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize zero-parameter INSERT"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_native_prepared_multirow_dml(void) {
    enum {
        duplicate_first_age = 63,
        duplicate_second_age = 17,
        duplicate_second_job_parameter = 5,
        duplicate_third_name_parameter = 6,
        duplicate_third_age_parameter = 7,
        duplicate_third_job_parameter = 8,
        duplicate_third_age = 75,
        upsert_insert_age = 31,
        upsert_update_name_parameter = 5,
        upsert_update_age = 32,
    };

    static const char duplicate_sql[] =
        "INSERT INTO people (name, age, job) VALUES (?, ?, ?), (?, ?, ?), (?, ?, ?)";
    static const char upsert_sql[] =
        "INSERT INTO people (job, age, name) VALUES (?, ?, ?), (?, ?, ?) "
        "ON DUPLICATE KEY UPDATE job = VALUES(job), age = VALUES(age), name = VALUES(name)";
    static const char update_sql[] = "UPDATE people SET name = ? WHERE job = ?";
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open multirow DML");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(
        database,
        "CREATE TABLE people (name VARCHAR(32) UNIQUE, age INT, job VARCHAR(32) UNIQUE)"
    );
    failures += execute_ok(database, "START TRANSACTION");
    failures += execute_ok(database, "INSERT INTO people VALUES ('John', 30, 'Speaker')");

    failures += mylite_test_expect_int(
        mylite_prepare(database, duplicate_sql, strlen(duplicate_sql), &stmt),
        MYLITE_OK,
        "prepare duplicate multirow INSERT"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 0U, "Elvis", strlen("Elvis")),
        MYLITE_OK,
        "bind duplicate row one name"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 1U, duplicate_first_age),
        MYLITE_OK,
        "bind duplicate row one age"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 2U, "Singer", strlen("Singer")),
        MYLITE_OK,
        "bind duplicate row one job"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 3U, "John", strlen("John")),
        MYLITE_OK,
        "bind duplicate row two name"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 4U, duplicate_second_age),
        MYLITE_OK,
        "bind duplicate row two age"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(
            stmt,
            duplicate_second_job_parameter,
            "Consultant",
            strlen("Consultant")
        ),
        MYLITE_OK,
        "bind duplicate row two job"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, duplicate_third_name_parameter, "Frank", strlen("Frank")),
        MYLITE_OK,
        "bind duplicate row three name"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, duplicate_third_age_parameter, duplicate_third_age),
        MYLITE_OK,
        "bind duplicate row three age"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, duplicate_third_job_parameter, "Bass", strlen("Bass")),
        MYLITE_OK,
        "bind duplicate row three job"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_ERROR,
        "execute duplicate multirow INSERT"
    );
    failures += mylite_test_expect_int(
        mylite_errcode(database),
        mysql_error_duplicate_key,
        "duplicate multirow INSERT diagnostic"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize duplicate multirow INSERT"
    );
    stmt = NULL;
    failures += expect_query_scalar_text(
        database,
        (struct expected_query_scalar_text){
            .sql = "SELECT COUNT(*) FROM people",
            .expected = "1",
            .context = "duplicate multirow INSERT remains atomic",
        }
    );

    failures += mylite_test_expect_int(
        mylite_prepare(database, upsert_sql, strlen(upsert_sql), &stmt),
        MYLITE_OK,
        "prepare multirow upsert"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 0U, "Presenter", strlen("Presenter")),
        MYLITE_OK,
        "bind upsert insert job"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 1U, upsert_insert_age),
        MYLITE_OK,
        "bind upsert insert age"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 2U, "Tiffany", strlen("Tiffany")),
        MYLITE_OK,
        "bind upsert insert name"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 3U, "Speaker", strlen("Speaker")),
        MYLITE_OK,
        "bind upsert update job"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 4U, upsert_update_age),
        MYLITE_OK,
        "bind upsert update age"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, upsert_update_name_parameter, "Meredith", strlen("Meredith")),
        MYLITE_OK,
        "bind upsert update name"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "execute multirow upsert");
    failures += mylite_test_expect_int64(
        mylite_stmt_affected_rows(stmt),
        3,
        "multirow upsert affected rows"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize multirow upsert");
    failures += expect_query_scalar_text(
        database,
        (struct expected_query_scalar_text){
            .sql = "SELECT COUNT(*) FROM people",
            .expected = "2",
            .context = "multirow upsert row count",
        }
    );
    failures += expect_query_scalar_text(
        database,
        (struct expected_query_scalar_text){
            .sql = "SELECT age FROM people WHERE job = 'Speaker'",
            .expected = "32",
            .context = "multirow upsert updated row",
        }
    );

    stmt = NULL;
    failures += mylite_test_expect_int(
        mylite_prepare(database, update_sql, strlen(update_sql), &stmt),
        MYLITE_OK,
        "prepare duplicate-key UPDATE"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 0U, "Meredith", strlen("Meredith")),
        MYLITE_OK,
        "bind duplicate-key UPDATE name"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_bind_text(stmt, 1U, "Presenter", strlen("Presenter")),
        MYLITE_OK,
        "bind duplicate-key UPDATE predicate"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_ERROR,
        "execute duplicate-key UPDATE"
    );
    failures += mylite_test_expect_int(
        mylite_errcode(database),
        mysql_error_duplicate_key,
        "duplicate-key UPDATE diagnostic"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize duplicate-key UPDATE"
    );
    failures += expect_query_scalar_text(
        database,
        (struct expected_query_scalar_text){
            .sql = "SELECT name FROM people WHERE job = 'Presenter'",
            .expected = "Tiffany",
            .context = "duplicate-key UPDATE remains atomic",
        }
    );
    failures += execute_ok(database, "ROLLBACK");

    mylite_close(database);
    return failures;
}

static int test_buffered_prepared_statement_releases_connection(void) {
    char path[test_path_capacity];
    const struct mylite_session_state *session = NULL;
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "buffered_prepared") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open buffered prepared");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE items (id INT NOT NULL, name VARCHAR(20))");
    failures += execute_ok(database, "INSERT INTO items VALUES (1, 'alpha'), (2, 'beta')");
    failures += mylite_test_expect_int(
        mylite_prepare_buffered(
            database,
            "SELECT name FROM items WHERE id >= ? ORDER BY id",
            strlen("SELECT name FROM items WHERE id >= ? ORDER BY id"),
            &stmt
        ),
        MYLITE_OK,
        "prepare buffered parameter query"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_bind_int64(stmt, 0U, 1), MYLITE_OK, "bind buffered id");
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "buffered first row");
    failures += expect_cursor_text(stmt, 0U, "alpha", "buffered first value");
    session = mylite_connection_session_state(database);
    failures +=
        mylite_test_expect_int((int)session->previous_row_count, -1, "publish buffered row count");
    failures += execute_ok(database, "INSERT INTO items VALUES (3, 'gamma')");
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "buffered unread row");
    failures += expect_cursor_text(stmt, 0U, "beta", "buffered unread value");
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "buffered result done");
    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_int(
        (int)session->previous_row_count,
        1,
        "preserve later statement row count"
    );

    failures +=
        mylite_test_expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset buffered statement");
    failures += mylite_test_expect_int(
        mylite_stmt_bind_int64(stmt, 0U, 3),
        MYLITE_OK,
        "rebind buffered id"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "reexecuted buffered row");
    failures += expect_cursor_text(stmt, 0U, "gamma", "reexecuted buffered value");
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize buffered statement"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_cursor_materializes_information_schema_selects(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "information_schema") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open information_schema file"
    );
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

    failures += mylite_test_expect_int(
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
    failures += mylite_test_expect_size(
        mylite_stmt_column_count(stmt),
        2U,
        "information_schema cursor column count"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "information_schema row step");
    failures += expect_cursor_text(stmt, 0U, "id", "information_schema column name");
    failures += expect_cursor_text(stmt, 1U, "int", "information_schema data type");
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "information_schema done step");
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize information_schema"
    );
    stmt = NULL;

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT id, 1 AS marker FROM items ORDER BY id LIMIT 1 OFFSET 2",
            strlen("SELECT id, 1 AS marker FROM items ORDER BY id LIMIT 1 OFFSET 2"),
            &stmt
        ),
        MYLITE_OK,
        "prepare materialized cursor limit offset"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "materialized offset row");
    failures += expect_cursor_text(stmt, 0U, "3", "materialized offset id");
    failures += expect_cursor_text(stmt, 1U, "1", "materialized offset marker");
    failures += mylite_test_expect_int(
        mylite_stmt_step(stmt),
        MYLITE_DONE,
        "materialized offset done step"
    );
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(stmt),
        MYLITE_OK,
        "finalize materialized offset"
    );
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

    if (mylite_test_make_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open cursor unsupported file"
    );
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE items (id INT NOT NULL)");

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT missing_column FROM items",
            strlen("SELECT missing_column FROM items"),
            &stmt
        ),
        MYLITE_ERROR,
        "preserve cursor planning error"
    );
    failures += mylite_test_expect_true(stmt == NULL, "failed cursor planning statement is null");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        "Unknown column 'missing_column'",
        "cursor planning diagnostic"
    );

    failures += mylite_test_expect_int(
        mylite_prepare(database, "SELECT ABS(1, ?)", strlen("SELECT ABS(1, ?)"), &stmt),
        MYLITE_ERROR,
        "preserve parameter-independent prepare error"
    );
    failures +=
        mylite_test_expect_true(stmt == NULL, "semantic prepare failure leaves null statement");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        "Incorrect parameter count",
        "semantic prepare diagnostic"
    );

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "CREATE TABLE another (id INT)",
            strlen("CREATE TABLE another (id INT)"),
            &stmt
        ),
        MYLITE_OK,
        "prepare DDL"
    );
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "execute prepared DDL");
    failures +=
        mylite_test_expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize prepared DDL");
    stmt = NULL;

    failures += mylite_test_expect_int(
        mylite_prepare_buffered(database, "SHOW TABLES", strlen("SHOW TABLES"), &stmt),
        MYLITE_OK,
        "prepare SHOW"
    );
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "prepared SHOW first row");
    failures += expect_cursor_text(stmt, 0U, "another", "prepared SHOW first table");
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "prepared SHOW second row");
    failures += expect_cursor_text(stmt, 0U, "items", "prepared SHOW second table");
    failures += mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "prepared SHOW done");
    failures += mylite_test_expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset prepared SHOW");
    failures +=
        mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "reexecute prepared SHOW");
    failures += expect_cursor_text(stmt, 0U, "another", "reexecuted prepared SHOW table");
    failures +=
        mylite_test_expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize prepared SHOW");
    stmt = NULL;

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "PREPARE nested FROM 'SELECT 1'",
            strlen("PREPARE nested FROM 'SELECT 1'"),
            &stmt
        ),
        MYLITE_ERROR,
        "prepare rejects nested PREPARE"
    );
    failures += mylite_test_expect_true(stmt == NULL, "nested prepare leaves null statement");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        "not supported",
        "nested prepare diagnostic"
    );

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT ? FROM missing_items",
            strlen("SELECT ? FROM missing_items"),
            &stmt
        ),
        MYLITE_ERROR,
        "prepared parameter resolves table at prepare time"
    );
    failures +=
        mylite_test_expect_true(stmt == NULL, "missing prepared table leaves null statement");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        "doesn't exist",
        "missing prepared table"
    );

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "SELECT missing_column FROM items WHERE id = ?",
            strlen("SELECT missing_column FROM items WHERE id = ?"),
            &stmt
        ),
        MYLITE_ERROR,
        "prepared parameter resolves column at prepare time"
    );
    failures +=
        mylite_test_expect_true(stmt == NULL, "missing prepared column leaves null statement");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        "Unknown column",
        "missing prepared column"
    );

    failures += mylite_test_expect_int(
        mylite_prepare(database, NULL, 0U, &stmt),
        MYLITE_MISUSE,
        "cursor rejects null SQL"
    );
    failures += mylite_test_expect_true(stmt == NULL, "misuse prepare leaves null statement");

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
    return mylite_test_expect_int(rc, MYLITE_OK, sql);
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
    failures += mylite_test_expect_int(rc, MYLITE_OK, expected.context);
    if (rc == MYLITE_OK) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 1U, expected.context);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, expected.context);
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, 0U, 0U),
            expected.expected,
            expected.context
        );
    }
    mylite_result_free(result);
    return failures;
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

    failures += mylite_test_expect_text(actual, expected, context);
    failures += mylite_test_expect_size(
        mylite_stmt_value_size(stmt, column_index),
        strlen(expected),
        context
    );
    return failures;
}

static int expect_cursor_null(const mylite_stmt *stmt, size_t column_index, const char *context) {
    int failures = 0;

    failures +=
        mylite_test_expect_true(mylite_stmt_value_text(stmt, column_index) == NULL, context);
    failures +=
        mylite_test_expect_true(mylite_stmt_value_bytes(stmt, column_index) == NULL, context);
    failures += mylite_test_expect_size(mylite_stmt_value_size(stmt, column_index), 0U, context);
    return failures;
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
