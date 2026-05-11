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
    physical_drop_sql_capacity = 128,
    expected_generation_after_schema = 2,
    expected_generation_after_create = 3,
    expected_generation_after_drop = 4,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_table_exists = 1050,
    mysql_error_unknown_table = 1051,
    mysql_error_duplicate_column = 1060,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_unknown = 1105,
    mysql_error_incorrect_column_name = 1166,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_public_api_and_result_cleanup(void);
static int test_create_show_drop_and_reopen(void);
static int test_failure_diagnostics_and_unwinding(void);
static int test_physical_create_failure_rolls_back_catalog(void);
static int test_physical_drop_failure_rolls_back_catalog(void);
static int test_independent_file_backed_handles(void);
static int seed_schema(mylite_db *database, const char *name);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int execute_sql(sqlite3 *connection, const char *sql);
static int table_exists(sqlite3 *connection, const char *table_name, int *out_exists);
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

    failures += test_public_api_and_result_cleanup();
    failures += test_create_show_drop_and_reopen();
    failures += test_failure_diagnostics_and_unwinding();
    failures += test_physical_create_failure_rolls_back_catalog();
    failures += test_physical_drop_failure_rolls_back_catalog();
    failures += test_independent_file_backed_handles();

    return failures == 0 ? 0 : 1;
}

