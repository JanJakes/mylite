#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"

#include <inttypes.h>
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
    sql_capacity = 1024,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
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

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_alter_table_force_success_persistence_and_preamble(void);
static int test_alter_table_force_diagnostics(void);
static int test_independent_alter_table_force_handles(void);
static int create_table_with_rows(
    mylite_db *database,
    const char *table_name,
    const char *insert_values
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_force_ok(mylite_db *database, const char *sql);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int execute_sql(sqlite3 *connection, const char *sql);
static int drop_physical_table(sqlite3 *connection, const char *physical_name);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_alter_table_force_success_persistence_and_preamble();
    failures += test_alter_table_force_diagnostics();
    failures += test_independent_alter_table_force_handles();

    return failures == 0 ? 0 : 1;
}

static int test_alter_table_force_success_persistence_and_preamble(void) {
    static const char *const status_rows[] = {"0", "0", "0"};
    static const char *const forced_rows[] = {"1", "10", "2", "20", "3", "30"};
    static const char *const nullable_rows[] = {NULL, "99", "1", "10", "2", "20"};
    static const char *const qualified_rows[] = {"1", "10", "2", "20"};
    static const char *const renamed_rows[] = {"4", "40", "5", "50"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor before_table = {0};
    struct mylite_catalog_table_descriptor after_table = {0};
    uint64_t catalog_generation_before = 0U;
    uint64_t sqlite_generation_before = 0U;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "CREATE TABLE app.qualified_target (id INT, v INT)");
    failures +=
        execute_statement_ok(database, "INSERT INTO app.qualified_target VALUES (2, 20), (1, 10)");
    failures += expect_force_ok(database, "ALTER TABLE app.qualified_target FORCE");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, @@error_count",
            .values = status_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "schema-qualified force status without default schema",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM app.qualified_target ORDER BY id",
            .values = qualified_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "schema-qualified rows preserved",
        }
    );

    failures += execute_statement_ok(database, "USE app");
    failures += create_table_with_rows(database, "forced_table", "(3, 30), (1, 10), (2, 20)");
    failures += execute_statement_ok(database, "CREATE TABLE empty_table (id INT)");
    failures += execute_statement_ok(database, "CREATE TABLE nullable_table (id INT NULL, v INT)");
    failures += execute_statement_ok(
        database,
        "INSERT INTO nullable_table VALUES (2, 20), (NULL, 99), (1, 10)"
    );
    failures += create_table_with_rows(database, "rename_source", "(5, 50), (4, 40)");

    failures += mylite_test_expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read app schema"
    );
    failures += mylite_test_expect_int(
        mylite_catalog_read_table_by_name(
            database,
            schema.schema_id,
            "forced_table",
            &before_table
        ),
        MYLITE_OK,
        "read forced_table before force"
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        catalog_generation_before = catalog->generation;
    }
    if (session != NULL) {
        sqlite_generation_before = session->sqlite_schema_generation;
    }

    failures += expect_force_ok(database, "ALTER TABLE forced_table FORCE");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, @@error_count",
            .values = status_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "force status variables",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM forced_table ORDER BY id",
            .values = forced_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "forced rows preserved",
        }
    );
    failures += mylite_test_expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "forced_table", &after_table),
        MYLITE_OK,
        "read forced_table after force"
    );
    failures +=
        mylite_test_expect_int64(after_table.table_id, before_table.table_id, "table id unchanged");
    failures += mylite_test_expect_text(
        after_table.physical_name,
        before_table.physical_name,
        "physical name restored"
    );
    failures += mylite_test_expect_uint64(
        after_table.descriptor_version,
        before_table.descriptor_version,
        "descriptor version unchanged"
    );
    failures += mylite_test_expect_uint64(
        after_table.updated_catalog_generation,
        before_table.updated_catalog_generation,
        "table updated generation unchanged"
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += mylite_test_expect_uint64(
            catalog->generation,
            catalog_generation_before,
            "catalog generation unchanged"
        );
    }
    if (session != NULL) {
        failures += mylite_test_expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before + 1U,
            "sqlite schema generation incremented"
        );
    }

    failures += expect_force_ok(database, "ALTER TABLE empty_table FORCE");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, @@error_count",
            .values = status_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "empty force status variables",
        }
    );
    failures += expect_force_ok(database, "ALTER TABLE nullable_table FORCE");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM nullable_table ORDER BY id",
            .values = nullable_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "nullable rows preserved",
        }
    );

    failures +=
        execute_statement_ok(database, "ALTER TABLE rename_source RENAME TO renamed_target");
    failures += execute_error(
        database,
        "ALTER TABLE rename_source FORCE",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.rename_source' doesn't exist",
        }
    );
    failures += expect_force_ok(database, "ALTER TABLE renamed_target FORCE");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM renamed_target ORDER BY id",
            .values = renamed_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "renamed table rows preserved",
        }
    );
    failures += execute_statement_ok(database, "DROP TABLE renamed_target");
    failures += execute_error(
        database,
        "ALTER TABLE renamed_target FORCE",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_target' doesn't exist",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "file preamble unchanged"
    );

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM forced_table ORDER BY id",
            .values = forced_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "forced rows persisted after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alter_table_force_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_error(
        database,
        "ALTER TABLE numbers FORCE",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.numbers FORCE",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_reserved.numbers FORCE",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_reserved'",
        }
    );
    failures += execute_statement_ok(database, "USE app");
    failures += create_table_with_rows(database, "numbers", "(2, 20), (1, 10)");

    failures += execute_error(
        database,
        "ALTER TABLE missing_table FORCE",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_reserved FORCE",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers FORCE ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers FORCE, FORCE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "multi-action ALTER TABLE does not support this action",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers FORCE, ALGORITHM=INPLACE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "does not support ALGORITHM=INPLACE",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers FORCE LOCK=NONE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    failures += mylite_test_expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read diagnostics schema"
    );
    failures += mylite_test_expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "numbers", &table),
        MYLITE_OK,
        "read diagnostics table"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += drop_physical_table(sqlite, table.physical_name);
    }
    failures += execute_error(
        database,
        "ALTER TABLE numbers FORCE",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite row operation failed",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_alter_table_force_handles(void) {
    static const char *const first_rows[] = {"1", "100", "2", "200"};
    static const char *const second_rows[] = {"1", "10", "2", "20"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (mylite_test_make_path(first_path, sizeof(first_path), "independent-first") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "independent-second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures +=
        mylite_test_expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures +=
        mylite_test_expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += execute_statement_ok(first, "CREATE DATABASE app");
    failures += execute_statement_ok(first, "USE app");
    failures += create_table_with_rows(first, "numbers", "(1, 100), (2, 200)");
    failures += execute_statement_ok(second, "CREATE DATABASE app");
    failures += execute_statement_ok(second, "USE app");
    failures += create_table_with_rows(second, "numbers", "(1, 10), (2, 20)");

    failures += expect_force_ok(first, "ALTER TABLE numbers FORCE");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, v FROM numbers ORDER BY id",
            .values = first_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "first handle rows preserved",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, v FROM numbers ORDER BY id",
            .values = second_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "second handle rows unchanged",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
    return failures;
}

static int create_table_with_rows(
    mylite_db *database,
    const char *table_name,
    const char *insert_values
) {
    char sql[sql_capacity];
    int written = snprintf(sql, sizeof(sql), "CREATE TABLE %s (id INT, v INT)", table_name);
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "create table SQL is too long for %s\n", table_name);
        return 1;
    }
    failures += execute_statement_ok(database, sql);

    written = snprintf(sql, sizeof(sql), "INSERT INTO %s VALUES %s", table_name, insert_values);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "insert SQL is too long for %s\n", table_name);
        return failures + 1;
    }
    failures += execute_statement_ok(database, sql);

    return failures;
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

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, sql);
    mylite_result_free(result);

    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += mylite_test_expect_true(result == NULL, "failed execute leaves result null");
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    mylite_result_free(result);

    return failures;
}

static int expect_force_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, sql);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, sql);
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, query.context);
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
        return mylite_test_expect_true(actual == NULL, context);
    }

    return mylite_test_expect_text(actual, expected, context);
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

static int drop_physical_table(sqlite3 *connection, const char *physical_name) {
    char sql[sql_capacity];
    int written = snprintf(sql, sizeof(sql), "DROP TABLE \"%s\"", physical_name);

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "drop physical table SQL is too long\n");
        return 1;
    }

    return execute_sql(connection, sql);
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
