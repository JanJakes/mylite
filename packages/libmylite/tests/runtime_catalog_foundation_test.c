#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
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
    sqlite_header_size = 16,
    expected_catalog_table_count = 4,
    expected_catalog_generation_after_mutations = 5,
};

static int test_catalog_created_in_shifted_payload_without_preamble_changes(void);
static int test_reopen_preserves_catalog_rows_and_generation(void);
static int test_idempotent_catalog_initialization_across_repeated_opens(void);
static int test_independent_file_backed_handles_have_independent_catalog_state(void);
static int test_rejects_incompatible_and_incomplete_catalog_metadata(void);
static int test_zero_initialized_catalog_cleanup(void);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int execute_sql(sqlite3 *connection, const char *sql);
static int query_catalog_table_count(sqlite3 *connection, int *out_count);
static int query_single_int(sqlite3 *connection, const char *sql, int *out_value);
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
    failures += test_rejects_incompatible_and_incomplete_catalog_metadata();
    failures += test_zero_initialized_catalog_cleanup();

    return failures == 0 ? 0 : 1;
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
        failures += expect_uint64(catalog->schema_version, 1U, "catalog schema version");
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
    }
    failures += expect_int(catalog_tables, expected_catalog_table_count, "catalog table count");
    failures += expect_int(state_rows, 1, "catalog state singleton row");
    failures += expect_int(schema_rows, 0, "initial schema row count");

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
            &column
        ),
        MYLITE_OK,
        "create column descriptor"
    );
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
    failures += expect_uint64(table.descriptor_version, 2U, "table descriptor version");
    failures += expect_int(
        mylite_catalog_read_column_by_name(database, table.table_id, "id", &column),
        MYLITE_OK,
        "read reopened column"
    );
    failures += expect_text(column.logical_type, "BIGINT", "reopened logical type");
    failures += expect_text(column.physical_type, "INTEGER", "reopened physical type");
    failures += expect_bool(column.is_nullable, false, "reopened nullability");

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

static int test_rejects_incompatible_and_incomplete_catalog_metadata(void) {
    char path[test_path_capacity];
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
        failures += execute_sql(sqlite, "UPDATE _mylite_catalog_state SET schema_version = 2");
    }
    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_ERROR, "reject bad version");
    failures += expect_true(database == NULL, "bad version leaves output null");
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
                             "'_mylite_catalog_columns'"
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
