#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"

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

struct expected_aggregate_query {
    const char *sql;
    const char *column;
    const char *value;
    const char *context;
};

static int test_min_max_values_persistence_rename_and_drop(void);
static int test_min_max_diagnostics(void);
static int test_independent_min_max_handles(void);
static int seed_schema(mylite_db *database, const char *name);
static int create_aggregate_table(mylite_db *database, const char *table_name);
static int create_empty_table(mylite_db *database, const char *table_name);
static int create_all_null_table(mylite_db *database, const char *table_name);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_aggregate_query(mylite_db *database, struct expected_aggregate_query query);
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

    failures += test_min_max_values_persistence_rename_and_drop();
    failures += test_min_max_diagnostics();
    failures += test_independent_min_max_handles();

    return failures == 0 ? 0 : 1;
}

static int test_min_max_values_persistence_rename_and_drop(void) {
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
    failures += create_aggregate_table(database, "numbers");
    failures += execute_ok(database, "ALTER TABLE numbers ALTER COLUMN tie SET INVISIBLE", &result);
    mylite_result_free(result);
    result = NULL;

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        catalog_generation_before_select = catalog->generation;
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        sqlite_generation_before_select = session->sqlite_schema_generation;
    }

    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MIN(i) FROM numbers",
            .column = "MIN(i)",
            .value = "-2",
            .context = "signed int minimum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(i) FROM numbers",
            .column = "MAX(i)",
            .value = "2147483647",
            .context = "signed int maximum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(I) FROM numbers",
            .column = "MAX(I)",
            .value = "2147483647",
            .context = "case-insensitive aggregate column",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MIN(ii) FROM numbers",
            .column = "MIN(ii)",
            .value = "-3",
            .context = "integer alias minimum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MIN(ti) FROM numbers",
            .column = "MIN(ti)",
            .value = "-128",
            .context = "tinyint minimum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(ti) FROM numbers",
            .column = "MAX(ti)",
            .value = "127",
            .context = "tinyint maximum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MIN(ti1) FROM numbers",
            .column = "MIN(ti1)",
            .value = "0",
            .context = "tinyint display-width minimum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(ti1) FROM numbers",
            .column = "MAX(ti1)",
            .value = "1",
            .context = "tinyint display-width maximum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MIN(si) FROM numbers",
            .column = "MIN(si)",
            .value = "-32768",
            .context = "smallint minimum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(si) FROM numbers",
            .column = "MAX(si)",
            .value = "32767",
            .context = "smallint maximum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MIN(mi) FROM numbers",
            .column = "MIN(mi)",
            .value = "-8388608",
            .context = "mediumint minimum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(mi) FROM numbers",
            .column = "MAX(mi)",
            .value = "8388607",
            .context = "mediumint maximum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MIN(bool_col) FROM numbers",
            .column = "MIN(bool_col)",
            .value = "0",
            .context = "bool alias minimum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(boolean_col) FROM numbers",
            .column = "MAX(boolean_col)",
            .value = "1",
            .context = "boolean alias maximum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(iu) FROM numbers",
            .column = "MAX(iu)",
            .value = "4294967295",
            .context = "unsigned int maximum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(integeru) FROM numbers",
            .column = "MAX(integeru)",
            .value = "10",
            .context = "integer unsigned alias maximum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MIN(b) FROM numbers",
            .column = "MIN(b)",
            .value = "-9223372036854775808",
            .context = "bigint minimum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(bu) FROM numbers",
            .column = "MAX(bu)",
            .value = "9223372036854775807",
            .context = "unsigned bigint physical maximum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MIN(n) FROM numbers",
            .column = "MIN(n)",
            .value = "20",
            .context = "nullable minimum ignores nulls",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(n) FROM numbers",
            .column = "MAX(n)",
            .value = "30",
            .context = "nullable maximum ignores nulls",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(tie) FROM numbers",
            .column = "MAX(tie)",
            .value = "2",
            .context = "explicit invisible column maximum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MIN(n) FROM empty_numbers",
            .column = "MIN(n)",
            .value = NULL,
            .context = "empty table minimum",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(n) FROM all_null_numbers",
            .column = "MAX(n)",
            .value = NULL,
            .context = "all null maximum",
        }
    );

    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT min(i) FROM numbers",
            .column = "min(i)",
            .value = "-2",
            .context = "lowercase label",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT Max( n ) FROM numbers",
            .column = "Max( n )",
            .value = "30",
            .context = "spaced label",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(/* inside */n) FROM numbers",
            .column = "MAX(/* inside */ n)",
            .value = "30",
            .context = "block comment label",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT (MIN(i)) FROM numbers",
            .column = "(MIN(i))",
            .value = "-2",
            .context = "parenthesized label",
        }
    );

    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MIN(id) FROM numbers WHERE i > 0",
            .column = "MIN(id)",
            .value = "2",
            .context = "greater than predicate",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(id) FROM numbers WHERE i <> 1",
            .column = "MAX(id)",
            .value = "4",
            .context = "not equal predicate",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MIN(id) FROM numbers WHERE i <= 0",
            .column = "MIN(id)",
            .value = "1",
            .context = "less equal predicate",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(id) FROM numbers WHERE i <=> 1",
            .column = "MAX(id)",
            .value = "2",
            .context = "null safe predicate",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MIN(id) FROM numbers WHERE n IS NULL",
            .column = "MIN(id)",
            .value = "1",
            .context = "is null predicate",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(id) FROM numbers WHERE n IS NOT NULL",
            .column = "MAX(id)",
            .value = "4",
            .context = "is not null predicate",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(i) FROM app.numbers WHERE id = 3",
            .column = "MAX(i)",
            .value = "2147483647",
            .context = "schema-qualified source",
        }
    );

    failures += expect_row_count(database, "-1", "row count after aggregate result set");
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += expect_size(
            (size_t)catalog->generation,
            (size_t)catalog_generation_before_select,
            "aggregate leaves catalog generation"
        );
    }
    if (session != NULL) {
        failures += expect_size(
            (size_t)session->sqlite_schema_generation,
            (size_t)sqlite_generation_before_select,
            "aggregate leaves SQLite schema generation"
        );
    }
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after aggregate reads"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MIN(n) FROM numbers",
            .column = "MIN(n)",
            .value = "20",
            .context = "reopened minimum",
        }
    );
    failures += execute_ok(database, "RENAME TABLE numbers TO minmax_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "SELECT MAX(i) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.numbers' doesn't exist",
        }
    );
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(i) FROM minmax_numbers",
            .column = "MAX(i)",
            .value = "2147483647",
            .context = "renamed source maximum",
        }
    );
    failures += execute_ok(database, "TRUNCATE TABLE minmax_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_aggregate_query(
        database,
        (struct expected_aggregate_query){
            .sql = "SELECT MIN(i) FROM minmax_numbers",
            .column = "MIN(i)",
            .value = NULL,
            .context = "truncated source minimum",
        }
    );
    failures += execute_ok(database, "DROP TABLE minmax_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "SELECT MIN(i) FROM minmax_numbers",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.minmax_numbers' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_min_max_diagnostics(void) {
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
        "SELECT MIN(i) FROM numbers",
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
    failures += create_aggregate_table(database, "numbers");

    failures += execute_error(
        database,
        "SELECT MIN(i) FROM missing",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT MIN(i) FROM missing_schema.numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "SELECT MIN(i) FROM _mylite_reserved.numbers",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "SELECT MIN(i) FROM _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "SELECT MIN(missing) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT MAX(i) FROM numbers WHERE missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT MIN(i)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "MIN/MAX supports only descriptor-backed table reads",
        }
    );
    failures += execute_error(
        database,
        "SELECT MIN(i) FROM DUAL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "MIN/MAX supports only descriptor-backed table reads",
        }
    );
    failures += execute_error(
        database,
        "SELECT MIN(i), MAX(i) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "MIN/MAX supports exactly one aggregate select item",
        }
    );
    failures += execute_error(
        database,
        "SELECT i, MAX(i) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "MIN/MAX supports exactly one aggregate select item",
        }
    );
    failures += execute_error(
        database,
        "SELECT MIN(i) FROM numbers ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "MIN/MAX supports only WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT MAX(i) FROM numbers LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "MIN/MAX supports only WHERE",
        }
    );

    failures += execute_error(
        database,
        "SELECT MIN (i) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT MAX/**/(i) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT MIN(1) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT MAX(NULL) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT MIN(*) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT MIN(numbers.i) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT MAX(DISTINCT i) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT MIN(i) AS m FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT MIN(i) FROM numbers GROUP BY tie",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "WITH cte AS (SELECT MIN(i) FROM numbers) SELECT MIN(i) FROM numbers",
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
        "SELECT MIN(i) FROM numbers",
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

static int test_independent_min_max_handles(void) {
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
    failures += create_aggregate_table(first, "numbers");
    failures += create_empty_table(second, "numbers");
    failures += execute_ok(second, "INSERT INTO numbers VALUES (1, 100), (2, 200)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += expect_aggregate_query(
        first,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(n) FROM numbers",
            .column = "MAX(n)",
            .value = "30",
            .context = "first handle maximum",
        }
    );
    failures += expect_aggregate_query(
        second,
        (struct expected_aggregate_query){
            .sql = "SELECT MAX(n) FROM numbers",
            .column = "MAX(n)",
            .value = "200",
            .context = "second handle maximum",
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

static int create_aggregate_table(mylite_db *database, const char *table_name) {
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
        "tie INT NULL)",
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
        "-128, 0, -32768, -8388608, FALSE, TRUE, NULL, 5, 1), "
        "(2, 1, 5, 2, 8, 3, 4, "
        "-1, 1, 0, 0, TRUE, FALSE, 20, 6, 1), "
        "(3, 2147483647, 6, 4294967295, 9, "
        "9223372036854775807, 9223372036854775807, "
        "127, NULL, 32767, 8388607, NULL, NULL, 30, 7, 2), "
        "(4, 0, 7, 8, 10, 8, 8, "
        "NULL, 1, NULL, NULL, FALSE, TRUE, 20, 8, 2)",
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

static int expect_aggregate_query(mylite_db *database, struct expected_aggregate_query query) {
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
        "%s/mylite_min_max_aggregate_%d_%s.mylite",
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

static int drop_physical_table(sqlite3 *connection, const char *physical_name) {
    char sql[sqlite_sql_capacity];
    int written = snprintf(sql, sizeof(sql), "DROP TABLE \"%s\"", physical_name);

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "drop physical table SQL is too long\n");
        return 1;
    }

    return execute_sql(connection, sql);
}

static int execute_sql(sqlite3 *connection, const char *sql) {
    int rc = sqlite3_exec(connection, sql, NULL, NULL, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQLite exec failed for '%s': %d\n", sql, rc);
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

static int expect_value(const char *actual, const char *expected, const char *context) {
    if (expected == NULL) {
        return expect_true(actual == NULL, context);
    }

    return expect_text(actual, expected, context);
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
