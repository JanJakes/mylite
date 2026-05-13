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
    path_suffix_capacity = 16,
    source_id_default = 7,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_table_exists = 1050,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_unknown = 1105,
    mysql_error_table_does_not_exist = 1146,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_column_descriptor {
    const char *name;
    const char *logical_type;
    bool is_nullable;
    bool is_visible;
    enum mylite_catalog_column_default_kind default_kind;
    int64_t default_integer;
};

static int test_create_table_like_success_persistence_and_preamble(void);
static int test_create_table_like_diagnostics(void);
static int test_create_table_like_after_source_rename_and_drop(void);
static int test_create_table_like_physical_create_failure_rolls_back_catalog(void);
static int test_independent_create_table_like_handles(void);
static int seed_like_source(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_create_like_ok(mylite_db *database, const char *sql);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_column_matches(
    mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    struct expected_column_descriptor expected
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int execute_sql(sqlite3 *connection, const char *sql);
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

    failures += test_create_table_like_success_persistence_and_preamble();
    failures += test_create_table_like_diagnostics();
    failures += test_create_table_like_after_source_rename_and_drop();
    failures += test_create_table_like_physical_create_failure_rolls_back_catalog();
    failures += test_independent_create_table_like_handles();

    return failures == 0 ? 0 : 1;
}

static int test_create_table_like_success_persistence_and_preamble(void) {
    static const char *const status_rows[] = {"0", "0", "0"};
    static const char *const clone_count_rows[] = {"0"};
    static const char *const source_rows[] = {"1", "2", "3", "4"};
    static const char *const clone_rows[] = {"8", NULL, "9", "10"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    struct mylite_catalog_schema_descriptor app_schema = {0};
    struct mylite_catalog_schema_descriptor other_schema = {0};
    struct mylite_catalog_table_descriptor source_table = {0};
    struct mylite_catalog_table_descriptor clone_table = {0};
    uint64_t catalog_generation_before = 0U;
    uint64_t sqlite_generation_before = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "CREATE DATABASE other");
    failures += seed_like_source(database);
    failures += execute_statement_ok(
        database,
        "INSERT INTO app.source (id, n, b, hidden) VALUES (1, 2, 3, 4)"
    );
    failures += execute_statement_ok(database, "ALTER TABLE app.source ALTER hidden DROP DEFAULT");

    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        catalog_generation_before = catalog->generation;
    }
    if (session != NULL) {
        sqlite_generation_before = session->sqlite_schema_generation;
    }

    failures +=
        expect_create_like_ok(database, "CREATE TABLE other.qualified_clone LIKE app.source");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, @@error_count",
            .values = status_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "schema-qualified create-like status without default schema",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM other.qualified_clone",
            .values = clone_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "create-like target is empty",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, b, hidden FROM app.source ORDER BY id",
            .values = source_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "create-like preserves source rows",
        }
    );
    failures += execute_statement_ok(
        database,
        "INSERT INTO other.qualified_clone (id, n, b, hidden) VALUES (8, NULL, 9, 10)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, b, hidden FROM other.qualified_clone ORDER BY id",
            .values = clone_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "create-like clone accepts explicit cloned columns",
        }
    );

    failures += execute_statement_ok(database, "USE app");
    failures += expect_create_like_ok(database, "CREATE TABLE paren_clone (LIKE source)");

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &app_schema),
        MYLITE_OK,
        "read app schema"
    );
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "other", &other_schema),
        MYLITE_OK,
        "read other schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, app_schema.schema_id, "source", &source_table),
        MYLITE_OK,
        "read source table"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(
            database,
            other_schema.schema_id,
            "qualified_clone",
            &clone_table
        ),
        MYLITE_OK,
        "read clone table"
    );
    failures += expect_true(
        clone_table.table_id != source_table.table_id,
        "create-like allocates a new table id"
    );
    failures += expect_true(
        strcmp(clone_table.physical_name, source_table.physical_name) != 0,
        "create-like allocates a new physical name"
    );
    failures += expect_uint64(clone_table.descriptor_version, 1U, "clone table descriptor version");
    failures +=
        expect_text(clone_table.default_charset, source_table.default_charset, "clone charset");
    failures += expect_text(
        clone_table.default_collation,
        source_table.default_collation,
        "clone collation"
    );
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            catalog_generation_before + 2U,
            "create-like advances catalog generation for each created table"
        );
    }
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before + 2U,
            "create-like advances SQLite schema generation for each physical table"
        );
    }

    failures += expect_column_matches(
        database,
        &clone_table,
        (struct expected_column_descriptor){
            .name = "id",
            .logical_type = "INT",
            .is_nullable = false,
            .is_visible = true,
            .default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER,
            .default_integer = source_id_default,
        }
    );
    failures += expect_column_matches(
        database,
        &clone_table,
        (struct expected_column_descriptor){
            .name = "n",
            .logical_type = "INT",
            .is_nullable = true,
            .is_visible = true,
            .default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NONE,
            .default_integer = 0,
        }
    );
    failures += expect_column_matches(
        database,
        &clone_table,
        (struct expected_column_descriptor){
            .name = "b",
            .logical_type = "BIGINT UNSIGNED",
            .is_nullable = false,
            .is_visible = true,
            .default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NONE,
            .default_integer = 0,
        }
    );
    failures += expect_column_matches(
        database,
        &clone_table,
        (struct expected_column_descriptor){
            .name = "hidden",
            .logical_type = "INT",
            .is_nullable = true,
            .is_visible = false,
            .default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT,
            .default_integer = 0,
        }
    );

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read create-like preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "create-like preserves MyLite preamble"
    );

    mylite_result_free(result);
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n, b, hidden FROM other.qualified_clone ORDER BY id",
            .values = clone_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "create-like clone row persists after reopen",
        }
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(
            database,
            other_schema.schema_id,
            "qualified_clone",
            &clone_table
        ),
        MYLITE_OK,
        "read clone table after reopen"
    );
    failures += expect_column_matches(
        database,
        &clone_table,
        (struct expected_column_descriptor){
            .name = "hidden",
            .logical_type = "INT",
            .is_nullable = true,
            .is_visible = false,
            .default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT,
            .default_integer = 0,
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_create_table_like_diagnostics(void) {
    static const char *const if_not_exists_status[] = {"0", "1", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "CREATE DATABASE other");
    failures += execute_statement_ok(database, "CREATE TABLE app.src (id INT)");
    failures += execute_statement_ok(database, "CREATE TABLE app.existing (id INT)");

    failures += execute_error(
        database,
        "CREATE TABLE no_default_target LIKE app.src",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE app.target_source_unqualified LIKE src",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE nosuch_schema.dst LIKE app.src",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'nosuch_schema'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE app.dst_unknown_source_schema LIKE nosuch_schema.src",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'nosuch_schema'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE nosuch_target_schema.dst LIKE nosuch_source_schema.src",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'nosuch_source_schema'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE nosuch_target_schema.dst LIKE app.missing_source_precedence",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_source_precedence' doesn't exist",
        }
    );

    failures += execute_statement_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE IF NOT EXISTS existing LIKE src", &result);
    failures += expect_empty_result(result, "create-like if-not-exists existing target result");
    failures += expect_size(
        mylite_result_warning_count(result),
        1U,
        "create-like if-not-exists result warning count"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, @@error_count",
            .values = if_not_exists_status,
            .column_count = 3U,
            .row_count = 1U,
            .context = "create-like if-not-exists status variables",
        }
    );

    failures += execute_error(
        database,
        "CREATE TABLE dst_missing_source LIKE missing",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE existing LIKE src",
        (struct expected_sql_error){
            .code = mysql_error_table_exists,
            .sqlstate = "42S01",
            .message_part = "Table 'existing' already exists",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE IF NOT EXISTS existing LIKE missing_source",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_source' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE _mylite_clone LIKE src",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_clone'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE clone_reserved_source LIKE _mylite_source",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_source'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE mixed_like (LIKE src, extra INT)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE like_with_options LIKE src ENGINE=InnoDB",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TEMPORARY TABLE temp_like LIKE src",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_statement_ok(database, "CREATE TABLE future_src (id INT)");
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += expect_true(sqlite != NULL, "read SQLite handle for future descriptor test");
    if (sqlite != NULL) {
        failures += execute_sql(
            sqlite,
            "UPDATE _mylite_catalog_columns "
            "SET logical_type = 'FUTURE_INT' "
            "WHERE name = 'id' "
            "AND table_id = ("
            "SELECT table_id FROM _mylite_catalog_tables "
            "WHERE name = 'future_src' "
            "AND schema_id = ("
            "SELECT schema_id FROM _mylite_catalog_schemas WHERE name = 'app'))"
        );
    }
    failures += execute_error(
        database,
        "CREATE TABLE future_clone LIKE future_src",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "CREATE TABLE LIKE supports only integer, string, binary string, decimal, "
                "approximate numeric, DATE, TIME, DATETIME, and TIMESTAMP descriptor columns",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_create_table_like_after_source_rename_and_drop(void) {
    static const char *const clone_rows[] = {"4"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "source_rename_drop") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open rename/drop file");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "CREATE TABLE app.src (id INT)");
    failures += execute_statement_ok(database, "RENAME TABLE app.src TO app.renamed_src");
    failures += execute_statement_ok(database, "CREATE TABLE app.clone LIKE app.renamed_src");
    failures += execute_statement_ok(database, "INSERT INTO app.clone VALUES (4)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM app.clone ORDER BY id",
            .values = clone_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "create-like after source rename",
        }
    );
    failures += execute_statement_ok(database, "DROP TABLE app.renamed_src");
    failures += execute_error(
        database,
        "CREATE TABLE app.after_drop LIKE app.renamed_src",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_src' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_create_table_like_physical_create_failure_rolls_back_catalog(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    const struct mylite_catalog *catalog = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "physical_failure") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open physical failure file");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "CREATE TABLE app.src (id INT)");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += execute_sql(sqlite, "CREATE TABLE _mylite_user_table_2(conflict INTEGER)");
    }
    failures += execute_error(
        database,
        "CREATE TABLE app.conflicted LIKE app.src",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite schema operation failed",
        }
    );
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read physical failure schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "conflicted", &table),
        MYLITE_ERROR,
        "failed create-like rolls back catalog target"
    );
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures +=
            expect_uint64(catalog->generation, 3U, "failed create-like leaves catalog generation");
    }

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_independent_create_table_like_handles(void) {
    static const char *const first_rows[] = {"1"};
    static const char *const second_rows[] = {"2"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += execute_statement_ok(first, "CREATE DATABASE app");
    failures += execute_statement_ok(second, "CREATE DATABASE app");
    failures += execute_statement_ok(first, "CREATE TABLE app.src (id INT)");
    failures += execute_statement_ok(second, "CREATE TABLE app.src (id INT)");
    failures += execute_statement_ok(first, "CREATE TABLE app.clone LIKE app.src");
    failures += execute_statement_ok(second, "CREATE TABLE app.clone LIKE app.src");
    failures += execute_statement_ok(first, "INSERT INTO app.clone VALUES (1)");
    failures += execute_statement_ok(second, "INSERT INTO app.clone VALUES (2)");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id FROM app.clone ORDER BY id",
            .values = first_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first handle clone state",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id FROM app.clone ORDER BY id",
            .values = second_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second handle clone state",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);

    return failures;
}

static int seed_like_source(mylite_db *database) {
    int failures = 0;

    failures += execute_statement_ok(
        database,
        "CREATE TABLE app.source ("
        "id INT NOT NULL DEFAULT 7, "
        "n INTEGER NULL DEFAULT NULL, "
        "b BIGINT UNSIGNED NOT NULL, "
        "hidden INT DEFAULT 3) COLLATE=utf8mb4_unicode_ci"
    );
    failures += execute_statement_ok(database, "ALTER TABLE app.source ALTER hidden SET INVISIBLE");

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected success for [%s], got %d (%s %s)\n",
            sql,
            rc,
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }

    return 0;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_empty_result(result, sql);
    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "expected error for [%s], got success\n", sql);
        failures += 1;
    } else {
        failures += expect_int(mylite_errcode(database), expected.code, sql);
        failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
        failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    }
    failures += expect_true(result == NULL, "failed statement leaves result null");
    mylite_result_free(result);

    return failures;
}

