#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"

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
    related_path_suffix_capacity = 16,
    sqlite_sql_capacity = 2048,
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

struct expected_sum_query {
    const char *sql;
    const char *column;
    const char *value;
    const char *context;
};

static int test_sum_values_persistence_rename_and_drop(void);
static int test_sum_diagnostics(void);
static int test_independent_sum_handles(void);
static int seed_schema(mylite_db *database, const char *name);
static int create_sum_table(mylite_db *database, const char *table_name);
static int create_empty_table(mylite_db *database, const char *table_name);
static int create_all_null_table(mylite_db *database, const char *table_name);
static int create_overflow_table(mylite_db *database, const char *table_name);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_sum_query(mylite_db *database, struct expected_sum_query query);
static int expect_row_count(mylite_db *database, const char *expected, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int drop_physical_table(sqlite3 *connection, const char *physical_name);
static int execute_sql(sqlite3 *connection, const char *sql);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_value(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_sum_values_persistence_rename_and_drop();
    failures += test_sum_diagnostics();
    failures += test_independent_sum_handles();

    return failures == 0 ? 0 : 1;
}

static int test_sum_values_persistence_rename_and_drop(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation_before_select = 0U;
    uint64_t sqlite_generation_before_select = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    failures += seed_schema(database, "app");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_empty_table(database, "empty_numbers");
    failures += create_all_null_table(database, "all_null_numbers");
    failures += create_sum_table(database, "numbers");
    failures += execute_ok(database, "ALTER TABLE numbers ALTER COLUMN tie SET INVISIBLE", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE options (option_value TEXT, autoload VARCHAR(20))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO options VALUES "
        "('abc','yes'),('de','on'),('ignored','no'),('', 'auto'),(NULL,'auto-on')",
        NULL
    );

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        catalog_generation_before_select = catalog->generation;
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        sqlite_generation_before_select = session->sqlite_schema_generation;
    }

    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(id) FROM numbers",
            .column = "SUM(id)",
            .value = "10",
            .context = "signed id sum",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT ALL SUM(i) FROM numbers",
            .column = "SUM(i)",
            .value = "2147483646",
            .context = "all signed int sum",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(ii) FROM numbers",
            .column = "SUM(ii)",
            .value = "15",
            .context = "integer alias sum",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(iu) FROM numbers",
            .column = "SUM(iu)",
            .value = "4294967305",
            .context = "unsigned int sum within signed range",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(integeru) FROM numbers",
            .column = "SUM(integeru)",
            .value = "34",
            .context = "integer unsigned alias sum",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(b) FROM numbers",
            .column = "SUM(b)",
            .value = "10",
            .context = "bigint sum within signed range",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(bu) FROM numbers",
            .column = "SUM(bu)",
            .value = "19",
            .context = "bigint unsigned physical sum within signed range",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(ti) FROM numbers",
            .column = "SUM(ti)",
            .value = "-2",
            .context = "tinyint sum",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(ti1) FROM numbers",
            .column = "SUM(ti1)",
            .value = "2",
            .context = "tinyint display-width sum",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(si) FROM numbers",
            .column = "SUM(si)",
            .value = "-1",
            .context = "smallint sum",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(mi) FROM numbers",
            .column = "SUM(mi)",
            .value = "-1",
            .context = "mediumint sum",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(bool_col) FROM numbers",
            .column = "SUM(bool_col)",
            .value = "1",
            .context = "bool alias sum",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(boolean_col) FROM numbers",
            .column = "SUM(boolean_col)",
            .value = "2",
            .context = "boolean alias sum",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(n) FROM numbers",
            .column = "SUM(n)",
            .value = "70",
            .context = "nullable integer sum ignores nulls",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(LENGTH(option_value)) FROM options "
                   "WHERE autoload IN ('yes','on','auto-on','auto')",
            .column = "SUM(LENGTH(option_value))",
            .value = "5",
            .context = "wordpress autoloaded option length sum",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(nn) FROM numbers",
            .column = "SUM(nn)",
            .value = "26",
            .context = "not-null integer sum",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(tie) FROM numbers",
            .column = "SUM(tie)",
            .value = "6",
            .context = "explicit invisible column sum",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(n) FROM empty_numbers",
            .column = "SUM(n)",
            .value = NULL,
            .context = "empty table sum",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(n) FROM all_null_numbers",
            .column = "SUM(n)",
            .value = NULL,
            .context = "all null sum",
        }
    );

    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT sum(n) FROM numbers",
            .column = "sum(n)",
            .value = "70",
            .context = "lowercase label",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT Sum( n ) FROM numbers",
            .column = "Sum( n )",
            .value = "70",
            .context = "spaced label",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(/* inside */n) FROM numbers",
            .column = "SUM(/* inside */ n)",
            .value = "70",
            .context = "block comment label",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT (SUM(n)) FROM numbers",
            .column = "(SUM(n))",
            .value = "70",
            .context = "parenthesized label",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(n) AS total FROM numbers",
            .column = "total",
            .value = "70",
            .context = "select item alias",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(nums.n) FROM numbers AS nums",
            .column = "SUM(nums.n)",
            .value = "70",
            .context = "source-qualified aggregate argument",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(`weird name`) FROM numbers",
            .column = "SUM(`weird name`)",
            .value = "11",
            .context = "quoted aggregate argument",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(`double\"quote`) FROM numbers",
            .column = "SUM(`double\"quote`)",
            .value = "15",
            .context = "quoted aggregate argument with quote",
        }
    );

    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(n) FROM numbers WHERE id > 99",
            .column = "SUM(n)",
            .value = NULL,
            .context = "no matched rows sum",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(id) FROM numbers WHERE n IS NULL",
            .column = "SUM(id)",
            .value = "1",
            .context = "is null predicate",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(n) FROM numbers WHERE n IS NOT NULL",
            .column = "SUM(n)",
            .value = "70",
            .context = "is not null predicate",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(n) FROM numbers WHERE id <> 1",
            .column = "SUM(n)",
            .value = "70",
            .context = "not equal predicate",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(n) FROM numbers WHERE id <=> 2",
            .column = "SUM(n)",
            .value = "20",
            .context = "null-safe equality predicate",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(id) FROM numbers WHERE i = 2147483647",
            .column = "SUM(id)",
            .value = "3",
            .context = "signed int boundary predicate",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(id) FROM numbers WHERE b = -9223372036854775808",
            .column = "SUM(id)",
            .value = "1",
            .context = "signed bigint boundary predicate",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(iu) FROM numbers WHERE iu = 4294967295",
            .column = "SUM(iu)",
            .value = "4294967295",
            .context = "unsigned int boundary predicate",
        }
    );

    failures += expect_row_count(database, "-1", "row count after aggregate result set");
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += expect_size(
            (size_t)catalog->generation,
            (size_t)catalog_generation_before_select,
            "sum leaves catalog generation"
        );
    }
    if (session != NULL) {
        failures += expect_size(
            (size_t)session->sqlite_schema_generation,
            (size_t)sqlite_generation_before_select,
            "sum leaves SQLite schema generation"
        );
    }
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after sum reads"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(n) FROM numbers",
            .column = "SUM(n)",
            .value = "70",
            .context = "reopened sum",
        }
    );
    failures += execute_ok(database, "RENAME TABLE numbers TO sum_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "SELECT SUM(n) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.numbers' doesn't exist",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(n) FROM sum_numbers",
            .column = "SUM(n)",
            .value = "70",
            .context = "renamed source sum",
        }
    );
    failures += execute_ok(database, "TRUNCATE TABLE sum_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(n) FROM sum_numbers",
            .column = "SUM(n)",
            .value = NULL,
            .context = "truncated source sum",
        }
    );
    failures += execute_ok(database, "DROP TABLE sum_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "SELECT SUM(n) FROM sum_numbers",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.sum_numbers' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_sum_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_error(
        database,
        "SELECT SUM(i) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );

    failures += seed_schema(database, "app");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_sum_table(database, "numbers");
    failures += create_overflow_table(database, "overflow_numbers");

    failures += execute_error(
        database,
        "SELECT SUM(i) FROM missing",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(i) FROM missing_schema.numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(i) FROM _mylite_reserved.numbers",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(i) FROM _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(missing) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(i) FROM numbers WHERE missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(i)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUM(column) supports only descriptor-backed table reads",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(i) FROM DUAL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUM(column) supports only descriptor-backed table reads",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(i), SUM(n) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUM(column) supports exactly one aggregate select item",
        }
    );
    failures += execute_error(
        database,
        "SELECT i, SUM(n) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUM(column) supports exactly one aggregate select item",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(i) FROM numbers ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUM(column) supports only WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(i) FROM numbers LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUM(column) supports only WHERE",
        }
    );

    failures += execute_error(
        database,
        "SELECT SUM (i) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM/**/(i) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(1) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUM(expression) supports only string length expressions",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(NULL) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUM(expression) supports only string length expressions",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(*) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(DISTINCT i) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(i + 1) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUM(expression) supports only string length expressions",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(b) FROM overflow_numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUM(column) result exceeds MyLite signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(bu) FROM overflow_numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUM(column) result exceeds MyLite signed 64-bit range",
        }
    );
    failures += expect_sum_query(
        database,
        (struct expected_sum_query){
            .sql = "SELECT SUM(i) AS s FROM numbers",
            .column = "s",
            .value = "2147483646",
            .context = "sum select item alias after diagnostics",
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
        "SELECT SUM(i) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite row operation failed",
        }
    );

    mylite_result_free(result);
    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_independent_sum_handles(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += seed_schema(first, "app");
    failures += seed_schema(second, "app");
    failures += execute_ok(first, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_sum_table(first, "numbers");
    failures += create_empty_table(second, "numbers");
    failures += execute_ok(second, "INSERT INTO numbers VALUES (1, 100), (2, 200)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += expect_sum_query(
        first,
        (struct expected_sum_query){
            .sql = "SELECT SUM(n) FROM numbers",
            .column = "SUM(n)",
            .value = "70",
            .context = "first handle sum",
        }
    );
    failures += expect_sum_query(
        second,
        (struct expected_sum_query){
            .sql = "SELECT SUM(n) FROM numbers",
            .column = "SUM(n)",
            .value = "300",
            .context = "second handle sum",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);

    return failures;
}

static int seed_schema(mylite_db *database, const char *name) {
    char sql[sqlite_sql_capacity];
    mylite_result *result = NULL;
    int written = snprintf(sql, sizeof(sql), "CREATE DATABASE %s", name);
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "create schema SQL is too long for %s\n", name);
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);

    return failures;
}

