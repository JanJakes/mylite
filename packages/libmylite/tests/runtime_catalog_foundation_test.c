#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_catalog_string_pool.h"
#include "runtime/mylite_connection.h"
#include "runtime/mylite_execution_loaded_catalog.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"

#include <inttypes.h>
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
    sql_buffer_capacity = 128,
    sqlite_header_size = 16,
    expected_catalog_table_count = 6,
    expected_catalog_generation_after_mutations = 5,
    catalog_test_timestamp_epoch = 1700000000,
    catalog_string_test_generation = 41,
    catalog_string_test_next_generation = 42,
    catalog_string_test_final_generation = 43,
    catalog_string_retained_byte_limit = 4096,
};

_Static_assert(
    sizeof(struct mylite_catalog_column_descriptor) <= 3072U,
    "catalog column descriptors must keep cold text out of line"
);

typedef int (*catalog_tamper_fn)(sqlite3 *sqlite);

static int test_catalog_created_in_shifted_payload_without_preamble_changes(void);
static int test_reopen_preserves_catalog_rows_and_generation(void);
static int test_idempotent_catalog_initialization_across_repeated_opens(void);
static int test_independent_file_backed_handles_have_independent_catalog_state(void);
static int test_catalog_default_text_validation(void);
static int test_rejects_incompatible_and_incomplete_catalog_metadata(void);
static int test_rejects_catalog_integrity_corruption(void);
static int test_zero_initialized_catalog_cleanup(void);
static int test_catalog_string_pool_deduplicates_and_grows(void);
static int test_catalog_string_pool_reclaims_retired_generations(void);
static int test_catalog_string_memory_stays_bounded_across_ddl_generations(void);
static int test_pinned_column_descriptors_keep_retired_strings_alive(void);
static int expect_catalog_tamper_rejected(
    const char *name,
    const char *const *setup_sql,
    size_t setup_count,
    catalog_tamper_fn tamper,
    const char *context
);
static int execute_mylite_statement(mylite_db *database, const char *sql);
static int tamper_remove_state_check(sqlite3 *sqlite);
static int tamper_remove_schema_primary_key(sqlite3 *sqlite);
static int tamper_remove_schema_unique_key(sqlite3 *sqlite);
static int tamper_remove_parent_foreign_key_index(sqlite3 *sqlite);
static int tamper_orphan_column(sqlite3 *sqlite);
static int tamper_gap_column_ordinal(sqlite3 *sqlite);
static int tamper_cross_index_ownership(sqlite3 *sqlite);
static int tamper_orphan_view_source(sqlite3 *sqlite);
static int tamper_orphan_foreign_key_parent(sqlite3 *sqlite);
static int tamper_future_descriptor_generation(sqlite3 *sqlite);
static int tamper_drop_physical_table(sqlite3 *sqlite);
static int tamper_drop_physical_index(sqlite3 *sqlite);
static int tamper_drop_physical_column(sqlite3 *sqlite);
static int tamper_delete_catalog_column(sqlite3 *sqlite);
static int tamper_replace_physical_index_definition(sqlite3 *sqlite);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int execute_sql(sqlite3 *connection, const char *sql);
static int query_catalog_table_count(sqlite3 *connection, int *out_count);
static int query_single_int(sqlite3 *connection, const char *sql, int *out_value);
static int query_single_text(
    sqlite3 *connection,
    const char *sql,
    char *destination,
    size_t destination_size
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_bool(bool actual, bool expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_catalog_created_in_shifted_payload_without_preamble_changes();
    failures += test_reopen_preserves_catalog_rows_and_generation();
    failures += test_idempotent_catalog_initialization_across_repeated_opens();
    failures += test_independent_file_backed_handles_have_independent_catalog_state();
    failures += test_catalog_default_text_validation();
    failures += test_rejects_incompatible_and_incomplete_catalog_metadata();
    failures += test_rejects_catalog_integrity_corruption();
    failures += test_zero_initialized_catalog_cleanup();
    failures += test_catalog_string_pool_deduplicates_and_grows();
    failures += test_catalog_string_pool_reclaims_retired_generations();
    failures += test_catalog_string_memory_stays_bounded_across_ddl_generations();
    failures += test_pinned_column_descriptors_keep_retired_strings_alive();

    return failures == 0 ? 0 : 1;
}

static int test_catalog_string_pool_deduplicates_and_grows(void) {
    enum { string_count = 160 };

    struct mylite_catalog_string_pool pool = {0};
    const char *strings[string_count] = {0};
    const char *duplicate = NULL;
    const char *empty = NULL;
    char text[32];
    int failures = 0;

    mylite_catalog_string_pool_init(&pool);
    failures += expect_int(
        mylite_catalog_string_pool_intern_c_string(&pool, NULL, &empty),
        MYLITE_OK,
        "intern empty catalog string"
    );
    failures += expect_text(empty, "", "empty catalog string value");
    for (size_t index = 0U; index < string_count; ++index) {
        int written = snprintf(text, sizeof(text), "catalog-string-%zu", index);

        failures +=
            expect_true(written > 0 && (size_t)written < sizeof(text), "format catalog string");
        failures += expect_int(
            mylite_catalog_string_pool_intern_c_string(&pool, text, &strings[index]),
            MYLITE_OK,
            "intern catalog string"
        );
    }
    failures += expect_int(
        mylite_catalog_string_pool_intern_c_string(&pool, "catalog-string-117", &duplicate),
        MYLITE_OK,
        "re-intern catalog string"
    );
    failures += expect_true(duplicate == strings[117], "catalog string pointer deduplication");
    for (size_t index = 0U; index < string_count; ++index) {
        int written = snprintf(text, sizeof(text), "catalog-string-%zu", index);

        failures +=
            expect_true(written > 0 && (size_t)written < sizeof(text), "reformat catalog string");
        failures += expect_text(strings[index], text, "catalog string survives pool growth");
    }
    failures += expect_int((int)pool.count, string_count + 1, "unique catalog string count");
    mylite_catalog_string_pool_deinit(&pool);

    return failures;
}

static int test_catalog_string_pool_reclaims_retired_generations(void) {
    struct mylite_catalog_string_pool pool = {0};
    struct mylite_catalog_string_pool_reference reference = {0};
    const char *old_text = NULL;
    const char *new_text = NULL;
    size_t old_generation_bytes = 0U;
    int failures = 0;

    mylite_catalog_string_pool_init(&pool);
    mylite_catalog_string_pool_set_generation(&pool, catalog_string_test_generation);
    failures += expect_int(
        mylite_catalog_string_pool_intern_c_string(&pool, "old-generation", &old_text),
        MYLITE_OK,
        "intern old catalog generation"
    );
    failures += expect_int(
        mylite_catalog_string_pool_reference_acquire(
            &pool,
            catalog_string_test_generation,
            &reference
        ),
        MYLITE_OK,
        "pin old catalog generation"
    );
    old_generation_bytes = mylite_catalog_string_pool_byte_count(&pool);

    mylite_catalog_string_pool_set_generation(&pool, catalog_string_test_next_generation);
    failures += expect_int(
        mylite_catalog_string_pool_intern_c_string(&pool, "new-generation", &new_text),
        MYLITE_OK,
        "intern new catalog generation"
    );
    failures += expect_text(old_text, "old-generation", "pinned old catalog string remains valid");
    failures += expect_text(new_text, "new-generation", "new catalog string value");
    failures += expect_int(
        (int)mylite_catalog_string_pool_generation_count(&pool),
        2,
        "pinned retired catalog generation retained"
    );
    failures += expect_true(
        mylite_catalog_string_pool_byte_count(&pool) > old_generation_bytes,
        "pinned generations contribute to pool bytes"
    );

    mylite_catalog_string_pool_reference_release(&reference);
    failures += expect_int(
        (int)mylite_catalog_string_pool_generation_count(&pool),
        1,
        "last pin reclaims retired catalog generation"
    );
    failures += expect_true(
        mylite_catalog_string_pool_byte_count(&pool) < old_generation_bytes * 2U,
        "retired catalog string bytes reclaimed"
    );

    mylite_catalog_string_pool_set_generation(&pool, catalog_string_test_final_generation);
    failures += expect_int(
        (int)mylite_catalog_string_pool_generation_count(&pool),
        0,
        "unpinned current generation reclaimed on advance"
    );
    failures += expect_int(
        (int)mylite_catalog_string_pool_byte_count(&pool),
        0,
        "empty catalog pool has no retained allocation"
    );
    mylite_catalog_string_pool_deinit(&pool);
    return failures;
}

static int test_catalog_string_memory_stays_bounded_across_ddl_generations(void) {
    enum { generation_count = 64, ddl_sql_capacity = 256 };

    char path[test_path_capacity];
    char sql[ddl_sql_capacity];
    mylite_db *database = NULL;
    size_t peak_bytes = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "catalog_string_generations") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open catalog string file");
    failures += execute_mylite_statement(database, "CREATE DATABASE app");
    failures += execute_mylite_statement(database, "USE app");
    failures += execute_mylite_statement(
        database,
        "CREATE TABLE items (id INT, value VARCHAR(32) COMMENT 'generation-initial')"
    );

    for (size_t generation = 0U; generation < generation_count && failures == 0; ++generation) {
        int written = snprintf(
            sql,
            sizeof(sql),
            "ALTER TABLE items MODIFY COLUMN value VARCHAR(32) COMMENT "
            "'generation-%03zu-abcdefghijklmnopqrstuvwxyz0123456789'",
            generation
        );
        size_t retained_bytes = 0U;

        failures += expect_true(
            written > 0 && (size_t)written < sizeof(sql),
            "format catalog generation DDL"
        );
        failures += execute_mylite_statement(database, sql);
        failures += execute_mylite_statement(database, "SELECT value FROM items LIMIT 0");
        retained_bytes = mylite_catalog_string_pool_byte_count(&database->catalog_strings);
        for (size_t index = 0U; index < database->table_columns_cache_count; ++index) {
            retained_bytes += database->table_columns_cache[index].byte_count;
        }
        if (retained_bytes > peak_bytes) {
            peak_bytes = retained_bytes;
        }
        failures += expect_true(
            retained_bytes <= MYLITE_EXECUTION_TABLE_COLUMNS_CACHE_BYTE_LIMIT,
            "catalog cache budget includes cold string storage"
        );
        failures += expect_true(
            mylite_catalog_string_pool_generation_count(&database->catalog_strings) <= 1U,
            "completed DDL generations do not retain cold strings"
        );
    }
    failures += expect_true(peak_bytes > 0U, "catalog generation test populated cold strings");
    failures += expect_true(
        mylite_catalog_string_pool_byte_count(&database->catalog_strings) <
            catalog_string_retained_byte_limit,
        "repeated DDL leaves only the current compact string generation"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_pinned_column_descriptors_keep_retired_strings_alive(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_stmt *statement = NULL;
    struct loaded_table_columns_cache_entry *pinned_entry = NULL;
    const char *pinned_comment = NULL;
    const char *next_generation_text = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "catalog_string_pin") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open catalog pin file");
    failures += execute_mylite_statement(database, "CREATE DATABASE app");
    failures += execute_mylite_statement(database, "USE app");
    failures += execute_mylite_statement(
        database,
        "CREATE TABLE pinned_items (id INT, value VARCHAR(32) COMMENT 'pinned-comment')"
    );
    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT value FROM pinned_items",
            sizeof("SELECT value FROM pinned_items") - 1U,
            &statement
        ),
        MYLITE_OK,
        "prepare cursor with pinned catalog strings"
    );
    for (size_t index = 0U; index < database->table_columns_cache_count; ++index) {
        if (database->table_columns_cache[index].reference_count != 0U) {
            pinned_entry = &database->table_columns_cache[index];
            break;
        }
    }
    failures += expect_true(pinned_entry != NULL, "prepared cursor pins column descriptor cache");
    if (pinned_entry != NULL) {
        for (size_t index = 0U; index < pinned_entry->column_count; ++index) {
            if (strcmp(pinned_entry->columns[index].name, "value") == 0) {
                pinned_comment = pinned_entry->columns[index].comment;
                break;
            }
        }
    }
    failures += expect_true(pinned_comment != NULL, "find pinned descriptor comment");
    if (pinned_comment != NULL) {
        failures += expect_text(pinned_comment, "pinned-comment", "pinned descriptor comment");
    }

    mylite_execution_table_columns_cache_invalidate(database);
    mylite_execution_table_key_metadata_cache_invalidate(database);
    mylite_catalog_string_pool_set_generation(
        &database->catalog_strings,
        database->session.catalog_generation + 1U
    );
    failures += expect_int(
        mylite_catalog_string_pool_intern_c_string(
            &database->catalog_strings,
            "next-generation",
            &next_generation_text
        ),
        MYLITE_OK,
        "create next catalog string generation"
    );
    if (pinned_comment != NULL) {
        failures += expect_text(
            pinned_comment,
            "pinned-comment",
            "retired descriptor string remains valid while pinned"
        );
    }
    failures += expect_int(
        (int)mylite_catalog_string_pool_generation_count(&database->catalog_strings),
        2,
        "pinned descriptor retains retired generation"
    );
    failures += expect_int(mylite_stmt_finalize(statement), MYLITE_OK, "release pinned cursor");
    statement = NULL;
    failures += expect_int(
        (int)mylite_catalog_string_pool_generation_count(&database->catalog_strings),
        1,
        "cursor release reclaims retired descriptor strings"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_catalog_created_in_shifted_payload_without_preamble_changes(void) {
    static const unsigned char sqlite_header[sqlite_header_size] = "SQLite format 3";

    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char payload_header[sqlite_header_size];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    int catalog_tables = 0;
    int state_rows = 0;
    int schema_rows = 0;
    int parent_foreign_key_indexes = 0;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "create") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open new catalog file");
    sqlite = mylite_connection_sqlite_for_test(database);
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);

    failures += expect_true(sqlite != NULL, "catalog SQLite connection exists");
    failures += expect_true(catalog != NULL, "catalog state exists");
    if (catalog != NULL) {
        failures += expect_bool(catalog->initialized, true, "catalog initialized");
        failures += expect_uint64(
            catalog->schema_version,
            MYLITE_CATALOG_SCHEMA_VERSION,
            "catalog schema version"
        );
        failures += expect_uint64(catalog->generation, 1U, "initial catalog generation");
        failures += expect_bool(
            catalog->descriptor_cache_is_valid,
            false,
            "initial descriptor cache invalid"
        );
    }
    if (session != NULL) {
        failures += expect_uint64(session->catalog_generation, 1U, "session catalog generation");
    }
    if (sqlite != NULL) {
        failures += query_catalog_table_count(sqlite, &catalog_tables);
        failures +=
            query_single_int(sqlite, "SELECT count(*) FROM _mylite_catalog_state", &state_rows);
        failures +=
            query_single_int(sqlite, "SELECT count(*) FROM _mylite_catalog_schemas", &schema_rows);
        failures += query_single_int(
            sqlite,
            "SELECT count(*) FROM sqlite_master WHERE type = 'index' "
            "AND name = '_mylite_catalog_foreign_keys_parent_table_id'",
            &parent_foreign_key_indexes
        );
    }
    failures += expect_int(catalog_tables, expected_catalog_table_count, "catalog table count");
    failures += expect_int(state_rows, 1, "catalog state singleton row");
    failures += expect_int(schema_rows, 0, "initial schema row count");
    failures += expect_int(parent_foreign_key_indexes, 1, "foreign-key parent lookup index");

    mylite_close(database);

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "catalog open preserves preamble"
    );
    failures += read_file_at(
        path,
        MYLITE_FILE_SQLITE_PAYLOAD_OFFSET,
        payload_header,
        sizeof(payload_header)
    );
    failures +=
        expect_bytes(payload_header, sqlite_header, sizeof(sqlite_header), "shifted SQLite header");

    remove_related_files(path);

    return failures;
}

