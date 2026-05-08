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
    sql_capacity = 512,
    show_columns_column_count = 6,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_unknown = 1105,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_incorrect_column_name = 1166,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_cant_remove_all_fields = 1090,
    mysql_error_cant_drop_field_or_key = 1091,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t row_count;
    size_t column_count;
    const char *context;
};

static int test_drop_column_success_descriptor_persistence_and_dml(void);
static int test_drop_column_diagnostics(void);
static int test_drop_column_physical_failure_preserves_catalog(void);
static int test_drop_column_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_ddl_result(const mylite_result *result, const char *context);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_column_descriptor(
    mylite_db *database,
    int64_t table_id,
    const char *name,
    int64_t ordinal_position,
    const char *logical_type,
    bool is_nullable,
    const char *context
);
static int expect_missing_column_descriptor(
    mylite_db *database,
    int64_t table_id,
    const char *name,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int execute_sql(sqlite3 *connection, const char *sql);
static int drop_physical_column(sqlite3 *connection, const char *physical_name, const char *column);
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

    failures += test_drop_column_success_descriptor_persistence_and_dml();
    failures += test_drop_column_diagnostics();
    failures += test_drop_column_physical_failure_preserves_catalog();
    failures += test_drop_column_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_drop_column_success_descriptor_persistence_and_dml(void) {
    static const char *const row_count_zero[] = {"0"};
    static const char *const warning_count_zero[] = {"0"};
    static const char *const rows_after_drop[] = {
        "1",
        "11",
        "12",
        "13",
        "2",
        NULL,
        NULL,
        "20",
        "3",
        "31",
        "-5",
        "33",
    };
    static const char *const rows_after_first_drop[] = {
        "1",
        "11",
        "12",
        "13",
        "2",
        NULL,
        NULL,
        "20",
    };
    static const char *const rows_after_rename_drop[] = {
        "1",
        "11",
        "13",
        "2",
        NULL,
        "20",
        "3",
        "31",
        "33",
    };
    static const char *const edge_rows_after_first_drop[] = {
        "2",
        "3",
        "5",
        "6",
    };
    static const char *const edge_rows_after_last_drop[] = {
        "2",
        "5",
    };
    static const char *const cleanup_after_delete[] = {"2", "20"};
    static const char *const show_columns_rows[] = {
        "id", "int",    "NO",  "", NULL, "", "iu", "int unsigned",    "YES", "", NULL, "",
        "b",  "bigint", "YES", "", NULL, "", "bu", "bigint unsigned", "NO",  "", NULL, "",
    };
    static const char *const show_create_rows[] = {
        "numbers",
        "CREATE TABLE `numbers` (\n"
        "  `id` int NOT NULL,\n"
        "  `iu` int unsigned DEFAULT NULL,\n"
        "  `b` bigint DEFAULT NULL,\n"
        "  `bu` bigint unsigned NOT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor before_table = {0};
    struct mylite_catalog_table_descriptor after_table = {0};
    struct mylite_catalog_table_descriptor edge_table = {0};
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation_before = 0U;
    uint64_t sqlite_generation_before = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open drop-column file");
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    failures += expect_ddl_result(result, "use schema result");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE numbers (id INT NOT NULL, i INT, iu INTEGER UNSIGNED, b BIGINT, "
        "bu BIGINT UNSIGNED NOT NULL)",
        &result
    );
    failures += expect_ddl_result(result, "create numbers result");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO numbers VALUES (1, 10, 11, 12, 13)", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "INSERT INTO numbers VALUES (2, NULL, NULL, NULL, 20)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read app schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "numbers", &before_table),
        MYLITE_OK,
        "read numbers table before drop"
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        catalog_generation_before = catalog->generation;
    }
    if (session != NULL) {
        sqlite_generation_before = session->sqlite_schema_generation;
    }

    failures += execute_ok(database, "ALTER TABLE numbers DROP COLUMN i", &result);
    failures += expect_ddl_result(result, "drop column result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "numbers", &after_table),
        MYLITE_OK,
        "read numbers table after drop"
    );
    failures += expect_int64(after_table.table_id, before_table.table_id, "table id unchanged");
    failures += expect_text(
        after_table.physical_name,
        before_table.physical_name,
        "physical name unchanged"
    );
    failures += expect_uint64(
        after_table.descriptor_version,
        before_table.descriptor_version + 1U,
        "drop bumps table descriptor version"
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            catalog_generation_before + 1U,
            "drop bumps catalog generation"
        );
    }
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before + 1U,
            "drop bumps SQLite schema generation"
        );
    }
    failures += expect_column_descriptor(
        database,
        before_table.table_id,
        "id",
        1,
        "INT",
        false,
        "id descriptor after drop"
    );
    failures += expect_missing_column_descriptor(
        database,
        before_table.table_id,
        "i",
        "dropped column descriptor removed"
    );
    failures += expect_column_descriptor(
        database,
        before_table.table_id,
        "iu",
        2,
        "INT UNSIGNED",
        true,
        "unsigned integer descriptor compacted"
    );
    failures += expect_column_descriptor(
        database,
        before_table.table_id,
        "b",
        3,
        "BIGINT",
        true,
        "bigint descriptor compacted"
    );
    failures += expect_column_descriptor(
        database,
        before_table.table_id,
        "bu",
        4,
        "BIGINT UNSIGNED",
        false,
        "unsigned bigint descriptor compacted"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .values = row_count_zero,
            .row_count = 1U,
            .column_count = 1U,
            .context = "row count after drop column",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count",
            .values = warning_count_zero,
            .row_count = 1U,
            .column_count = 1U,
            .context = "warning count after drop column",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM numbers ORDER BY id",
            .values = rows_after_first_drop,
            .row_count = 2U,
            .column_count = 4U,
            .context = "select star after first drop",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM numbers",
            .values = show_columns_rows,
            .row_count = 4U,
            .column_count = show_columns_column_count,
            .context = "show columns after drop",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE numbers",
            .values = show_create_rows,
            .row_count = 1U,
            .column_count = 2U,
            .context = "show create table after drop",
        }
    );

    failures += execute_ok(database, "INSERT INTO numbers VALUES (3, 31, 32, 33)", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 1, "insert after drop rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "insert after drop warnings");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "UPDATE numbers SET b = -5 WHERE id = 3", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 1, "update after drop rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "update after drop warnings");
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, iu, b, bu FROM numbers ORDER BY id",
            .values = rows_after_drop,
            .row_count = 3U,
            .column_count = 4U,
            .context = "rows after insert and update",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE edge_drop (first_col INT, middle_col BIGINT, last_col INT UNSIGNED)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO edge_drop VALUES (1, 2, 3), (4, 5, 6)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "edge_drop", &edge_table),
        MYLITE_OK,
        "read edge_drop table"
    );
    failures += execute_ok(database, "ALTER TABLE edge_drop DROP COLUMN first_col", &result);
    failures += expect_ddl_result(result, "drop first column result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_missing_column_descriptor(
        database,
        edge_table.table_id,
        "first_col",
        "first column descriptor removed"
    );
    failures += expect_column_descriptor(
        database,
        edge_table.table_id,
        "middle_col",
        1,
        "BIGINT",
        true,
        "middle column shifted to first ordinal"
    );
    failures += expect_column_descriptor(
        database,
        edge_table.table_id,
        "last_col",
        2,
        "INT UNSIGNED",
        true,
        "last column shifted after first drop"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM edge_drop ORDER BY middle_col",
            .values = edge_rows_after_first_drop,
            .row_count = 2U,
            .column_count = 2U,
            .context = "rows after first column drop",
        }
    );
    failures += execute_ok(database, "ALTER TABLE edge_drop DROP COLUMN last_col", &result);
    failures += expect_ddl_result(result, "drop last column result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_missing_column_descriptor(
        database,
        edge_table.table_id,
        "last_col",
        "last column descriptor removed"
    );
    failures += expect_column_descriptor(
        database,
        edge_table.table_id,
        "middle_col",
        1,
        "BIGINT",
        true,
        "middle column remains first after last drop"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM edge_drop ORDER BY middle_col",
            .values = edge_rows_after_last_drop,
            .row_count = 2U,
            .column_count = 1U,
            .context = "rows after last column drop",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE cleanup (id INT NOT NULL, dropped INT, kept INT)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "INSERT INTO cleanup VALUES (1, 100, 10), (2, NULL, 20)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "ALTER TABLE cleanup DROP dropped", &result);
    failures += expect_ddl_result(result, "drop cleanup column result");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "DELETE FROM cleanup WHERE kept = 10", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 1, "delete after drop rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "delete after drop warnings");
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, kept FROM cleanup ORDER BY id",
            .values = cleanup_after_delete,
            .row_count = 1U,
            .column_count = 2U,
            .context = "delete observes drop",
        }
    );
    failures += execute_ok(database, "TRUNCATE TABLE cleanup", &result);
    failures += expect_ddl_result(result, "truncate after drop");
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, kept FROM cleanup ORDER BY id",
            .values = cleanup_after_delete,
            .row_count = 0U,
            .column_count = 2U,
            .context = "truncate observes drop",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures +=
        expect_bytes(actual_preamble, expected_preamble, sizeof(expected_preamble), "preamble");

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen drop-column file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, iu, b, bu FROM numbers ORDER BY id",
            .values = rows_after_drop,
            .row_count = 3U,
            .column_count = 4U,
            .context = "reopen sees dropped column",
        }
    );
    failures += execute_ok(database, "RENAME TABLE numbers TO renamed_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "ALTER TABLE renamed_numbers DROP COLUMN b", &result);
    failures += expect_ddl_result(result, "drop after rename result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, iu, bu FROM renamed_numbers ORDER BY id",
            .values = rows_after_rename_drop,
            .row_count = 3U,
            .column_count = 3U,
            .context = "rename preserves remaining columns",
        }
    );
    failures += execute_ok(database, "DROP TABLE renamed_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "ALTER TABLE renamed_numbers DROP COLUMN iu",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_numbers' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_drop_column_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics database");
    failures += execute_error(
        database,
        "ALTER TABLE missing DROP COLUMN n",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE numbers (id INT NOT NULL, n INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE one_col (id INT NOT NULL)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "ALTER TABLE unknown DROP COLUMN n",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.unknown' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.numbers DROP COLUMN n",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers DROP COLUMN missing",
        (struct expected_sql_error){
            .code = mysql_error_cant_drop_field_or_key,
            .sqlstate = "42000",
            .message_part = "Can't DROP 'missing'; check that column/key exists",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE one_col DROP COLUMN id",
        (struct expected_sql_error){
            .code = mysql_error_cant_remove_all_fields,
            .sqlstate = "42000",
            .message_part = "You can't delete all columns with ALTER TABLE; use DROP TABLE instead",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private DROP COLUMN n",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers DROP COLUMN _mylite_private",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_column_name,
            .sqlstate = "42000",
            .message_part = "Incorrect column name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers DROP COLUMN numbers.n",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers DROP COLUMN n, DROP COLUMN id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers DROP PRIMARY KEY",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers DROP INDEX idx",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers DROP FOREIGN KEY fk",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers DROP COLUMN IF EXISTS n",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers DROP (n)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers DROP COLUMN n FIRST",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers DROP COLUMN n, ALGORITHM=INSTANT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers DROP COLUMN n, LOCK=DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_drop_column_physical_failure_preserves_catalog(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor before_table = {0};
    struct mylite_catalog_table_descriptor after_table = {0};
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t generation_before_failure = 0U;
    uint64_t sqlite_generation_before_failure = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "physical_failure") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open physical failure file");
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE broken (id INT NOT NULL, doomed INT, kept INT)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO broken VALUES (1, 10, 20)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read failure schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "broken", &before_table),
        MYLITE_OK,
        "read failure table before"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += drop_physical_column(sqlite, before_table.physical_name, "doomed");
    }
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        generation_before_failure = catalog->generation;
    }
    if (session != NULL) {
        sqlite_generation_before_failure = session->sqlite_schema_generation;
    }

    failures += execute_error(
        database,
        "ALTER TABLE broken DROP COLUMN doomed",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite schema operation failed",
        }
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            generation_before_failure,
            "physical failure leaves catalog generation unchanged"
        );
    }
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before_failure,
            "physical failure leaves SQLite schema generation unchanged"
        );
    }
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "broken", &after_table),
        MYLITE_OK,
        "read failure table after"
    );
    failures += expect_uint64(
        after_table.descriptor_version,
        before_table.descriptor_version,
        "physical failure leaves table descriptor version unchanged"
    );
    failures += expect_column_descriptor(
        database,
        before_table.table_id,
        "doomed",
        2,
        "INT",
        true,
        "physical failure keeps dropped descriptor"
    );
    failures += expect_column_descriptor(
        database,
        before_table.table_id,
        "kept",
        3,
        "INT",
        true,
        "physical failure keeps later descriptor ordinal"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_drop_column_independent_handles(void) {
    static const char *const first_rows[] = {"1", "10"};
    static const char *const second_rows[] = {"2", "200", "20"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += execute_ok(first, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(first, "CREATE TABLE app.t (id INT NOT NULL, dropped INT, kept INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(second, "CREATE TABLE app.t (id INT NOT NULL, dropped INT, kept INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "INSERT INTO app.t VALUES (1, 100, 10)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "INSERT INTO app.t VALUES (2, 200, 20)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "ALTER TABLE app.t DROP COLUMN dropped", &result);
    failures += expect_ddl_result(result, "first independent drop");
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, kept FROM app.t ORDER BY id",
            .values = first_rows,
            .row_count = 1U,
            .column_count = 2U,
            .context = "first handle dropped column",
        }
    );
    failures += execute_error(
        first,
        "SELECT id, dropped FROM app.t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'dropped'",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, dropped, kept FROM app.t ORDER BY id",
            .values = second_rows,
            .row_count = 1U,
            .column_count = 3U,
            .context = "second handle keeps column",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected success for %s: rc=%d err=%d state=%s msg=%s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (out_result == NULL) {
        mylite_result_free(result);
    } else {
        *out_result = result;
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    const char *message = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "expected error for %s, got rc=%d\n", sql, rc);
        failures += 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    message = mylite_errmsg(database);
    if (expected.message_part != NULL) {
        failures += expect_contains(message, expected.message_part, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_ddl_result(const mylite_result *result, const char *context) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), 0U, context);
    failures += expect_size(mylite_result_row_count(result), 0U, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    size_t value_count = query.row_count * query.column_count;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t index = 0U; index < value_count; ++index) {
        failures += expect_result_value(
            result,
            index / query.column_count,
            index % query.column_count,
            query.values[index],
            query.context
        );
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
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s: expected NULL at %zu,%zu, got '%s'\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }

    return expect_text(actual, expected, context);
}

static int expect_column_descriptor(
    mylite_db *database,
    int64_t table_id,
    const char *name,
    int64_t ordinal_position,
    const char *logical_type,
    bool is_nullable,
    const char *context
) {
    struct mylite_catalog_column_descriptor column = {0};
    int failures = expect_int(
        mylite_catalog_read_column_by_name(database, table_id, name, &column),
        MYLITE_OK,
        context
    );

    failures += expect_int64(column.ordinal_position, ordinal_position, context);
    failures += expect_text(column.name, name, context);
    failures += expect_text(column.logical_type, logical_type, context);
    failures += expect_text(column.physical_type, "INTEGER", context);
    failures += expect_true(column.is_nullable == is_nullable, context);
    return failures;
}

static int expect_missing_column_descriptor(
    mylite_db *database,
    int64_t table_id,
    const char *name,
    const char *context
) {
    struct mylite_catalog_column_descriptor column = {0};

    return expect_int(
        mylite_catalog_read_column_by_name(database, table_id, name, &column),
        MYLITE_ERROR,
        context
    );
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "test-runtime-alter-table-drop-column-%s-%d.mylite",
        name,
        current_process_id()
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
    return (int)getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(file);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    if (read_count != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        fclose(file);
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
        fprintf(stderr, "SQLite exec failed for %s: %d\n", sql, rc);
        return 1;
    }
    return 0;
}

static int drop_physical_column(
    sqlite3 *connection,
    const char *physical_name,
    const char *column
) {
    char sql[sql_capacity];
    int written =
        snprintf(sql, sizeof(sql), "ALTER TABLE \"%s\" DROP COLUMN \"%s\"", physical_name, column);

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "failed to build physical drop column SQL\n");
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
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