static int create_sum_table(mylite_db *database, const char *table_name) {
    char sql[sqlite_sql_capacity];
    mylite_result *result = NULL;
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s ("
        "id INT NOT NULL, "
        "i INT, "
        "ii INTEGER, "
        "iu INT UNSIGNED, "
        "integeru INTEGER UNSIGNED, "
        "b BIGINT, "
        "bu BIGINT UNSIGNED, "
        "ti TINYINT, "
        "ti1 TINYINT(1), "
        "si SMALLINT, "
        "mi MEDIUMINT, "
        "bool_col BOOL, "
        "boolean_col BOOLEAN, "
        "n INT NULL, "
        "nn INT NOT NULL, "
        "tie INT NULL, "
        "`weird name` INT, "
        "`double\"quote` INT)",
        table_name
    );
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "create table SQL is too long for %s\n", table_name);
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);
    result = NULL;

    written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO %s VALUES "
        "(1, -2, -3, 0, 7, -9223372036854775808, 0, "
        "-128, 0, -32768, -8388608, FALSE, TRUE, NULL, 5, 1, 2, 3), "
        "(2, 1, 5, 2, 8, 3, 4, "
        "-1, 1, 0, 0, TRUE, FALSE, 20, 6, 1, 3, 4), "
        "(3, 2147483647, 6, 4294967295, 9, "
        "9223372036854775807, 7, "
        "127, NULL, 32767, 8388607, NULL, NULL, 30, 7, 2, 4, 5), "
        "(4, 0, 7, 8, 10, 8, 8, "
        "NULL, 1, NULL, NULL, FALSE, TRUE, 20, 8, 2, 2, 3)",
        table_name
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "insert SQL is too long for %s\n", table_name);
        return failures + 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);

    return failures;
}