static int test_reopen_preserves_catalog_rows_and_generation(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    struct mylite_catalog_column_descriptor column = {0};
    const struct mylite_catalog *catalog = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "reopen") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open catalog file");
    failures += expect_int(
        mylite_catalog_create_schema(database, "app", &schema),
        MYLITE_OK,
        "create schema descriptor"
    );
    failures += expect_int64(schema.schema_id > 0 ? 1 : 0, 1, "schema id is positive");
    failures += expect_uint64(schema.created_catalog_generation, 2U, "schema create generation");
    failures += expect_int(
        mylite_catalog_create_table(
            database,
            schema.schema_id,
            "items",
            "phys_items",
            MYLITE_CATALOG_TABLE_KIND_BASE,
            MYLITE_CATALOG_DEFAULT_TABLE_CHARSET,
            MYLITE_CATALOG_DEFAULT_TABLE_COLLATION,
            "items comment",
            catalog_test_timestamp_epoch,
            catalog_test_timestamp_epoch,
            &table
        ),
        MYLITE_OK,
        "create table descriptor"
    );
    failures += expect_int(
        mylite_catalog_create_column(
            database,
            table.table_id,
            1,
            "id",
            "BIGINT",
            "INTEGER",
            false,
            MYLITE_CATALOG_COLUMN_DEFAULT_NONE,
            0,
            NULL,
            false,
            "",
            "",
            "",
            false,
            MYLITE_CATALOG_GENERATED_COLUMN_INVALID,
            "",
            "",
            &column
        ),
        MYLITE_OK,
        "create column descriptor"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += execute_sql(sqlite, "CREATE TABLE phys_items (id INTEGER NOT NULL)");
    }
    failures += expect_int(
        mylite_catalog_update_table_name(database, table.table_id, "renamed_items"),
        MYLITE_OK,
        "update table descriptor"
    );
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            expected_catalog_generation_after_mutations,
            "catalog generation after mutations"
        );
    }
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen catalog file");
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            expected_catalog_generation_after_mutations,
            "reopened catalog generation"
        );
    }
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read reopened schema"
    );
    failures += expect_text(schema.name, "app", "reopened schema name");
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "renamed_items", &table),
        MYLITE_OK,
        "read renamed table"
    );
    failures += expect_text(table.name, "renamed_items", "reopened table name");
    failures += expect_text(table.physical_name, "phys_items", "reopened physical table name");
    failures += expect_text(table.comment, "items comment", "reopened table comment");
    failures += expect_int64(
        table.created_time_utc_epoch,
        catalog_test_timestamp_epoch,
        "table created timestamp"
    );
    failures += expect_int64(
        table.updated_time_utc_epoch,
        catalog_test_timestamp_epoch,
        "table updated timestamp"
    );
    failures += expect_uint64(table.descriptor_version, 2U, "table descriptor version");
    failures += expect_int(
        mylite_catalog_read_column_by_name(database, table.table_id, "id", &column),
        MYLITE_OK,
        "read reopened column"
    );
    failures += expect_text(column.logical_type, "BIGINT", "reopened logical type");
    failures += expect_text(column.physical_type, "INTEGER", "reopened physical type");
    failures += expect_bool(column.is_nullable, false, "reopened nullability");
    failures += expect_bool(column.is_visible, true, "reopened visibility");

    failures += expect_int(
        mylite_catalog_delete_column(database, column.column_id),
        MYLITE_OK,
        "delete column descriptor"
    );
    failures += expect_int(
        mylite_catalog_read_column_by_name(database, table.table_id, "id", &column),
        MYLITE_ERROR,
        "deleted column is not readable"
    );
    failures += expect_int(
        mylite_catalog_delete_table(database, table.table_id),
        MYLITE_OK,
        "delete table descriptor"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "renamed_items", &table),
        MYLITE_ERROR,
        "deleted table is not readable"
    );
    failures += expect_int(
        mylite_catalog_delete_schema(database, schema.schema_id),
        MYLITE_OK,
        "delete schema descriptor"
    );
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_ERROR,
        "deleted schema is not readable"
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_idempotent_catalog_initialization_across_repeated_opens(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    const struct mylite_catalog *catalog = NULL;
    int catalog_tables = 0;
    int state_rows = 0;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "idempotent") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open idempotent file");
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "first reopen idempotent file");
    mylite_close(database);
    database = NULL;

    failures +=
        expect_int(mylite_open(path, &database), MYLITE_OK, "second reopen idempotent file");
    sqlite = mylite_connection_sqlite_for_test(database);
    catalog = mylite_connection_catalog_for_test(database);
    if (sqlite != NULL) {
        failures += query_catalog_table_count(sqlite, &catalog_tables);
        failures +=
            query_single_int(sqlite, "SELECT count(*) FROM _mylite_catalog_state", &state_rows);
    }
    if (catalog != NULL) {
        failures += expect_uint64(catalog->generation, 1U, "idempotent generation");
    }
    failures += expect_int(catalog_tables, expected_catalog_table_count, "idempotent table count");
    failures += expect_int(state_rows, 1, "idempotent state row count");

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_independent_file_backed_handles_have_independent_catalog_state(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    const struct mylite_catalog *first_catalog = NULL;
    const struct mylite_catalog *second_catalog = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first catalog file");
    failures +=
        expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second catalog file");
    failures += expect_int(
        mylite_catalog_create_schema(first, "first_app", &schema),
        MYLITE_OK,
        "create schema in first file"
    );
    failures += expect_int(
        mylite_catalog_read_schema_by_name(second, "first_app", &schema),
        MYLITE_ERROR,
        "second file does not see first catalog row"
    );

    first_catalog = mylite_connection_catalog_for_test(first);
    second_catalog = mylite_connection_catalog_for_test(second);
    if (first_catalog != NULL) {
        failures += expect_uint64(first_catalog->generation, 2U, "first catalog generation");
    }
    if (second_catalog != NULL) {
        failures += expect_uint64(second_catalog->generation, 1U, "second catalog generation");
    }

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);

    return failures;
}

