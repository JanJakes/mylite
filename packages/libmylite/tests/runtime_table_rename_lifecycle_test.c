#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "sqlite3.h"
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
    sqlite_sql_capacity = 512,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_table_exists = 1050,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_unknown = 1105,
    mysql_error_table_does_not_exist = 1146,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_rename_preserves_catalog_and_physical_state(void);
static int test_alter_table_rename_preserves_catalog_and_physical_state(void);
static int test_cross_schema_and_independent_name_resolution(void);
static int test_alter_table_rename_schema_resolution_and_noop(void);
static int test_failure_diagnostics_and_unwinding(void);
static int test_alter_table_rename_failure_diagnostics_and_unwinding(void);
static int test_catalog_failure_rolls_back_rename(void);
static int test_alter_table_rename_catalog_failure_rolls_back(void);
static int test_independent_file_backed_handles(void);
static int seed_schema(mylite_db *database, const char *name);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_show_table(mylite_db *database, const char *sql, const char *expected_name);
static int expect_show_tables_empty(mylite_db *database, const char *sql);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int execute_sql(sqlite3 *connection, const char *sql);
static int table_exists(sqlite3 *connection, const char *table_name, int *out_exists);
static int insert_physical_row(sqlite3 *connection, const char *table_name);
static int query_physical_row_count(sqlite3 *connection, const char *table_name, int *out_count);
static int expect_empty_result(const mylite_result *result, const char *context);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_bool(bool actual, bool expected, const char *context);
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

    failures += test_rename_preserves_catalog_and_physical_state();
    failures += test_alter_table_rename_preserves_catalog_and_physical_state();
    failures += test_cross_schema_and_independent_name_resolution();
    failures += test_alter_table_rename_schema_resolution_and_noop();
    failures += test_failure_diagnostics_and_unwinding();
    failures += test_alter_table_rename_failure_diagnostics_and_unwinding();
    failures += test_catalog_failure_rolls_back_rename();
    failures += test_alter_table_rename_catalog_failure_rolls_back();
    failures += test_independent_file_backed_handles();

    return failures == 0 ? 0 : 1;
}