static int expect_create_like_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_empty_result(result, sql);
    failures += expect_int64(mylite_result_affected_rows(result), 0, "create-like affected rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "create-like warning count");
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }

    mylite_result_free(result);
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
    if (actual == NULL) {
        fprintf(stderr, "%s: expected [%s], got NULL\n", context, expected);
        return 1;
    }

    return expect_text(actual, expected, context);
}

static int expect_column_matches(
    mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    struct expected_column_descriptor expected
) {
    struct mylite_catalog_column_descriptor column = {0};
    int failures = 0;

    failures += expect_int(
        mylite_catalog_read_column_by_name(database, table->table_id, expected.name, &column),
        MYLITE_OK,
        expected.name
    );
    failures += expect_text(column.logical_type, expected.logical_type, expected.name);
    failures += expect_text(column.physical_type, "INTEGER", expected.name);
    failures += expect_bool(column.is_nullable, expected.is_nullable, expected.name);
    failures += expect_bool(column.is_visible, expected.is_visible, expected.name);
    failures += expect_int((int)column.default_kind, (int)expected.default_kind, expected.name);
    failures += expect_int64(column.default_integer, expected.default_integer, expected.name);

    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_create_table_like_%d_%s.mylite",
        current_process_id(),
        name
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
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    fclose(file);

    return read_count == size ? 0 : 1;
}

static int execute_sql(sqlite3 *connection, const char *sql) {
    char *error_message = NULL;
    int sqlite_rc = sqlite3_exec(connection, sql, NULL, NULL, &error_message);

    if (sqlite_rc != SQLITE_OK) {
        fprintf(stderr, "sqlite exec failed for [%s]: %s\n", sql, error_message);
        sqlite3_free(error_message);
        return 1;
    }

    return 0;
}

static int expect_empty_result(const mylite_result *result, const char *context) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), 0U, context);
    failures += expect_size(mylite_result_row_count(result), 0U, context);

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
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle
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
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }

    return 0;
}