static int test_catalog_default_text_validation(void) {
    mylite_db *database = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    struct mylite_catalog_column_descriptor column = {0};
    int failures =
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open default-text catalog");

    failures += expect_int(
        mylite_catalog_create_schema(database, "app", &schema),
        MYLITE_OK,
        "create default-text schema"
    );
    failures += expect_int(
        mylite_catalog_create_table(
            database,
            schema.schema_id,
            "defaults",
            "phys_defaults",
            MYLITE_CATALOG_TABLE_KIND_BASE,
            MYLITE_CATALOG_DEFAULT_TABLE_CHARSET,
            MYLITE_CATALOG_DEFAULT_TABLE_COLLATION,
            "",
            catalog_test_timestamp_epoch,
            catalog_test_timestamp_epoch,
            &table
        ),
        MYLITE_OK,
        "create default-text table"
    );
    failures += expect_int(
        mylite_catalog_create_column(
            database,
            table.table_id,
            1,
            "empty_varchar",
            "VARCHAR(3)",
            "TEXT",
            true,
            MYLITE_CATALOG_COLUMN_DEFAULT_TEXT,
            0,
            "",
            false,
            "",
            "",
            "",
            false,
            MYLITE_CATALOG_GENERATED_COLUMN_INVALID,
            "",
            "",
            &column
        ),
        MYLITE_OK,
        "catalog accepts empty string defaults for VARCHAR"
    );
    failures += expect_text(column.default_text, "", "empty VARCHAR catalog default");
    failures += expect_int(
        mylite_catalog_create_column(
            database,
            table.table_id,
            2,
            "date_default",
            "DATE",
            "TEXT",
            true,
            MYLITE_CATALOG_COLUMN_DEFAULT_TEXT,
            0,
            "2024-01-01",
            false,
            "",
            "",
            "",
            false,
            MYLITE_CATALOG_GENERATED_COLUMN_INVALID,
            "",
            "",
            &column
        ),
        MYLITE_OK,
        "catalog accepts nonempty temporal text defaults"
    );
    failures += expect_int(
        mylite_catalog_create_column(
            database,
            table.table_id,
            3,
            "decimal_default",
            "DECIMAL(5,2)",
            "TEXT",
            true,
            MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL,
            0,
            "1.00",
            false,
            "",
            "",
            "",
            false,
            MYLITE_CATALOG_GENERATED_COLUMN_INVALID,
            "",
            "",
            &column
        ),
        MYLITE_OK,
        "catalog accepts nonempty decimal text defaults"
    );
    failures += expect_int(
        mylite_catalog_create_column(
            database,
            table.table_id,
            4,
            "empty_decimal",
            "DECIMAL(5,2)",
            "TEXT",
            true,
            MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL,
            0,
            "",
            false,
            "",
            "",
            "",
            false,
            MYLITE_CATALOG_GENERATED_COLUMN_INVALID,
            "",
            "",
            &column
        ),
        MYLITE_MISUSE,
        "catalog rejects empty decimal text defaults"
    );
    failures += expect_int(
        mylite_catalog_create_column(
            database,
            table.table_id,
            4,
            "empty_date",
            "DATE",
            "TEXT",
            true,
            MYLITE_CATALOG_COLUMN_DEFAULT_TEXT,
            0,
            "",
            false,
            "",
            "",
            "",
            false,
            MYLITE_CATALOG_GENERATED_COLUMN_INVALID,
            "",
            "",
            &column
        ),
        MYLITE_MISUSE,
        "catalog rejects empty temporal text defaults"
    );
    failures += expect_int(
        mylite_catalog_create_column(
            database,
            table.table_id,
            4,
            "text_default",
            "TEXT",
            "TEXT",
            true,
            MYLITE_CATALOG_COLUMN_DEFAULT_TEXT,
            0,
            "abc",
            false,
            "",
            "",
            "",
            false,
            MYLITE_CATALOG_GENERATED_COLUMN_INVALID,
            "",
            "",
            &column
        ),
        MYLITE_OK,
        "catalog accepts TEXT descriptor defaults"
    );

    mylite_close(database);
    return failures;
}