static int test_public_api_and_result_cleanup(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    mylite_result_free(NULL);
    failures += expect_size(mylite_result_column_count(NULL), 0U, "NULL result column count");
    failures += expect_size(mylite_result_row_count(NULL), 0U, "NULL result row count");
    failures += expect_true(mylite_result_column_name(NULL, 0U) == NULL, "NULL column name");
    failures += expect_true(mylite_result_value_text(NULL, 0U, 0U) == NULL, "NULL value text");
    failures += expect_int64(mylite_result_affected_rows(NULL), 0, "NULL affected rows");
    failures += expect_size(mylite_result_warning_count(NULL), 0U, "NULL warning count");

    failures += expect_int(
        mylite_execute(NULL, "", 0U, &result),
        MYLITE_MISUSE,
        "execute rejects NULL database"
    );
    failures += expect_true(result == NULL, "NULL database leaves result null");

    if (make_test_path(path, sizeof(path), "api") != 0) {
        return failures + 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open API test file");
    failures += expect_int(
        mylite_execute(database, NULL, 0U, &result),
        MYLITE_MISUSE,
        "execute rejects NULL SQL"
    );
    failures += expect_true(result == NULL, "NULL SQL leaves result null");
    failures += expect_int(mylite_errcode(database), MYLITE_MISUSE, "NULL SQL diagnostic");
    failures += expect_int(
        mylite_execute(database, "", 0U, NULL),
        MYLITE_MISUSE,
        "execute rejects NULL output"
    );

    failures += execute_ok(database, "", &result);
    failures += expect_empty_result(result, "empty SQL result");
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_create_show_drop_and_reopen(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    struct mylite_catalog_column_descriptor column = {0};
    const struct mylite_catalog *catalog = NULL;
    sqlite3 *sqlite = NULL;
    int has_physical_table = 0;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "create_drop") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open lifecycle file");
    failures += seed_schema(database, "app");
    failures += execute_ok(database, "USE app", &result);
    failures += expect_empty_result(result, "USE result");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "CREATE TABLE simple_lifecycle (id INT, amount BIGINT NOT NULL, "
        "flags INTEGER UNSIGNED NULL)",
        &result
    );
    failures += expect_empty_result(result, "CREATE TABLE result");
    mylite_result_free(result);
    result = NULL;

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            expected_generation_after_create,
            "generation after create"
        );
    }
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read lifecycle schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "simple_lifecycle", &table),
        MYLITE_OK,
        "read created table"
    );
    failures += expect_text(table.name, "simple_lifecycle", "created table logical name");
    failures += expect_text(table.physical_name, "_mylite_user_table_1", "created physical name");
    failures += expect_uint64(table.descriptor_version, 1U, "table descriptor version");
    failures += expect_uint64(
        table.created_catalog_generation,
        expected_generation_after_create,
        "table created generation"
    );

    failures += expect_int(
        mylite_catalog_read_column_by_name(database, table.table_id, "id", &column),
        MYLITE_OK,
        "read id column"
    );
    failures += expect_text(column.logical_type, "INT", "id logical type");
    failures += expect_text(column.physical_type, "INTEGER", "id physical type");
    failures += expect_bool(column.is_nullable, true, "id nullable");
    failures += expect_uint64(
        column.created_catalog_generation,
        expected_generation_after_create,
        "id created generation"
    );
    failures += expect_int(
        mylite_catalog_read_column_by_name(database, table.table_id, "amount", &column),
        MYLITE_OK,
        "read amount column"
    );
    failures += expect_text(column.logical_type, "BIGINT", "amount logical type");
    failures += expect_bool(column.is_nullable, false, "amount not null");
    failures += expect_int(
        mylite_catalog_read_column_by_name(database, table.table_id, "flags", &column),
        MYLITE_OK,
        "read flags column"
    );
    failures += expect_text(column.logical_type, "INT UNSIGNED", "flags logical type");
    failures += expect_bool(column.is_nullable, true, "flags nullable");

    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += table_exists(sqlite, table.physical_name, &has_physical_table);
    }
    failures += expect_int(has_physical_table, 1, "created physical SQLite table exists");

    failures += execute_ok(database, "SHOW TABLES", &result);
    failures += expect_size(mylite_result_column_count(result), 1U, "SHOW TABLES column count");
    failures += expect_text(
        mylite_result_column_name(result, 0U),
        "Tables_in_app",
        "SHOW TABLES column name"
    );
    failures += expect_size(mylite_result_row_count(result), 1U, "SHOW TABLES row count");
    failures += expect_text(
        mylite_result_value_text(result, 0U, 0U),
        "simple_lifecycle",
        "SHOW TABLES table name"
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SHOW TABLES FROM app", &result);
    failures +=
        expect_size(mylite_result_column_count(result), 1U, "SHOW TABLES FROM column count");
    failures += expect_text(
        mylite_result_column_name(result, 0U),
        "Tables_in_app",
        "SHOW TABLES FROM column name"
    );
    failures += expect_size(mylite_result_row_count(result), 1U, "SHOW TABLES FROM row count");
    failures += expect_text(
        mylite_result_value_text(result, 0U, 0U),
        "simple_lifecycle",
        "SHOW TABLES FROM table name"
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SHOW TABLES IN app", &result);
    failures += expect_size(mylite_result_column_count(result), 1U, "SHOW TABLES IN column count");
    failures += expect_text(
        mylite_result_column_name(result, 0U),
        "Tables_in_app",
        "SHOW TABLES IN column name"
    );
    failures += expect_size(mylite_result_row_count(result), 1U, "SHOW TABLES IN row count");
    failures += expect_text(
        mylite_result_value_text(result, 0U, 0U),
        "simple_lifecycle",
        "SHOW TABLES IN table name"
    );
    mylite_result_free(result);
    result = NULL;

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "lifecycle preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen lifecycle file");
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read reopened schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "simple_lifecycle", &table),
        MYLITE_OK,
        "read reopened table"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    has_physical_table = 0;
    if (sqlite != NULL) {
        failures += table_exists(sqlite, table.physical_name, &has_physical_table);
    }
    failures += expect_int(has_physical_table, 1, "reopened physical table exists");

    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "DROP TABLE simple_lifecycle", &result);
    failures += expect_empty_result(result, "DROP TABLE result");
    mylite_result_free(result);
    result = NULL;
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            expected_generation_after_drop,
            "generation after drop"
        );
    }
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "simple_lifecycle", &table),
        MYLITE_ERROR,
        "dropped table descriptor removed"
    );
    has_physical_table = 1;
    if (sqlite != NULL) {
        failures += table_exists(sqlite, "_mylite_user_table_1", &has_physical_table);
    }
    failures += expect_int(has_physical_table, 0, "dropped physical table removed");

    failures += execute_ok(database, "SHOW TABLES", &result);
    failures += expect_size(mylite_result_row_count(result), 0U, "SHOW TABLES after drop");
    mylite_result_free(result);

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
    int failures = 0;

    if (make_test_path(path, sizeof(path), "failures") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open failure test file");
    failures += seed_schema(database, "app");

    failures += execute_error(
        database,
        "CREATE TABLE no_schema (id INT)",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE missing_schema.t (id INT)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE _mylite_schema.t (id INT)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name",
        }
    );

    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE duplicate_target (id INT)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "CREATE TABLE duplicate_target (id INT)",
        (struct expected_sql_error){
            .code = mysql_error_table_exists,
            .sqlstate = "42S01",
            .message_part = "already exists",
        }
    );
    failures += execute_error(
        database,
        "DROP TABLE missing_table",
        (struct expected_sql_error){
            .code = mysql_error_unknown_table,
            .sqlstate = "42S02",
            .message_part = "Unknown table 'app.missing_table'",
        }
    );
    failures += execute_error(
        database,
        "DROP TABLE _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "USE _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLES FROM _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE reserved_target (_mylite_column INT)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_column_name,
            .sqlstate = "42000",
            .message_part = "Incorrect column name",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE _mylite_reserved (id INT)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE duplicate_columns (id INT, id BIGINT)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_column,
            .sqlstate = "42S21",
            .message_part = "Duplicate column name",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE duplicate_case_columns (id INT, ID BIGINT)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_column,
            .sqlstate = "42S21",
            .message_part = "Duplicate column name 'ID'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE unsupported_type (name VARCHAR)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_ok(database, "DROP TABLE IF EXISTS missing_table", &result);
    failures += expect_size(
        mylite_result_column_count(result),
        0U,
        "DROP TABLE IF EXISTS missing column count"
    );
    failures +=
        expect_size(mylite_result_row_count(result), 0U, "DROP TABLE IF EXISTS missing row count");
    failures += expect_int64(
        mylite_result_affected_rows(result),
        0,
        "DROP TABLE IF EXISTS missing affected rows"
    );
    failures += expect_size(
        mylite_result_warning_count(result),
        1U,
        "DROP TABLE IF EXISTS missing warning count"
    );
    mylite_result_free(result);
    result = NULL;

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            expected_generation_after_create,
            "failed statements do not advance generation"
        );
    }
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read schema after failures"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "reserved_target", &table),
        MYLITE_ERROR,
        "reserved-column failure has no table descriptor"
    );
    failures += expect_bool(
        mylite_catalog_name_is_reserved("_MyLite_reserved"),
        true,
        "reserved prefix is case-insensitive"
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_physical_create_failure_rolls_back_catalog(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    const struct mylite_catalog *catalog = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "create_failure") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open create-failure file");
    failures += seed_schema(database, "app");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += execute_sql(sqlite, "CREATE TABLE _mylite_user_table_1(conflict INTEGER)");
    }
    failures += execute_error(
        database,
        "CREATE TABLE app.conflicted (id INT)",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite schema operation failed",
        }
    );
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read schema after create physical failure"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "conflicted", &table),
        MYLITE_ERROR,
        "failed physical create rolls back catalog table"
    );
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            expected_generation_after_schema,
            "physical create failure leaves generation"
        );
    }

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_physical_drop_failure_rolls_back_catalog(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    const struct mylite_catalog *catalog = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "drop_failure") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open drop-failure file");
    failures += seed_schema(database, "app");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE survives_failed_drop (id INT)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read drop-failure schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(
            database,
            schema.schema_id,
            "survives_failed_drop",
            &table
        ),
        MYLITE_OK,
        "read drop-failure table"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        char sql[physical_drop_sql_capacity];
        int written = snprintf(sql, sizeof(sql), "DROP TABLE \"%s\"", table.physical_name);

        if (written < 0 || (size_t)written >= sizeof(sql)) {
            failures += 1;
        } else {
            failures += execute_sql(sqlite, sql);
        }
    }
    failures += execute_error(
        database,
        "DROP TABLE survives_failed_drop",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite schema operation failed",
        }
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(
            database,
            schema.schema_id,
            "survives_failed_drop",
            &table
        ),
        MYLITE_OK,
        "failed physical drop keeps catalog table"
    );
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            expected_generation_after_create,
            "physical drop failure leaves generation"
        );
    }

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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first lifecycle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second lifecycle");
    failures += seed_schema(first, "app");
    failures += seed_schema(second, "app");

    failures += execute_ok(first, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "CREATE TABLE only_first (id INT)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(second, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "SHOW TABLES", &result);
    failures += expect_size(mylite_result_row_count(result), 0U, "second handle has no tables");
    mylite_result_free(result);
    result = NULL;

    failures += expect_int(
        mylite_catalog_read_schema_by_name(first, "app", &schema),
        MYLITE_OK,
        "read first schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(first, schema.schema_id, "only_first", &table),
        MYLITE_OK,
        "first has table"
    );
    failures += expect_int(
        mylite_catalog_read_schema_by_name(second, "app", &schema),
        MYLITE_OK,
        "read second schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(second, schema.schema_id, "only_first", &table),
        MYLITE_ERROR,
        "second lacks first table"
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
        "%s/mylite_basic_table_lifecycle_%d_%s.mylite",
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
        int expected_value = 0;
        int actual_value = 0;

        if (expected) {
            expected_value = 1;
        }
        if (actual) {
            actual_value = 1;
        }
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected_value, actual_value);
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
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }

    return 0;
}