static int test_rename_preserves_catalog_and_physical_state(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor before = {0};
    struct mylite_catalog_table_descriptor after = {0};
    struct mylite_catalog_column_descriptor before_column = {0};
    struct mylite_catalog_column_descriptor after_column = {0};
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t generation_before_rename = 0U;
    uint64_t sqlite_generation_before_rename = 0U;
    int has_physical_table = 0;
    int physical_rows = 0;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "preserve") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open rename file");
    failures += seed_schema(database, "app");
    failures += execute_ok(database, "USE app", &result);
    failures += expect_empty_result(result, "USE app result");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE rename_source (id INT, amount BIGINT NOT NULL)",
        &result
    );
    failures += expect_empty_result(result, "CREATE TABLE rename source result");
    mylite_result_free(result);
    result = NULL;

    failures += expect_show_table(database, "SHOW TABLES", "rename_source");

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read schema before rename"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "rename_source", &before),
        MYLITE_OK,
        "read table before rename"
    );
    failures += expect_int(
        mylite_catalog_read_column_by_name(database, before.table_id, "id", &before_column),
        MYLITE_OK,
        "read id column before rename"
    );
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        generation_before_rename = catalog->generation;
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        sqlite_generation_before_rename = session->sqlite_schema_generation;
    }

    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += insert_physical_row(sqlite, before.physical_name);
    }

    failures += execute_ok(database, "RENAME TABLE rename_source TO renamed_target", &result);
    failures += expect_empty_result(result, "RENAME TABLE result");
    mylite_result_free(result);
    result = NULL;

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            generation_before_rename + 1U,
            "generation after rename"
        );
        failures += expect_bool(
            catalog->descriptor_cache_is_valid,
            false,
            "rename invalidates descriptor cache"
        );
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before_rename,
            "rename leaves SQLite schema generation unchanged"
        );
    }
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "rename_source", &after),
        MYLITE_ERROR,
        "old table descriptor is gone"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "renamed_target", &after),
        MYLITE_OK,
        "renamed table descriptor exists"
    );
    failures += expect_int64(after.table_id, before.table_id, "rename keeps table id");
    failures += expect_text(after.name, "renamed_target", "renamed logical table name");
    failures +=
        expect_text(after.physical_name, before.physical_name, "rename keeps physical name");
    failures +=
        expect_uint64(after.descriptor_version, before.descriptor_version + 1U, "table version");
    failures += expect_uint64(
        after.created_catalog_generation,
        before.created_catalog_generation,
        "rename keeps created generation"
    );
    failures += expect_uint64(
        after.updated_catalog_generation,
        generation_before_rename + 1U,
        "rename updates table generation"
    );
    failures += expect_int(
        mylite_catalog_read_column_by_name(database, after.table_id, "id", &after_column),
        MYLITE_OK,
        "read id column after rename"
    );
    failures += expect_int64(after_column.column_id, before_column.column_id, "column id stable");
    failures +=
        expect_text(after_column.logical_type, before_column.logical_type, "column logical type");
    failures += expect_text(
        after_column.physical_type,
        before_column.physical_type,
        "column physical type"
    );
    failures += expect_bool(after_column.is_nullable, before_column.is_nullable, "column nullable");
    failures += expect_uint64(
        after_column.descriptor_version,
        before_column.descriptor_version,
        "column descriptor version unchanged"
    );
    failures += expect_uint64(
        after_column.updated_catalog_generation,
        before_column.updated_catalog_generation,
        "column updated generation unchanged"
    );

    has_physical_table = 0;
    if (sqlite != NULL) {
        failures += table_exists(sqlite, after.physical_name, &has_physical_table);
        failures += query_physical_row_count(sqlite, after.physical_name, &physical_rows);
    }
    failures += expect_int(has_physical_table, 1, "physical table remains after rename");
    failures += expect_int(physical_rows, 1, "physical table rows remain after rename");
    failures += expect_show_table(database, "SHOW TABLES", "renamed_target");

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "rename preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen renamed file");
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read reopened schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "renamed_target", &after),
        MYLITE_OK,
        "read reopened renamed table"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    has_physical_table = 0;
    physical_rows = 0;
    if (sqlite != NULL) {
        failures += table_exists(sqlite, after.physical_name, &has_physical_table);
        failures += query_physical_row_count(sqlite, after.physical_name, &physical_rows);
    }
    failures += expect_int(has_physical_table, 1, "reopened physical table exists");
    failures += expect_int(physical_rows, 1, "reopened physical table rows remain");

    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "DROP TABLE renamed_target", &result);
    failures += expect_empty_result(result, "drop renamed table result");
    mylite_result_free(result);
    result = NULL;
    has_physical_table = 1;
    if (sqlite != NULL) {
        failures += table_exists(sqlite, before.physical_name, &has_physical_table);
    }
    failures += expect_int(has_physical_table, 0, "drop removes renamed physical table");

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_alter_table_rename_preserves_catalog_and_physical_state(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor before = {0};
    struct mylite_catalog_table_descriptor after = {0};
    struct mylite_catalog_column_descriptor before_column = {0};
    struct mylite_catalog_column_descriptor after_column = {0};
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t generation_before_rename = 0U;
    uint64_t sqlite_generation_before_rename = 0U;
    int physical_rows = 0;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "alter_preserve") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open alter rename file");
    failures += seed_schema(database, "app");
    failures += execute_ok(database, "USE app", &result);
    failures += expect_empty_result(result, "USE app before alter rename");
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "CREATE TABLE alter_source (id INT, amount BIGINT NOT NULL)", &result);
    failures += expect_empty_result(result, "create alter source");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO alter_source VALUES (1, 10), (2, 20)", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 2, "insert alter source rows");
    mylite_result_free(result);
    result = NULL;

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read alter rename schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "alter_source", &before),
        MYLITE_OK,
        "read alter source before rename"
    );
    failures += expect_int(
        mylite_catalog_read_column_by_name(database, before.table_id, "id", &before_column),
        MYLITE_OK,
        "read alter source column before rename"
    );
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        generation_before_rename = catalog->generation;
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        sqlite_generation_before_rename = session->sqlite_schema_generation;
    }

    failures += execute_ok(database, "ALTER TABLE alter_source RENAME TO alter_target", &result);
    failures += expect_empty_result(result, "ALTER TABLE RENAME TO result");
    mylite_result_free(result);
    result = NULL;

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            generation_before_rename + 1U,
            "alter rename advances generation"
        );
        failures += expect_bool(
            catalog->descriptor_cache_is_valid,
            false,
            "alter rename invalidates descriptor cache"
        );
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before_rename,
            "alter rename leaves SQLite schema generation unchanged"
        );
    }
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "alter_source", &after),
        MYLITE_ERROR,
        "alter source descriptor is gone"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "alter_target", &after),
        MYLITE_OK,
        "alter target descriptor exists"
    );
    failures += expect_int64(after.table_id, before.table_id, "alter rename keeps table id");
    failures += expect_text(after.name, "alter_target", "alter renamed logical table name");
    failures +=
        expect_text(after.physical_name, before.physical_name, "alter rename keeps physical name");
    failures += expect_uint64(
        after.descriptor_version,
        before.descriptor_version + 1U,
        "alter rename table version"
    );
    failures += expect_int(
        mylite_catalog_read_column_by_name(database, after.table_id, "id", &after_column),
        MYLITE_OK,
        "read alter source column after rename"
    );
    failures += expect_int64(
        after_column.column_id,
        before_column.column_id,
        "alter rename keeps column id"
    );
    failures += expect_uint64(
        after_column.descriptor_version,
        before_column.descriptor_version,
        "alter rename leaves column version unchanged"
    );

    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += query_physical_row_count(sqlite, after.physical_name, &physical_rows);
    }
    failures += expect_int(physical_rows, 2, "alter rename preserves physical rows");
    failures += expect_show_table(database, "SHOW TABLES", "alter_target");

    failures += execute_ok(database, "SHOW CREATE TABLE alter_target", &result);
    failures += expect_size(mylite_result_column_count(result), 2U, "show create column count");
    failures += expect_size(mylite_result_row_count(result), 1U, "show create row count");
    failures +=
        expect_text(mylite_result_value_text(result, 0U, 0U), "alter_target", "show create table");
    failures += expect_contains(
        mylite_result_value_text(result, 0U, 1U),
        "CREATE TABLE `alter_target`",
        "show create uses renamed table"
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "UPDATE alter_target SET amount = 30 WHERE id = 2", &result);
    failures +=
        expect_int64(mylite_result_affected_rows(result), 1, "update renamed table affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT id, amount FROM alter_target ORDER BY id", &result);
    failures += expect_size(mylite_result_row_count(result), 2U, "renamed select row count");
    failures += expect_text(mylite_result_value_text(result, 0U, 0U), "1", "renamed row 1 id");
    failures += expect_text(mylite_result_value_text(result, 0U, 1U), "10", "renamed row 1 amount");
    failures += expect_text(mylite_result_value_text(result, 1U, 0U), "2", "renamed row 2 id");
    failures += expect_text(mylite_result_value_text(result, 1U, 1U), "30", "renamed row 2 amount");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "ALTER TABLE alter_target RENAME AS alter_final", &result);
    failures += expect_empty_result(result, "ALTER TABLE RENAME AS result");
    mylite_result_free(result);
    result = NULL;

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "alter rename preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen alter renamed file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT id, amount FROM alter_final ORDER BY id", &result);
    failures += expect_size(mylite_result_row_count(result), 2U, "reopened renamed row count");
    failures +=
        expect_text(mylite_result_value_text(result, 1U, 1U), "30", "reopened update value");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "TRUNCATE TABLE alter_final", &result);
    failures += expect_empty_result(result, "truncate after alter rename");
    mylite_result_free(result);
    result = NULL;

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_cross_schema_and_independent_name_resolution(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    struct mylite_catalog_schema_descriptor app = {0};
    struct mylite_catalog_schema_descriptor archive = {0};
    struct mylite_catalog_table_descriptor before_cross = {0};
    struct mylite_catalog_table_descriptor table = {0};
    int failures = 0;

    if (make_test_path(path, sizeof(path), "cross_schema") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open cross-schema file");
    failures += seed_schema(database, "app");
    failures += seed_schema(database, "archive");
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &app),
        MYLITE_OK,
        "read app schema"
    );
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "archive", &archive),
        MYLITE_OK,
        "read archive schema"
    );
    failures += execute_ok(database, "CREATE TABLE app.cross_source (id INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, app.schema_id, "cross_source", &before_cross),
        MYLITE_OK,
        "read cross-schema table before rename"
    );

    failures +=
        execute_ok(database, "RENAME TABLE app.cross_source TO archive.cross_target", &result);
    failures += expect_empty_result(result, "cross-schema rename result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, app.schema_id, "cross_source", &table),
        MYLITE_ERROR,
        "cross-schema source removed from app"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, archive.schema_id, "cross_target", &table),
        MYLITE_OK,
        "cross-schema target exists in archive"
    );
    failures += expect_int64(table.schema_id, archive.schema_id, "cross-schema target schema id");
    failures += expect_int64(table.table_id, before_cross.table_id, "cross-schema keeps table id");
    failures += expect_text(
        table.physical_name,
        before_cross.physical_name,
        "cross-schema keeps physical name"
    );
    failures += expect_uint64(
        table.descriptor_version,
        before_cross.descriptor_version + 1U,
        "cross-schema table version"
    );
    failures += expect_show_tables_empty(database, "SHOW TABLES FROM app");
    failures += expect_show_table(database, "SHOW TABLES FROM archive", "cross_target");

    failures += execute_ok(database, "CREATE TABLE app.default_target_source (id INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE archive", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "RENAME TABLE app.default_target_source TO default_target", &result);
    failures += expect_empty_result(result, "qualified source to default target result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, archive.schema_id, "default_target", &table),
        MYLITE_OK,
        "unqualified target uses selected schema"
    );

    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE source_unqualified (id INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "RENAME TABLE source_unqualified TO archive.target_qualified",
        &result
    );
    failures += expect_empty_result(result, "unqualified source to qualified target result");
    mylite_result_free(result);
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, archive.schema_id, "target_qualified", &table),
        MYLITE_OK,
        "qualified target uses named schema"
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_alter_table_rename_schema_resolution_and_noop(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    struct mylite_catalog_schema_descriptor app = {0};
    struct mylite_catalog_schema_descriptor archive = {0};
    struct mylite_catalog_table_descriptor before_noop = {0};
    struct mylite_catalog_table_descriptor after_noop = {0};
    struct mylite_catalog_table_descriptor table = {0};
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t generation_before_noop = 0U;
    uint64_t sqlite_generation_before_noop = 0U;
    bool descriptor_cache_before_noop = false;
    int physical_rows = 0;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "alter_schema_noop") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open alter schema file");
    failures += seed_schema(database, "app");
    failures += seed_schema(database, "archive");
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &app),
        MYLITE_OK,
        "read app schema for alter rename"
    );
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "archive", &archive),
        MYLITE_OK,
        "read archive schema for alter rename"
    );

    failures += execute_ok(database, "CREATE TABLE app.cross_source (id INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "ALTER TABLE app.cross_source RENAME archive.cross_target", &result);
    failures += expect_empty_result(result, "alter cross-schema rename result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, archive.schema_id, "cross_target", &table),
        MYLITE_OK,
        "alter cross-schema target exists"
    );
    failures +=
        expect_int64(table.schema_id, archive.schema_id, "alter cross-schema target schema");

    failures += execute_ok(database, "CREATE TABLE app.default_target_source (id INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE archive", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "ALTER TABLE app.default_target_source RENAME default_target",
        &result
    );
    failures += expect_empty_result(result, "alter qualified source to default target result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, archive.schema_id, "default_target", &table),
        MYLITE_OK,
        "alter unqualified target uses selected schema"
    );

    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE source_unqualified (id INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "ALTER TABLE source_unqualified RENAME archive.target_qualified",
        &result
    );
    failures += expect_empty_result(result, "alter unqualified source to qualified target result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, archive.schema_id, "target_qualified", &table),
        MYLITE_OK,
        "alter qualified target uses named schema"
    );

    failures += execute_ok(database, "CREATE TABLE same_name (id INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO same_name VALUES (7)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, app.schema_id, "same_name", &before_noop),
        MYLITE_OK,
        "read same-name table before alter noop"
    );
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        generation_before_noop = catalog->generation;
        descriptor_cache_before_noop = catalog->descriptor_cache_is_valid;
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        sqlite_generation_before_noop = session->sqlite_schema_generation;
    }

    failures += execute_ok(database, "ALTER TABLE same_name RENAME same_name", &result);
    failures += expect_empty_result(result, "alter same-name noop result");
    mylite_result_free(result);
    result = NULL;

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            generation_before_noop,
            "alter same-name noop leaves generation"
        );
        failures += expect_bool(
            catalog->descriptor_cache_is_valid,
            descriptor_cache_before_noop,
            "alter same-name noop leaves descriptor cache state"
        );
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before_noop,
            "alter same-name noop leaves SQLite schema generation"
        );
    }
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, app.schema_id, "same_name", &after_noop),
        MYLITE_OK,
        "read same-name table after alter noop"
    );
    failures +=
        expect_int64(after_noop.table_id, before_noop.table_id, "alter noop keeps table id");
    failures += expect_uint64(
        after_noop.descriptor_version,
        before_noop.descriptor_version,
        "alter noop keeps descriptor version"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += query_physical_row_count(sqlite, after_noop.physical_name, &physical_rows);
    }
    failures += expect_int(physical_rows, 1, "alter noop preserves physical row");
    failures += expect_show_table(database, "SHOW TABLES LIKE 'same_name'", "same_name");

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_failure_diagnostics_and_unwinding(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    const struct mylite_catalog *catalog = NULL;
    uint64_t generation_before_failures = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "failures") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open failure file");
    failures += seed_schema(database, "app");
    failures += seed_schema(database, "archive");
    failures += execute_ok(database, "CREATE TABLE app.qualified_source (id INT)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "RENAME TABLE app.qualified_source TO no_default_target",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );

    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE duplicate_target (id INT)", &result);
    mylite_result_free(result);
    result = NULL;

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        generation_before_failures = catalog->generation;
    }

    failures += execute_error(
        database,
        "RENAME TABLE missing_source TO missing_target",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_source' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "RENAME TABLE qualified_source TO duplicate_target",
        (struct expected_sql_error){
            .code = mysql_error_table_exists,
            .sqlstate = "42S01",
            .message_part = "Table 'duplicate_target' already exists",
        }
    );
    failures += execute_error(
        database,
        "RENAME TABLE duplicate_target TO duplicate_target",
        (struct expected_sql_error){
            .code = mysql_error_table_exists,
            .sqlstate = "42S01",
            .message_part = "Table 'duplicate_target' already exists",
        }
    );
    failures += execute_error(
        database,
        "RENAME TABLE duplicate_target TO missing_schema.new_name",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );
    failures += execute_error(
        database,
        "RENAME TABLE _mylite_reserved TO valid_target",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "RENAME TABLE duplicate_target TO _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "RENAME TABLE _mylite_schema.duplicate_target TO valid_target",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name",
        }
    );
    failures += execute_error(
        database,
        "RENAME TABLE duplicate_target TO _mylite_schema.valid_target",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name",
        }
    );
    failures += execute_error(
        database,
        "RENAME TABLE duplicate_target TO renamed_duplicate, qualified_source TO renamed_source",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE duplicate_target RENAME TABLE renamed_duplicate",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            generation_before_failures,
            "failed rename statements do not advance generation"
        );
    }
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read schema after failures"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "qualified_source", &table),
        MYLITE_OK,
        "source survives failed renames"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "renamed_source", &table),
        MYLITE_ERROR,
        "failed multi rename target absent"
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_alter_table_rename_failure_diagnostics_and_unwinding(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    const struct mylite_catalog *catalog = NULL;
    uint64_t generation_before_failures = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "alter_failures") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open alter failures file");
    failures += seed_schema(database, "app");
    failures += seed_schema(database, "archive");
    failures += execute_ok(database, "CREATE TABLE app.qualified_source (id INT)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "ALTER TABLE no_default_source RENAME no_default_target",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.missing_source RENAME no_default_target",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.missing_source RENAME app.missing_target",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );

    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE duplicate_target (id INT)", &result);
    mylite_result_free(result);
    result = NULL;

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        generation_before_failures = catalog->generation;
    }

    failures += execute_error(
        database,
        "ALTER TABLE missing_source RENAME missing_target",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE qualified_source RENAME duplicate_target",
        (struct expected_sql_error){
            .code = mysql_error_table_exists,
            .sqlstate = "42S01",
            .message_part = "already exists",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE qualified_source RENAME missing_schema.new_name",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_reserved RENAME valid_target",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE duplicate_target RENAME _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_schema.duplicate_target RENAME valid_target",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE duplicate_target RENAME _mylite_schema.valid_target",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE duplicate_target RENAME renamed_duplicate, ADD COLUMN added INT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE duplicate_target RENAME renamed_duplicate, RENAME final_name",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE duplicate_target RENAME TABLE renamed_duplicate",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE duplicate_target RENAME COLUMN id TO renamed_id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            generation_before_failures,
            "failed alter rename statements do not advance generation"
        );
    }
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read schema after alter failures"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "qualified_source", &table),
        MYLITE_OK,
        "source survives failed alter renames"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "renamed_duplicate", &table),
        MYLITE_ERROR,
        "failed alter rename target absent"
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_catalog_failure_rolls_back_rename(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    const struct mylite_catalog *catalog = NULL;
    uint64_t generation_before_failure = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "catalog_failure") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open catalog-failure file");
    failures += seed_schema(database, "app");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE blocked_source (id INT)", &result);
    mylite_result_free(result);
    result = NULL;

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        generation_before_failure = catalog->generation;
    }
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += execute_sql(
            sqlite,
            "CREATE TRIGGER block_table_rename "
            "BEFORE UPDATE OF name ON _mylite_catalog_tables "
            "BEGIN SELECT RAISE(ABORT, 'blocked catalog rename'); END"
        );
    }

    failures += execute_error(
        database,
        "RENAME TABLE blocked_source TO blocked_target",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "failed to rename table descriptor",
        }
    );
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            generation_before_failure,
            "catalog failure leaves generation unchanged"
        );
    }
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read schema after catalog failure"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "blocked_source", &table),
        MYLITE_OK,
        "catalog failure keeps old source name"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "blocked_target", &table),
        MYLITE_ERROR,
        "catalog failure leaves target absent"
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_alter_table_rename_catalog_failure_rolls_back(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    const struct mylite_catalog *catalog = NULL;
    uint64_t generation_before_failure = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "alter_catalog_failure") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        expect_int(mylite_open(path, &database), MYLITE_OK, "open alter catalog-failure file");
    failures += seed_schema(database, "app");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE blocked_source (id INT)", &result);
    mylite_result_free(result);
    result = NULL;

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        generation_before_failure = catalog->generation;
    }
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += execute_sql(
            sqlite,
            "CREATE TRIGGER block_alter_table_rename "
            "BEFORE UPDATE OF name ON _mylite_catalog_tables "
            "BEGIN SELECT RAISE(ABORT, 'blocked alter catalog rename'); END"
        );
    }

    failures += execute_error(
        database,
        "ALTER TABLE blocked_source RENAME blocked_target",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "failed to rename table descriptor",
        }
    );
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            generation_before_failure,
            "alter catalog failure leaves generation unchanged"
        );
    }
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read schema after alter catalog failure"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "blocked_source", &table),
        MYLITE_OK,
        "alter catalog failure keeps old source name"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "blocked_target", &table),
        MYLITE_ERROR,
        "alter catalog failure leaves target absent"
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_independent_file_backed_handles(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first rename file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second rename file");
    failures += seed_schema(first, "app");
    failures += seed_schema(second, "app");

    failures += execute_ok(first, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "CREATE TABLE first_source (id INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "RENAME TABLE first_source TO first_target", &result);
    failures += expect_empty_result(result, "first handle rename result");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(second, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_show_tables_empty(second, "SHOW TABLES");
    failures += execute_ok(second, "CREATE TABLE second_source (id INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "ALTER TABLE second_source RENAME second_target", &result);
    failures += expect_empty_result(result, "second handle alter rename result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_catalog_read_schema_by_name(first, "app", &schema),
        MYLITE_OK,
        "read first schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(first, schema.schema_id, "first_target", &table),
        MYLITE_OK,
        "first handle has renamed table"
    );
    failures += expect_int(
        mylite_catalog_read_schema_by_name(second, "app", &schema),
        MYLITE_OK,
        "read second schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(second, schema.schema_id, "first_target", &table),
        MYLITE_ERROR,
        "second handle lacks renamed table"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(second, schema.schema_id, "second_target", &table),
        MYLITE_OK,
        "second handle has alter-renamed table"
    );
    failures += expect_int(
        mylite_catalog_read_schema_by_name(first, "app", &schema),
        MYLITE_OK,
        "read first schema after second alter rename"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(first, schema.schema_id, "second_target", &table),
        MYLITE_ERROR,
        "first handle lacks second alter-renamed table"
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);

    return failures;
}

static int seed_schema(mylite_db *database, const char *name) {
    struct mylite_catalog_schema_descriptor schema = {0};

    return expect_int(
        mylite_catalog_create_schema(database, name, &schema),
        MYLITE_OK,
        "seed schema"
    );
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int expect_show_table(mylite_db *database, const char *sql, const char *expected_name) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 1U, sql);
    failures += expect_size(mylite_result_row_count(result), 1U, sql);
    failures += expect_text(mylite_result_value_text(result, 0U, 0U), expected_name, sql);
    mylite_result_free(result);

    return failures;
}

static int expect_show_tables_empty(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 1U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    mylite_result_free(result);

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
        "%s/mylite_table_rename_lifecycle_%d_%s.mylite",
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
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        (void)fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
        return 1;
    }

    return 0;
}

static int execute_sql(sqlite3 *connection, const char *sql) {
    int rc = sqlite3_exec(connection, sql, NULL, NULL, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQLite exec failed for '%s': %d\n", sql, rc);
        return 1;
    }

    return 0;
}

static int table_exists(sqlite3 *connection, const char *table_name, int *out_exists) {
    sqlite3_stmt *statement = NULL;
    int rc = sqlite3_prepare_v2(
        connection,
        "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name = ?1",
        -1,
        &statement,
        NULL
    );

    *out_exists = 0;
    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_text(statement, 1, table_name, -1, SQLITE_TRANSIENT);
    }
    if (rc == SQLITE_OK) {
        rc = sqlite3_step(statement);
        if (rc == SQLITE_ROW) {
            *out_exists = sqlite3_column_int(statement, 0) == 0 ? 0 : 1;
            rc = SQLITE_OK;
        }
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && rc == SQLITE_OK) {
        rc = SQLITE_ERROR;
    }
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to query table existence for %s: %d\n", table_name, rc);
        return 1;
    }

    return 0;
}