static int test_rejects_incompatible_and_incomplete_catalog_metadata(void) {
    char path[test_path_capacity];
    char sql[sql_buffer_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "bad_version") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open bad-version file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        snprintf(
            sql,
            sizeof(sql),
            "UPDATE _mylite_catalog_state SET schema_version = %" PRIu32,
            (uint32_t)(MYLITE_CATALOG_SCHEMA_VERSION + 1U)
        );
        failures += execute_sql(sqlite, sql);
    }
    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_ERROR, "reject bad version");
    failures += expect_true(database == NULL, "bad version leaves output null");
    remove_related_files(path);

    if (make_test_path(path, sizeof(path), "v36_migration") != 0) {
        return failures + 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open v36 migration file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += execute_sql(
            sqlite,
            "UPDATE _mylite_catalog_state "
            "SET schema_version = 36, minimum_reader_schema_version = 35"
        );
    }
    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "migrate v36 catalog");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        int version = 0;
        int minimum_reader = 0;

        failures +=
            query_single_int(sqlite, "SELECT schema_version FROM _mylite_catalog_state", &version);
        failures += query_single_int(
            sqlite,
            "SELECT minimum_reader_schema_version FROM _mylite_catalog_state",
            &minimum_reader
        );
        failures += expect_int(version, MYLITE_CATALOG_SCHEMA_VERSION, "migrated catalog version");
        failures += expect_int(
            minimum_reader,
            MYLITE_CATALOG_MINIMUM_READER_SCHEMA_VERSION,
            "migrated minimum reader"
        );
    }
    mylite_close(database);
    database = NULL;
    remove_related_files(path);

    if (make_test_path(path, sizeof(path), "legacy_file_format_provenance") != 0) {
        return failures + 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open legacy provenance file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += execute_sql(
            sqlite,
            "UPDATE _mylite_catalog_state SET created_with_file_format_version = 1"
        );
    }
    mylite_close(database);
    database = NULL;
    failures +=
        expect_int(mylite_open(path, &database), MYLITE_OK, "accept legacy file-format provenance");
    mylite_close(database);
    database = NULL;
    remove_related_files(path);

    if (make_test_path(path, sizeof(path), "bad_file_format_provenance") != 0) {
        return failures + 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open bad provenance file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += execute_sql(
            sqlite,
            "UPDATE _mylite_catalog_state SET created_with_file_format_version = 0"
        );
    }
    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_ERROR, "reject invalid provenance");
    failures += expect_true(database == NULL, "invalid provenance leaves output null");
    remove_related_files(path);

    if (make_test_path(path, sizeof(path), "future_file_format_provenance") != 0) {
        return failures + 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open future provenance file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        snprintf(
            sql,
            sizeof(sql),
            "UPDATE _mylite_catalog_state SET created_with_file_format_version = %u",
            (unsigned int)(MYLITE_FILE_FORMAT_VERSION + 1U)
        );
        failures += execute_sql(sqlite, sql);
    }
    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_ERROR, "reject future provenance");
    failures += expect_true(database == NULL, "future provenance leaves output null");
    remove_related_files(path);

    if (make_test_path(path, sizeof(path), "bad_state_type") != 0) {
        return failures + 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open bad-state-type file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += execute_sql(
            sqlite,
            "DROP TABLE _mylite_catalog_state;"
            "CREATE TABLE _mylite_catalog_state ("
            "singleton_id INTEGER,"
            "schema_version TEXT NOT NULL,"
            "minimum_reader_schema_version INTEGER NOT NULL,"
            "catalog_generation INTEGER NOT NULL,"
            "created_with_file_format_version INTEGER NOT NULL"
            ");"
            "INSERT INTO _mylite_catalog_state VALUES (1, '1', 1, 1, 1);"
        );
    }
    mylite_close(database);
    database = NULL;
    failures +=
        expect_int(mylite_open(path, &database), MYLITE_ERROR, "reject bad catalog state type");
    failures += expect_true(database == NULL, "bad state type leaves output null");
    remove_related_files(path);

    if (make_test_path(path, sizeof(path), "extra_state_row") != 0) {
        return failures + 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open extra-state-row file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += execute_sql(
            sqlite,
            "DROP TABLE _mylite_catalog_state;"
            "CREATE TABLE _mylite_catalog_state ("
            "singleton_id INTEGER,"
            "schema_version INTEGER NOT NULL,"
            "minimum_reader_schema_version INTEGER NOT NULL,"
            "catalog_generation INTEGER NOT NULL,"
            "created_with_file_format_version INTEGER NOT NULL"
            ");"
            "INSERT INTO _mylite_catalog_state VALUES (1, 1, 1, 1, 1);"
            "INSERT INTO _mylite_catalog_state VALUES (2, 1, 1, 1, 1);"
        );
    }
    mylite_close(database);
    database = NULL;
    failures +=
        expect_int(mylite_open(path, &database), MYLITE_ERROR, "reject extra catalog state row");
    failures += expect_true(database == NULL, "extra state row leaves output null");
    remove_related_files(path);

    if (make_test_path(path, sizeof(path), "bad_schema_shape") != 0) {
        return failures + 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open bad-schema-shape file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += execute_sql(
            sqlite,
            "DROP TABLE _mylite_catalog_schemas;"
            "CREATE TABLE _mylite_catalog_schemas (schema_id INTEGER PRIMARY KEY);"
        );
    }
    mylite_close(database);
    database = NULL;
    failures += expect_int(
        mylite_open(path, &database),
        MYLITE_ERROR,
        "reject malformed schema descriptor table"
    );
    failures += expect_true(database == NULL, "malformed descriptor table leaves output null");
    remove_related_files(path);

    if (make_test_path(path, sizeof(path), "incomplete") != 0) {
        return failures + 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open incomplete file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += execute_sql(sqlite, "DROP TABLE _mylite_catalog_columns");
    }
    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_ERROR, "reject incomplete catalog");
    failures += expect_true(database == NULL, "incomplete catalog leaves output null");
    remove_related_files(path);

    return failures;
}