static int create_empty_table(mylite_db *database, const char *table_name) {
    char sql[sqlite_sql_capacity];
    mylite_result *result = NULL;
    int written =
        snprintf(sql, sizeof(sql), "CREATE TABLE %s (id INT NOT NULL, n INT NULL)", table_name);
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "create empty table SQL is too long for %s\n", table_name);
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);

    return failures;
}

static int create_all_null_table(mylite_db *database, const char *table_name) {
    char sql[sqlite_sql_capacity];
    mylite_result *result = NULL;
    int written =
        snprintf(sql, sizeof(sql), "CREATE TABLE %s (id INT NOT NULL, n INT NULL)", table_name);
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "create all-null table SQL is too long for %s\n", table_name);
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);
    result = NULL;

    written = snprintf(sql, sizeof(sql), "INSERT INTO %s VALUES (1, NULL), (2, NULL)", table_name);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "insert all-null SQL is too long for %s\n", table_name);
        return failures + 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);

    return failures;
}

static int create_overflow_table(mylite_db *database, const char *table_name) {
    char sql[sqlite_sql_capacity];
    mylite_result *result = NULL;
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s (b BIGINT NOT NULL, bu BIGINT UNSIGNED NOT NULL)",
        table_name
    );
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "create overflow table SQL is too long for %s\n", table_name);
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);
    result = NULL;

    written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO %s VALUES (9223372036854775807, 9223372036854775807), (1, 1)",
        table_name
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "insert overflow SQL is too long for %s\n", table_name);
        return failures + 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

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
        mylite_result_free(result);
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
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
    if (mylite_errcode(database) != expected.code) {
        fprintf(
            stderr,
            "execute '%s': expected error code %d, got %d\n",
            sql,
            expected.code,
            mylite_errcode(database)
        );
        failures += 1;
    }
    if (strcmp(mylite_sqlstate(database), expected.sqlstate) != 0) {
        fprintf(
            stderr,
            "execute '%s': expected SQLSTATE '%s', got '%s'\n",
            sql,
            expected.sqlstate,
            mylite_sqlstate(database)
        );
        failures += 1;
    }
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int expect_sum_query(mylite_db *database, struct expected_sum_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), 1U, query.context);
    failures += expect_text(mylite_result_column_name(result, 0U), query.column, query.context);
    failures += expect_size(mylite_result_row_count(result), 1U, query.context);
    failures += expect_value(mylite_result_value_text(result, 0U, 0U), query.value, query.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    mylite_result_free(result);

    return failures;
}