static int insert_physical_row(sqlite3 *connection, const char *table_name) {
    char sql[sqlite_sql_capacity];
    int written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO \"%s\" (\"id\", \"amount\") VALUES (7, 9)",
        table_name
    );

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "physical insert SQL is too long\n");
        return 1;
    }

    return execute_sql(connection, sql);
}

static int query_physical_row_count(sqlite3 *connection, const char *table_name, int *out_count) {
    char sql[sqlite_sql_capacity];
    sqlite3_stmt *statement = NULL;
    int written = snprintf(sql, sizeof(sql), "SELECT count(*) FROM \"%s\"", table_name);
    int rc = SQLITE_OK;

    *out_count = 0;
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "physical count SQL is too long\n");
        return 1;
    }

    rc = sqlite3_prepare_v2(connection, sql, -1, &statement, NULL);
    if (rc == SQLITE_OK) {
        rc = sqlite3_step(statement);
        if (rc == SQLITE_ROW) {
            *out_count = sqlite3_column_int(statement, 0);
            rc = SQLITE_OK;
        }
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && rc == SQLITE_OK) {
        rc = SQLITE_ERROR;
    }
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to count physical rows for %s: %d\n", table_name, rc);
        return 1;
    }

    return 0;
}

static int expect_empty_result(const mylite_result *result, const char *context) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), 0U, context);
    failures += expect_size(mylite_result_row_count(result), 0U, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    return failures;
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
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
        );
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

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
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
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected '%s', got '%s'\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected '%s' to contain '%s'\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
        );
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
    if (actual == NULL || expected == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }

    return 0;
}