static int test_rejects_catalog_integrity_corruption(void) {
    static const char foreign_key_child_sql[] =
        "CREATE TABLE child_table (parent_id INT, "
        "CONSTRAINT fk_parent FOREIGN KEY (parent_id) REFERENCES parent_table (id))";
    static const char *const base_table_setup[] = {
        "CREATE DATABASE app",
        "USE app",
        "CREATE TABLE t (id INT PRIMARY KEY, value INT)",
    };
    static const char *const indexed_table_setup[] = {
        "CREATE DATABASE app",
        "USE app",
        "CREATE TABLE t (id INT PRIMARY KEY, value INT, KEY idx_value (value))",
    };
    static const char *const unindexed_table_setup[] = {
        "CREATE DATABASE app",
        "USE app",
        "CREATE TABLE t (id INT, value INT)",
    };
    static const char *const cross_index_setup[] = {
        "CREATE DATABASE app",
        "USE app",
        "CREATE TABLE t1 (id INT PRIMARY KEY, value INT, KEY idx_value (value))",
        "CREATE TABLE t2 (id INT PRIMARY KEY)",
    };
    static const char *const view_setup[] = {
        "CREATE DATABASE app",
        "USE app",
        "CREATE TABLE source_table (id INT PRIMARY KEY)",
        "CREATE VIEW source_view AS SELECT id FROM source_table",
    };
    static const char *const foreign_key_setup[] = {
        "CREATE DATABASE app",
        "USE app",
        "CREATE TABLE parent_table (id INT PRIMARY KEY)",
        foreign_key_child_sql,
    };
    int failures = 0;

    failures += expect_catalog_tamper_rejected(
        "missing_state_check",
        NULL,
        0U,
        tamper_remove_state_check,
        "reject catalog state without singleton check"
    );
    failures += expect_catalog_tamper_rejected(
        "missing_schema_pk",
        NULL,
        0U,
        tamper_remove_schema_primary_key,
        "reject catalog schema table without primary key"
    );
    failures += expect_catalog_tamper_rejected(
        "missing_schema_unique",
        NULL,
        0U,
        tamper_remove_schema_unique_key,
        "reject catalog schema table without unique name"
    );
    failures += expect_catalog_tamper_rejected(
        "missing_parent_fk_index",
        NULL,
        0U,
        tamper_remove_parent_foreign_key_index,
        "reject missing parent foreign-key lookup index"
    );
    failures += expect_catalog_tamper_rejected(
        "orphan_column",
        base_table_setup,
        sizeof(base_table_setup) / sizeof(base_table_setup[0]),
        tamper_orphan_column,
        "reject orphaned catalog column"
    );
    failures += expect_catalog_tamper_rejected(
        "column_ordinal_gap",
        base_table_setup,
        sizeof(base_table_setup) / sizeof(base_table_setup[0]),
        tamper_gap_column_ordinal,
        "reject catalog column ordinal gap"
    );
    failures += expect_catalog_tamper_rejected(
        "cross_index_ownership",
        cross_index_setup,
        sizeof(cross_index_setup) / sizeof(cross_index_setup[0]),
        tamper_cross_index_ownership,
        "reject cross-table catalog index part"
    );
    failures += expect_catalog_tamper_rejected(
        "orphan_view_source",
        view_setup,
        sizeof(view_setup) / sizeof(view_setup[0]),
        tamper_orphan_view_source,
        "reject orphaned catalog view source"
    );
    failures += expect_catalog_tamper_rejected(
        "orphan_fk_parent",
        foreign_key_setup,
        sizeof(foreign_key_setup) / sizeof(foreign_key_setup[0]),
        tamper_orphan_foreign_key_parent,
        "reject orphaned catalog foreign-key parent"
    );
    failures += expect_catalog_tamper_rejected(
        "future_generation",
        base_table_setup,
        sizeof(base_table_setup) / sizeof(base_table_setup[0]),
        tamper_future_descriptor_generation,
        "reject future descriptor generation"
    );
    failures += expect_catalog_tamper_rejected(
        "missing_physical_table",
        base_table_setup,
        sizeof(base_table_setup) / sizeof(base_table_setup[0]),
        tamper_drop_physical_table,
        "reject missing physical table"
    );
    failures += expect_catalog_tamper_rejected(
        "missing_physical_index",
        indexed_table_setup,
        sizeof(indexed_table_setup) / sizeof(indexed_table_setup[0]),
        tamper_drop_physical_index,
        "reject missing physical index"
    );
    failures += expect_catalog_tamper_rejected(
        "missing_physical_column",
        base_table_setup,
        sizeof(base_table_setup) / sizeof(base_table_setup[0]),
        tamper_drop_physical_column,
        "reject missing physical column"
    );
    failures += expect_catalog_tamper_rejected(
        "undeclared_physical_column",
        unindexed_table_setup,
        sizeof(unindexed_table_setup) / sizeof(unindexed_table_setup[0]),
        tamper_delete_catalog_column,
        "reject undeclared physical column"
    );
    failures += expect_catalog_tamper_rejected(
        "wrong_physical_index_definition",
        indexed_table_setup,
        sizeof(indexed_table_setup) / sizeof(indexed_table_setup[0]),
        tamper_replace_physical_index_definition,
        "reject mismatched physical index definition"
    );
    return failures;
}

