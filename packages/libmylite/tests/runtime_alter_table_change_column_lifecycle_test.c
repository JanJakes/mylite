#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"

#include <stdbool.h>
#include <stdint.h>
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
    show_columns_field_count = 6,
    numbers_column_count = 5,
    numbers_after_added_column_count = 6,
    row_count_text_capacity = 32,
    physical_drop_sql_capacity = 256,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_duplicate_column = 1060,
    mysql_error_unknown = 1105,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_incorrect_column_name = 1166,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_data_out_of_range = 1264,
    mysql_error_data_truncated = 1265,
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

static int test_change_column_success_persistence_and_dml(void);
static int test_change_column_diagnostics_and_rollback(void);
static int test_change_column_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_change_ok(mylite_db *database, const char *sql, int64_t affected_rows);
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
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int execute_sql(sqlite3 *connection, const char *sql);
static int drop_physical_table(sqlite3 *connection, const char *physical_name);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
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

    failures += test_change_column_success_persistence_and_dml();
    failures += test_change_column_diagnostics_and_rollback();
    failures += test_change_column_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_change_column_success_persistence_and_dml(void) {
    static const char *const qualified_rows[] = {"1", "2", "2", "3"};
    static const char *const rows_after_type_change[] = {"1", "2", "2", "6"};
    static const char *const changed_insert_row[] = {"3", "4", "11", "12"};
    static const char *const show_columns_after_change[] = {
        "id",    "int",
        "NO",    "",
        NULL,    "",
        "final", "bigint",
        "YES",   "",
        NULL,    "",
        "u",     "int unsigned",
        "YES",   "",
        NULL,    "",
        "b",     "bigint",
        "YES",   "",
        NULL,    "",
        "bu",    "bigint unsigned",
        "YES",   "",
        NULL,    "",
    };
    static const char *const show_create_after_change[] = {
        "numbers",
        "CREATE TABLE `numbers` (\n"
        "  `id` int NOT NULL,\n"
        "  `final` bigint DEFAULT NULL,\n"
        "  `u` int unsigned DEFAULT NULL,\n"
        "  `b` bigint DEFAULT NULL,\n"
        "  `bu` bigint unsigned DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const nullability_only_columns[] = {"c", "int", "YES", "", NULL, ""};
    static const char *const case_column_rows[] = {"mixed", "int", "YES", "", NULL, ""};
    static const char *const integer_family_columns[] = {
        "plain_integer",
        "int",
        "YES",
        "",
        NULL,
        "",
        "plain_int_unsigned",
        "int unsigned",
        "YES",
        "",
        NULL,
        "",
        "plain_integer_unsigned",
        "int unsigned",
        "YES",
        "",
        NULL,
        "",
        "plain_bigint_unsigned",
        "bigint unsigned",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const integer_family_rows[] = {
        "2",
        "3",
        "4",
        "5",
        "6",
        "7",
        "8",
        "9",
    };
    static const char *const change_after_add_rename_columns[] = {
        "id",      "int",
        "NO",      "",
        NULL,      "",
        "final",   "bigint",
        "YES",     "",
        NULL,      "",
        "u",       "int unsigned",
        "YES",     "",
        NULL,      "",
        "b",       "bigint",
        "YES",     "",
        NULL,      "",
        "bu",      "bigint unsigned",
        "YES",     "",
        NULL,      "",
        "renamed", "bigint",
        "YES",     "",
        NULL,      "",
    };
    static const char *const rows_after_delete[] = {"1", "2", "2", "6"};
    static const char *const persisted_rows[] = {"1", "2", "2", "6"};
    static const char *const renamed_rows[] = {"9"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t noop_catalog_generation = 0U;
    uint64_t noop_sqlite_generation = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "CREATE TABLE app.qualified (id INT NOT NULL, n INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO app.qualified VALUES (1, 2), (2, 3)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_change_ok(
        database,
        "ALTER TABLE app.qualified CHANGE COLUMN n renamed BIGINT NOT NULL",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, renamed FROM app.qualified ORDER BY id",
            .values = qualified_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "schema-qualified change without selected schema",
        }
    );

    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE numbers ("
        "id INT NOT NULL, n INT NOT NULL, u INT UNSIGNED, b BIGINT, bu BIGINT UNSIGNED)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO numbers VALUES (1, 2, 3, 4, 5), (2, 6, 7, 8, 9)",
        &result
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_change_ok(database, "ALTER TABLE numbers CHANGE n renamed INT NOT NULL", 0);
    failures += expect_change_ok(database, "ALTER TABLE numbers CHANGE renamed renamed BIGINT", 2);
    failures += expect_change_ok(
        database,
        "ALTER TABLE numbers CHANGE COLUMN renamed final BIGINT NULL",
        0
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, final FROM numbers ORDER BY id",
            .values = rows_after_type_change,
            .column_count = 2U,
            .row_count = 2U,
            .context = "rows preserved after change",
        }
    );

    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    noop_catalog_generation = catalog == NULL ? 0U : catalog->generation;
    noop_sqlite_generation = session == NULL ? 0U : session->sqlite_schema_generation;
    failures += expect_change_ok(database, "ALTER TABLE numbers CHANGE final final BIGINT NULL", 0);
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    failures += expect_size(
        catalog == NULL ? 0U : (size_t)catalog->generation,
        (size_t)noop_catalog_generation,
        "same definition leaves catalog generation"
    );
    failures += expect_size(
        session == NULL ? 0U : (size_t)session->sqlite_schema_generation,
        (size_t)noop_sqlite_generation,
        "same definition leaves SQLite schema generation"
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM numbers",
            .values = show_columns_after_change,
            .column_count = show_columns_field_count,
            .row_count = numbers_column_count,
            .context = "show columns after change",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE numbers",
            .values = show_create_after_change,
            .column_count = 2U,
            .row_count = 1U,
            .context = "show create after change",
        }
    );
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "numbers", &table),
        MYLITE_OK,
        "read numbers descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        "final",
        2,
        "BIGINT",
        true,
        "final descriptor"
    );

    failures += execute_ok(database, "INSERT INTO numbers VALUES (3, NULL, 11, 12, 13)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "UPDATE numbers SET final = 4 WHERE id = 3", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 1, "update after change");
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, final, u, b FROM numbers WHERE id = 3",
            .values = changed_insert_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "insert and update after change",
        }
    );

    failures += execute_ok(database, "CREATE TABLE nullability_only (c INT NOT NULL)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO nullability_only VALUES (1), (2)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_change_ok(database, "ALTER TABLE nullability_only CHANGE c c INT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM nullability_only",
            .values = nullability_only_columns,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "omitted nullability becomes nullable",
        }
    );

    failures += execute_ok(database, "CREATE TABLE case_columns (MiXeD INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO case_columns VALUES (1)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_change_ok(database, "ALTER TABLE case_columns CHANGE MiXeD mixed INT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM case_columns",
            .values = case_column_rows,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "case-only spelling update",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE integer_family (i INTEGER, iu INT UNSIGNED, "
        "integer_unsigned INTEGER UNSIGNED, bu BIGINT UNSIGNED)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO integer_family VALUES (2, 3, 4, 5), (6, 7, 8, 9)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures +=
        expect_change_ok(database, "ALTER TABLE integer_family CHANGE i plain_integer INTEGER", 0);
    failures += expect_change_ok(
        database,
        "ALTER TABLE integer_family CHANGE iu plain_int_unsigned INT UNSIGNED",
        0
    );
    failures += expect_change_ok(
        database,
        "ALTER TABLE integer_family CHANGE integer_unsigned plain_integer_unsigned "
        "INTEGER UNSIGNED",
        0
    );
    failures += expect_change_ok(
        database,
        "ALTER TABLE integer_family CHANGE bu plain_bigint_unsigned BIGINT UNSIGNED",
        0
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM integer_family",
            .values = integer_family_columns,
            .column_count = show_columns_field_count,
            .row_count = 4U,
            .context = "integer family descriptors after change",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT plain_integer, plain_int_unsigned, plain_integer_unsigned, "
                   "plain_bigint_unsigned FROM integer_family ORDER BY plain_integer",
            .values = integer_family_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "integer family rows after change",
        }
    );

    failures += execute_ok(database, "ALTER TABLE numbers ADD added INT", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "ALTER TABLE numbers RENAME COLUMN added TO renamed", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_change_ok(database, "ALTER TABLE numbers CHANGE renamed renamed BIGINT", 3);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM numbers",
            .values = change_after_add_rename_columns,
            .column_count = show_columns_field_count,
            .row_count = numbers_after_added_column_count,
            .context = "change after add and rename column",
        }
    );
    failures += execute_ok(database, "ALTER TABLE numbers DROP COLUMN renamed", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "ALTER TABLE numbers CHANGE renamed again BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'renamed' in 'numbers'",
        }
    );
    failures += execute_ok(database, "DELETE FROM numbers WHERE final = 4", &result);
    failures += expect_int64(mylite_result_affected_rows(result), 1, "delete after change");
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, final FROM numbers ORDER BY id",
            .values = rows_after_delete,
            .column_count = 2U,
            .row_count = 2U,
            .context = "delete observes changed column",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE rename_source (id INT NOT NULL, n INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO rename_source VALUES (1, 9)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "RENAME TABLE rename_source TO renamed_target", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_change_ok(database, "ALTER TABLE renamed_target CHANGE n changed BIGINT", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT changed FROM renamed_target",
            .values = renamed_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "change after table rename",
        }
    );
    failures += execute_ok(database, "DROP TABLE renamed_target", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "ALTER TABLE renamed_target CHANGE changed again INT",
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
        "change preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, final FROM numbers ORDER BY id",
            .values = persisted_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "changed table persists after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_change_column_diagnostics_and_rollback(void) {
    static const char *const unchanged_nullable[] = {NULL, "1"};
    static const char *const unchanged_range[] = {"2147483648"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "ALTER TABLE numbers CHANGE n renamed BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.numbers CHANGE n renamed BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_reserved.numbers CHANGE n renamed BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name",
        }
    );

    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE numbers (id INT NOT NULL, n INT, other INT NOT NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO numbers VALUES (1, NULL, 5), (2, 1, 6)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "ALTER TABLE missing_table CHANGE n renamed BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_reserved CHANGE n renamed BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers CHANGE _mylite_hidden changed BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_column_name,
            .sqlstate = "42000",
            .message_part = "Incorrect column name",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers CHANGE n _mylite_hidden BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_column_name,
            .sqlstate = "42000",
            .message_part = "Incorrect column name",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers CHANGE missing changed BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'numbers'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers CHANGE n other BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_column,
            .sqlstate = "42S21",
            .message_part = "Duplicate column name 'other'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers CHANGE n changed BIGINT NOT NULL",
        (struct expected_sql_error){
            .code = mysql_error_data_truncated,
            .sqlstate = "01000",
            .message_part = "Data truncated for column 'changed' at row 1",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT n FROM numbers ORDER BY id",
            .values = unchanged_nullable,
            .column_count = 1U,
            .row_count = 2U,
            .context = "NULL validation failure preserves rows",
        }
    );

    failures += execute_ok(database, "CREATE TABLE range_bad (c BIGINT NOT NULL)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO range_bad VALUES (2147483648)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "ALTER TABLE range_bad CHANGE c changed INT NOT NULL",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'changed' at row 1",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT c FROM range_bad",
            .values = unchanged_range,
            .column_count = 1U,
            .row_count = 1U,
            .context = "range validation failure preserves rows",
        }
    );

    failures += execute_ok(database, "CREATE TABLE unsigned_bad (c INT NOT NULL)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO unsigned_bad VALUES (-1), (0)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "ALTER TABLE unsigned_bad CHANGE c changed INT UNSIGNED NOT NULL",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'changed' at row 1",
        }
    );

    failures += execute_error(
        database,
        "ALTER TABLE numbers CHANGE numbers.n changed BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers CHANGE n numbers.changed BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers CHANGE n changed",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers CHANGE n changed BIGINT DEFAULT '5'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers CHANGE n changed BIGINT FIRST",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers CHANGE n changed BIGINT, CHANGE other other2 BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers CHANGE n changed VARCHAR(10)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ALTER TABLE CHANGE COLUMN supports only baseline integer columns",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE numbers CHANGE n changed BIGINT, ALGORITHM=INPLACE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE rowid_shadow (rowid INT, _rowid_ INT, oid INT)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO rowid_shadow VALUES (1, 2, 3)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "ALTER TABLE rowid_shadow CHANGE rowid changed BIGINT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ALTER TABLE CHANGE COLUMN requires an unshadowed SQLite rowid alias",
        }
    );

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read physical failure schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "numbers", &table),
        MYLITE_OK,
        "read physical failure table"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += drop_physical_table(sqlite, table.physical_name);
    }
    failures += execute_error(
        database,
        "ALTER TABLE numbers CHANGE other changed BIGINT NOT NULL",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite row operation failed",
        }
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        "other",
        3,
        "INT",
        false,
        "physical failure preserves descriptor"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_change_column_independent_handles(void) {
    static const char *const first_columns[] = {"changed", "bigint", "YES", "", NULL, ""};
    static const char *const second_columns[] = {"n", "int", "YES", "", NULL, ""};
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += execute_ok(first, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "CREATE TABLE app.t (n INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "CREATE TABLE app.t (n INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "INSERT INTO app.t VALUES (1)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "INSERT INTO app.t VALUES (2)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += expect_change_ok(first, "ALTER TABLE app.t CHANGE n changed BIGINT", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM app.t",
            .values = first_columns,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "first independent descriptor",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM app.t",
            .values = second_columns,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "second independent descriptor",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
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
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    message = mylite_errmsg(database);
    if (expected.message_part != NULL) {
        failures += expect_contains(message, expected.message_part, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_change_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    char row_count_text[row_count_text_capacity];
    const char *row_count_values[] = {row_count_text};
    mylite_result *result = NULL;
    int written =
        snprintf(row_count_text, sizeof(row_count_text), "%lld", (long long)affected_rows);
    int failures = execute_ok(database, sql, &result);

    if (written < 0 || (size_t)written >= sizeof(row_count_text)) {
        fprintf(stderr, "failed to format expected affected rows\n");
        failures += 1;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, "change column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "change row count");
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, "change affected");
    failures += expect_size(mylite_result_warning_count(result), 0U, "change warning count");
    mylite_result_free(result);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .values = row_count_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "ROW_COUNT after change",
        }
    );

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
        "%s/mylite_alter_table_change_column_%d_%s.mylite",
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
    char *message = NULL;
    int rc = sqlite3_exec(connection, sql, NULL, NULL, &message);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "sqlite exec failed for %s: %s\n", sql, message == NULL ? "" : message);
        sqlite3_free(message);
        return 1;
    }

    return 0;
}

static int drop_physical_table(sqlite3 *connection, const char *physical_name) {
    char sql[physical_drop_sql_capacity];
    int written = snprintf(sql, sizeof(sql), "DROP TABLE \"%s\"", physical_name);

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "physical drop SQL is too long\n");
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