static int expect_row_count(mylite_db *database, const char *expected, const char *context) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SELECT ROW_COUNT()", &result);

    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures += expect_text(mylite_result_column_name(result, 0U), "ROW_COUNT()", context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_text(mylite_result_value_text(result, 0U, 0U), expected, context);
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
        "%s/mylite_sum_aggregate_%d_%s.mylite",
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
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity + related_path_suffix_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }

    remove(related);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        fclose(file);
        return 1;
    }
    fclose(file);

    return 0;
}

static int drop_physical_table(sqlite3 *connection, const char *physical_name) {
    char sql[sqlite_sql_capacity];
    int written = snprintf(sql, sizeof(sql), "DROP TABLE \"%s\"", physical_name);

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "drop physical table SQL is too long for %s\n", physical_name);
        return 1;
    }

    return execute_sql(connection, sql);
}

static int execute_sql(sqlite3 *connection, const char *sql) {
    char *error_message = NULL;
    int rc = sqlite3_exec(connection, sql, NULL, NULL, &error_message);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQLite execute '%s' failed: %s\n", sql, error_message);
        sqlite3_free(error_message);
        return 1;
    }

    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_true(int condition, const char *context) {
    if (condition) {
        return 0;
    }

    fprintf(stderr, "%s: expected true\n", context);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected '%s', got '%s'\n",
        context,
        expected != NULL ? expected : "(null)",
        actual != NULL ? actual : "(null)"
    );
    return 1;
}

static int expect_value(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected value '%s', got '%s'\n",
        context,
        expected != NULL ? expected : "(null)",
        actual != NULL ? actual : "(null)"
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected '%s' to contain '%s'\n",
        context,
        actual != NULL ? actual : "(null)",
        needle != NULL ? needle : "(null)"
    );
    return 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) == 0) {
        return 0;
    }

    fprintf(stderr, "%s: byte comparison failed\n", context);
    return 1;
}