static int expect_catalog_tamper_rejected(
    const char *name,
    const char *const *setup_sql,
    size_t setup_count,
    catalog_tamper_fn tamper,
    const char *context
) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open catalog tamper file");
    for (size_t index = 0U; failures == 0 && index < setup_count; ++index) {
        failures += execute_mylite_statement(database, setup_sql[index]);
    }
    sqlite = mylite_connection_sqlite_for_test(database);
    if (failures == 0 && sqlite != NULL) {
        failures += tamper(sqlite);
    }
    mylite_close(database);
    database = NULL;
    if (failures == 0) {
        failures += expect_int(mylite_open(path, &database), MYLITE_ERROR, context);
        failures += expect_true(database == NULL, "catalog integrity rejection leaves output null");
    }
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int execute_mylite_statement(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    mylite_result_free(result);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "MyLite setup statement failed: %s\n", sql);
        return 1;
    }
    return 0;
}

static int tamper_remove_state_check(sqlite3 *sqlite) {
    return execute_sql(
        sqlite,
        "ALTER TABLE _mylite_catalog_state RENAME TO _mylite_bad_state;"
        "CREATE TABLE _mylite_catalog_state ("
        "singleton_id INTEGER PRIMARY KEY, schema_version INTEGER NOT NULL, "
        "minimum_reader_schema_version INTEGER NOT NULL, catalog_generation INTEGER NOT NULL, "
        "created_with_file_format_version INTEGER NOT NULL);"
        "INSERT INTO _mylite_catalog_state SELECT * FROM _mylite_bad_state;"
        "DROP TABLE _mylite_bad_state"
    );
}

