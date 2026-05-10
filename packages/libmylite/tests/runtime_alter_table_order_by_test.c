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
    mysql_error_unknown_column = 1054,
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

static int test_alter_table_order_by_success_persistence_and_preamble(void);
static int test_alter_table_order_by_diagnostics(void);
static int test_independent_alter_table_order_by_handles(void);
static int create_table_with_rows(
    mylite_db *database,
    const char *table_name,
    const char *insert_values
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_alter_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
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
static int execute_sql(sqlite3 *connection, const char *sql);
static int drop_physical_table(sqlite3 *connection, const char *physical_name);
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

    failures += test_alter_table_order_by_success_persistence_and_preamble();
    failures += test_alter_table_order_by_diagnostics();
    failures += test_independent_alter_table_order_by_handles();

    return failures == 0 ? 0 : 1;
}

static int test_alter_table_order_by_success_persistence_and_preamble(void) {
    static const char *const ordered_default_rows[] = {"1", "10", "2", "20", "3", "30"};
    static const char *const ordered_desc_rows[] = {"3", "30", "2", "20", "1", "10"};
    static const char *const ordered_multi_rows[] = {"1", "30", "1", "20", "2", "10"};
    static const char *const ordered_quoted_rows[] = {"2", "20", "1", "10", "3", "5"};
    static const char *const ordered_null_rows[] = {NULL, "99", "1", "10", "2", "20"};
    static const char *const qualified_desc_rows[] = {"2", "20", "1", "10"};
    static const char *const qualified_asc_rows[] = {"1", "10", "2", "20"};
    static const char *const status_rows[] = {"3", "0", "0"};
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

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += create_table_with_rows(database, "ordered_default", "(3, 30), (1, 10), (2, 20)");
    failures += create_table_with_rows(database, "ordered_desc", "(1, 10), (3, 30), (2, 20)");
    failures += create_table_with_rows(database, "ordered_multi", "(2, 10), (1, 20), (1, 30)");
    failures += execute_statement_ok(database, "CREATE TABLE ordered_quoted (id INT, `value` INT)");
    failures += execute_statement_ok(
        database,
        "INSERT INTO ordered_quoted VALUES (1, 10), (2, 20), (3, 5)"
    );
    failures += execute_statement_ok(database, "CREATE TABLE ordered_nulls (id INT NULL, v INT)");
    failures += execute_statement_ok(
        database,
        "INSERT INTO ordered_nulls VALUES (2, 20), (NULL, 99), (1, 10)"
    );
    failures += create_table_with_rows(database, "qualified_target", "(2, 20), (1, 10)");
    failures += execute_statement_ok(database, "CREATE TABLE empty_table (id INT)");

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read app schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(
            database,
            schema.schema_id,
            "ordered_default",
            &before_table
        ),
        MYLITE_OK,
        "read ordered_default before alter"
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        catalog_generation_before = catalog->generation;
    }
    if (session != NULL) {
        sqlite_generation_before = session->sqlite_schema_generation;
    }

    failures += expect_alter_ok(database, "ALTER TABLE ordered_default ORDER BY id", 3);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, @@error_count",
            .values = status_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "alter status variables",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM ordered_default",
            .values = ordered_default_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "default physical order",
        }
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(
            database,
            schema.schema_id,
            "ordered_default",
            &after_table
        ),
        MYLITE_OK,
        "read ordered_default after alter"
    );
    failures += expect_int64(after_table.table_id, before_table.table_id, "table id unchanged");
    failures += expect_text(
        after_table.physical_name,
        before_table.physical_name,
        "physical name restored"
    );
    failures += expect_uint64(
        after_table.descriptor_version,
        before_table.descriptor_version,
        "descriptor version unchanged"
    );
    failures += expect_uint64(
        after_table.updated_catalog_generation,
        before_table.updated_catalog_generation,
        "table updated generation unchanged"
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            catalog_generation_before,
            "alter order leaves catalog generation"
        );
    }
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before + 1U,
            "alter order bumps SQLite schema generation"
        );
    }

    failures += expect_alter_ok(database, "ALTER TABLE ordered_desc ORDER BY id DESC", 3);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM ordered_desc",
            .values = ordered_desc_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "descending physical order",
        }
    );
    failures += expect_alter_ok(database, "ALTER TABLE ordered_multi ORDER BY id ASC, v DESC", 3);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM ordered_multi",
            .values = ordered_multi_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "multi-key physical order",
        }
    );
    failures += expect_alter_ok(database, "ALTER TABLE ordered_quoted ORDER BY `value` DESC", 3);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, `value` FROM ordered_quoted",
            .values = ordered_quoted_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "quoted order key physical order",
        }
    );
    failures += expect_alter_ok(database, "ALTER TABLE ordered_nulls ORDER BY id", 3);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM ordered_nulls",
            .values = ordered_null_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "nullable ascending physical order",
        }
    );
    failures += expect_alter_ok(database, "ALTER TABLE empty_table ORDER BY id", 0);
    failures += expect_alter_ok(
        database,
        "ALTER TABLE app.qualified_target ORDER BY app.qualified_target.id DESC",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM qualified_target",
            .values = qualified_desc_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "schema-qualified target order",
        }
    );
    failures += expect_alter_ok(
        database,
        "ALTER TABLE qualified_target ORDER BY qualified_target.id ASC",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM qualified_target",
            .values = qualified_asc_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "table-qualified order key",
        }
    );

    failures += create_table_with_rows(database, "rename_source", "(2, 20), (1, 10)");
    failures += execute_statement_ok(database, "RENAME TABLE rename_source TO renamed_target");
    failures += execute_error(
        database,
        "ALTER TABLE rename_source ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.rename_source' doesn't exist",
        }
    );
    failures += expect_alter_ok(database, "ALTER TABLE renamed_target ORDER BY id", 2);
    failures += execute_statement_ok(database, "DROP TABLE renamed_target");
    failures += execute_error(
        database,
        "ALTER TABLE renamed_target ORDER BY id",
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
        "alter order preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM ordered_default",
            .values = ordered_default_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "reopened ordered rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alter_table_order_by_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_error(
        database,
        "ALTER TABLE numbers ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.numbers ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_reserved.numbers ORDER BY id",
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
        "ALTER TABLE missing_table ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_reserved ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers ORDER BY missing",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers ORDER BY other_table.id",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'other_table.id' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers ORDER BY other_schema.numbers.id",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'other_schema.numbers.id' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers ORDER BY 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers ORDER BY id + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers ORDER BY id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read diagnostics schema"
    );
    failures += expect_int(
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
        "ALTER TABLE numbers ORDER BY id",
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

static int test_independent_alter_table_order_by_handles(void) {
    static const char *const first_rows[] = {"3", "30", "2", "20", "1", "10"};
    static const char *const second_rows[] = {"2", "20", "1", "10", "3", "30"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent-first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent-second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += execute_statement_ok(first, "CREATE DATABASE app");
    failures += execute_statement_ok(first, "USE app");
    failures += create_table_with_rows(first, "numbers", "(2, 20), (1, 10), (3, 30)");
    failures += execute_statement_ok(second, "CREATE DATABASE app");
    failures += execute_statement_ok(second, "USE app");
    failures += create_table_with_rows(second, "numbers", "(2, 20), (1, 10), (3, 30)");

    failures += expect_alter_ok(first, "ALTER TABLE numbers ORDER BY id DESC", 3);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, v FROM numbers",
            .values = first_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "first handle ordered rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, v FROM numbers",
            .values = second_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "second handle unchanged rows",
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
    mylite_result *result = NULL;
    int written = snprintf(sql, sizeof(sql), "CREATE TABLE %s (id INT, v INT)", table_name);
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "create table SQL is too long for %s\n", table_name);
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);
    result = NULL;

    written = snprintf(sql, sizeof(sql), "INSERT INTO %s VALUES %s", table_name, insert_values);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "insert SQL is too long for %s\n", table_name);
        return failures + 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);
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

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_size(mylite_result_warning_count(result), 0U, sql);
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
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int expect_alter_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
    failures += expect_size(mylite_result_warning_count(result), 0U, sql);
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
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
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

    return expect_text(actual, expected, context);
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
        "%s/mylite_alter_table_order_by_%d_%s.mylite",
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

static int drop_physical_table(sqlite3 *connection, const char *physical_name) {
    char sql[sql_capacity];
    int written = snprintf(sql, sizeof(sql), "DROP TABLE \"%s\"", physical_name);

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "drop physical table SQL is too long\n");
        return 1;
    }

    return execute_sql(connection, sql);
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

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(stderr, "%s: expected '%s' to contain '%s'\n", context, actual, needle);
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
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