static int tamper_remove_schema_primary_key(sqlite3 *sqlite) {
    return execute_sql(
        sqlite,
        "ALTER TABLE _mylite_catalog_schemas RENAME TO _mylite_bad_schemas;"
        "CREATE TABLE _mylite_catalog_schemas ("
        "schema_id INTEGER, name TEXT NOT NULL UNIQUE, default_charset TEXT NOT NULL, "
        "default_collation TEXT NOT NULL, descriptor_version INTEGER NOT NULL, "
        "created_catalog_generation INTEGER NOT NULL, updated_catalog_generation INTEGER NOT NULL);"
        "INSERT INTO _mylite_catalog_schemas SELECT * FROM _mylite_bad_schemas;"
        "DROP TABLE _mylite_bad_schemas"
    );
}

static int tamper_remove_schema_unique_key(sqlite3 *sqlite) {
    return execute_sql(
        sqlite,
        "ALTER TABLE _mylite_catalog_schemas RENAME TO _mylite_bad_schemas;"
        "CREATE TABLE _mylite_catalog_schemas ("
        "schema_id INTEGER PRIMARY KEY, name TEXT NOT NULL, default_charset TEXT NOT NULL, "
        "default_collation TEXT NOT NULL, descriptor_version INTEGER NOT NULL, "
        "created_catalog_generation INTEGER NOT NULL, updated_catalog_generation INTEGER NOT NULL);"
        "INSERT INTO _mylite_catalog_schemas SELECT * FROM _mylite_bad_schemas;"
        "DROP TABLE _mylite_bad_schemas"
    );
}

static int tamper_remove_parent_foreign_key_index(sqlite3 *sqlite) {
    return execute_sql(sqlite, "DROP INDEX _mylite_catalog_foreign_keys_parent_table_id");
}

static int tamper_orphan_column(sqlite3 *sqlite) {
    return execute_sql(
        sqlite,
        "UPDATE _mylite_catalog_columns SET table_id = 999999 WHERE name = 'value'"
    );
}

static int tamper_gap_column_ordinal(sqlite3 *sqlite) {
    return execute_sql(
        sqlite,
        "UPDATE _mylite_catalog_columns SET ordinal_position = 3 WHERE name = 'value'"
    );
}

static int tamper_cross_index_ownership(sqlite3 *sqlite) {
    return execute_sql(
        sqlite,
        "UPDATE _mylite_catalog_index_columns SET table_id = "
        "(SELECT MAX(table_id) FROM _mylite_catalog_tables) WHERE index_id = "
        "(SELECT index_id FROM _mylite_catalog_indexes WHERE name = 'idx_value')"
    );
}

static int tamper_orphan_view_source(sqlite3 *sqlite) {
    return execute_sql(sqlite, "UPDATE _mylite_catalog_views SET source_table_id = 999999");
}

static int tamper_orphan_foreign_key_parent(sqlite3 *sqlite) {
    return execute_sql(sqlite, "UPDATE _mylite_catalog_foreign_keys SET parent_table_id = 999999");
}

static int tamper_future_descriptor_generation(sqlite3 *sqlite) {
    return execute_sql(
        sqlite,
        "UPDATE _mylite_catalog_tables SET updated_catalog_generation = "
        "(SELECT catalog_generation + 1 FROM _mylite_catalog_state)"
    );
}

static int tamper_drop_physical_table(sqlite3 *sqlite) {
    char physical_name[sql_buffer_capacity];
    char sql[sql_buffer_capacity];
    int rc = query_single_text(
        sqlite,
        "SELECT physical_name FROM _mylite_catalog_tables WHERE kind = 1 LIMIT 1",
        physical_name,
        sizeof(physical_name)
    );
    int written = snprintf(sql, sizeof(sql), "DROP TABLE \"%s\"", physical_name);

    if (rc != 0 || written < 0 || (size_t)written >= sizeof(sql)) {
        return 1;
    }
    return execute_sql(sqlite, sql);
}

static int tamper_drop_physical_index(sqlite3 *sqlite) {
    char physical_name[sql_buffer_capacity];
    char sql[sql_buffer_capacity];
    int rc = query_single_text(
        sqlite,
        "SELECT physical_name FROM _mylite_catalog_indexes WHERE name = 'idx_value'",
        physical_name,
        sizeof(physical_name)
    );
    int written = snprintf(sql, sizeof(sql), "DROP INDEX \"%s\"", physical_name);

    if (rc != 0 || written < 0 || (size_t)written >= sizeof(sql)) {
        return 1;
    }
    return execute_sql(sqlite, sql);
}

static int tamper_drop_physical_column(sqlite3 *sqlite) {
    char physical_name[sql_buffer_capacity];
    char sql[sql_buffer_capacity];
    int rc = query_single_text(
        sqlite,
        "SELECT physical_name FROM _mylite_catalog_tables WHERE kind = 1 LIMIT 1",
        physical_name,
        sizeof(physical_name)
    );
    int written = snprintf(sql, sizeof(sql), "ALTER TABLE \"%s\" DROP COLUMN value", physical_name);

    if (rc != 0 || written < 0 || (size_t)written >= sizeof(sql)) {
        return 1;
    }
    return execute_sql(sqlite, sql);
}

static int tamper_delete_catalog_column(sqlite3 *sqlite) {
    return execute_sql(sqlite, "DELETE FROM _mylite_catalog_columns WHERE name = 'value'");
}

static int tamper_replace_physical_index_definition(sqlite3 *sqlite) {
    char index_name[sql_buffer_capacity];
    char table_name[sql_buffer_capacity];
    char sql[(sql_buffer_capacity * 3) + 64];
    int rc = query_single_text(
        sqlite,
        "SELECT physical_name FROM _mylite_catalog_indexes WHERE name = 'idx_value'",
        index_name,
        sizeof(index_name)
    );
    int written = 0;

    if (rc == 0) {
        rc = query_single_text(
            sqlite,
            "SELECT physical_name FROM _mylite_catalog_tables WHERE kind = 1 LIMIT 1",
            table_name,
            sizeof(table_name)
        );
    }
    written = snprintf(
        sql,
        sizeof(sql),
        "DROP INDEX \"%s\"; CREATE INDEX \"%s\" ON \"%s\" (id)",
        index_name,
        index_name,
        table_name
    );
    if (rc != 0 || written < 0 || (size_t)written >= sizeof(sql)) {
        return 1;
    }
    return execute_sql(sqlite, sql);
}

static int test_zero_initialized_catalog_cleanup(void) {
    struct mylite_catalog catalog = {.initialized = false};
    int failures = 0;

    mylite_catalog_deinit(NULL);
    mylite_catalog_deinit(&catalog);
    failures += expect_bool(catalog.initialized, false, "zero catalog initialized flag");
    failures += expect_uint64(catalog.generation, 0U, "zero catalog generation");
    failures +=
        expect_bool(catalog.descriptor_cache_is_valid, false, "zero catalog cache validity");
    failures += expect_bool(
        mylite_catalog_name_is_reserved("_mylite_catalog_state"),
        true,
        "catalog state name is reserved"
    );
    failures += expect_bool(
        mylite_catalog_name_is_reserved("application_table"),
        false,
        "ordinary name is not reserved"
    );
    failures += expect_int(
        mylite_catalog_create_schema(NULL, "app", NULL),
        MYLITE_MISUSE,
        "schema creation rejects NULL database"
    );

    return failures;
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
        "%s/mylite_catalog_foundation_%d_%s.mylite",
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
    return (int)getpid();
#endif
}

static void remove_related_files(const char *path) {
    (void)remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    int failures = 0;

    if (file == NULL) {
        fprintf(stderr, "open %s for reading failed\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "seek %s to %ld failed\n", path, offset);
        failures += 1;
    } else if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "read %zu bytes from %s at %ld failed\n", size, path, offset);
        failures += 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "close %s after reading failed\n", path);
        failures += 1;
    }

    return failures;
}

static int execute_sql(sqlite3 *connection, const char *sql) {
    char *message = NULL;
    int rc = sqlite3_exec(connection, sql, NULL, NULL, &message);

    if (rc != SQLITE_OK) {
        fprintf(
            stderr,
            "execute SQLite SQL \"%s\": error %d: %s\n",
            sql,
            rc,
            message == NULL ? "(no message)" : message
        );
        sqlite3_free(message);
        return 1;
    }

    return 0;
}

static int query_catalog_table_count(sqlite3 *connection, int *out_count) {
    static const char *sql = "SELECT count(*) FROM sqlite_master "
                             "WHERE type = 'table' "
                             "AND name IN ("
                             "'_mylite_catalog_state',"
                             "'_mylite_catalog_schemas',"
                             "'_mylite_catalog_tables',"
                             "'_mylite_catalog_columns',"
                             "'_mylite_catalog_indexes',"
                             "'_mylite_catalog_index_columns'"
                             ")";

    return query_single_int(connection, sql, out_count);
}

static int query_single_int(sqlite3 *connection, const char *sql, int *out_value) {
    enum { sqlite_use_nul_terminated_string = -1 };

    sqlite3_stmt *statement = NULL;
    int rc = SQLITE_OK;

    *out_value = 0;
    rc = sqlite3_prepare_v2(connection, sql, sqlite_use_nul_terminated_string, &statement, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "prepare SQLite SQL \"%s\": error %d\n", sql, rc);
        return 1;
    }

    rc = sqlite3_step(statement);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "step SQLite SQL \"%s\": error %d\n", sql, rc);
        (void)sqlite3_finalize(statement);
        return 1;
    }
    *out_value = sqlite3_column_int(statement, 0);

    rc = sqlite3_finalize(statement);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "finalize SQLite SQL \"%s\": error %d\n", sql, rc);
        return 1;
    }

    return 0;
}

static int query_single_text(
    sqlite3 *connection,
    const char *sql,
    char *destination,
    size_t destination_size
) {
    enum { sqlite_use_nul_terminated_string = -1 };

    sqlite3_stmt *statement = NULL;
    const unsigned char *value = NULL;
    int rc = SQLITE_OK;
    int written = 0;

    if (destination == NULL || destination_size == 0U) {
        return 1;
    }
    destination[0] = '\0';
    rc = sqlite3_prepare_v2(connection, sql, sqlite_use_nul_terminated_string, &statement, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "prepare SQLite SQL \"%s\": error %d\n", sql, rc);
        return 1;
    }
    rc = sqlite3_step(statement);
    if (rc != SQLITE_ROW || sqlite3_column_type(statement, 0) != SQLITE_TEXT) {
        fprintf(stderr, "step SQLite text SQL \"%s\": error %d\n", sql, rc);
        (void)sqlite3_finalize(statement);
        return 1;
    }
    value = sqlite3_column_text(statement, 0);
    written =
        snprintf(destination, destination_size, "%s", value == NULL ? "" : (const char *)value);
    if (written < 0 || (size_t)written >= destination_size) {
        (void)sqlite3_finalize(statement);
        return 1;
    }
    rc = sqlite3_finalize(statement);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "finalize SQLite SQL \"%s\": error %d\n", sql, rc);
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

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %" PRId64 ", got %" PRId64 "\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %" PRIu64 ", got %" PRIu64 "\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_bool(bool actual, bool expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, (int)expected, (int)actual);
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
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", context, expected, actual);
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
