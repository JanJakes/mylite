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
    sqlite_sql_capacity = 512,
    numbers_column_count = 6,
    numbers_nn_column_index = 5,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_unknown = 1105,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_data_out_of_range = 1264,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_single_value_query {
    const char *sql;
    const char *expected;
    const char *context;
};

struct expected_empty_query {
    const char *sql;
    const char *context;
};

static int test_filtered_select_success_persistence_rename_and_drop(void);
static int test_filtered_select_diagnostics(void);
static int test_independent_filtered_select_handles(void);
static int seed_schema(mylite_db *database, const char *name);
static int create_select_tables(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_single_value(mylite_db *database, struct expected_single_value_query query);
static int expect_query_empty(mylite_db *database, struct expected_empty_query query);
static int expect_select_value(
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

    failures += test_filtered_select_success_persistence_rename_and_drop();
    failures += test_filtered_select_diagnostics();
    failures += test_independent_filtered_select_handles();

    return failures == 0 ? 0 : 1;
}

static int test_filtered_select_success_persistence_rename_and_drop(void) {
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

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += seed_schema(database, "app");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_select_tables(database);

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        catalog_generation_before_select = catalog->generation;
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        sqlite_generation_before_select = session->sqlite_schema_generation;
    }

    failures += execute_ok(database, "SELECT * FROM numbers WHERE i = 1", &result);
    failures += expect_size(
        mylite_result_column_count(result),
        numbers_column_count,
        "filtered star column count"
    );
    failures += expect_size(mylite_result_row_count(result), 1U, "filtered star row count");
    failures += expect_text(mylite_result_column_name(result, 0U), "i", "filtered star i name");
    failures += expect_text(mylite_result_column_name(result, 1U), "iu", "filtered star iu name");
    failures += expect_text(mylite_result_column_name(result, 2U), "b", "filtered star b name");
    failures += expect_text(mylite_result_column_name(result, 3U), "bu", "filtered star bu name");
    failures += expect_text(mylite_result_column_name(result, 4U), "n", "filtered star n name");
    failures += expect_text(
        mylite_result_column_name(result, numbers_nn_column_index),
        "nn",
        "filtered star nn name"
    );
    failures += expect_select_value(result, 0U, 0U, "1", "filtered star i value");
    failures += expect_select_value(result, 0U, 1U, "2", "filtered star iu value");
    failures += expect_select_value(result, 0U, 2U, "3", "filtered star b value");
    failures += expect_select_value(result, 0U, 3U, "4", "filtered star bu value");
    failures += expect_select_value(result, 0U, 4U, "9", "filtered star n value");
    failures +=
        expect_select_value(result, 0U, numbers_nn_column_index, "6", "filtered star nn value");
    failures += expect_int64(mylite_result_affected_rows(result), 0, "filtered star affected rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "filtered star warnings");
    mylite_result_free(result);
    result = NULL;

    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT id FROM single_values WHERE id <> 2",
            .expected = "1",
            .context = "not equal angle predicate",
        }
    );
    failures += expect_query_empty(
        database,
        (struct expected_empty_query){
            .sql = "SELECT id FROM single_values WHERE id != 1",
            .context = "not equal bang predicate",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT i FROM numbers WHERE i < 0",
            .expected = "-2",
            .context = "less than predicate",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT i FROM numbers WHERE i <= -2",
            .expected = "-2",
            .context = "less equal predicate",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT i FROM numbers WHERE i > 2147483646",
            .expected = "2147483647",
            .context = "greater than predicate",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT i FROM numbers WHERE i >= 2147483647",
            .expected = "2147483647",
            .context = "greater equal predicate",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT i FROM numbers WHERE i <=> 1",
            .expected = "1",
            .context = "null safe equal predicate",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT id FROM null_probe WHERE n IS NULL",
            .expected = "1",
            .context = "is null predicate",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT id FROM null_probe WHERE n IS NOT NULL",
            .expected = "2",
            .context = "is not null predicate",
        }
    );
    failures += expect_query_empty(
        database,
        (struct expected_empty_query){
            .sql = "SELECT id FROM null_probe WHERE nn IS NULL",
            .context = "not null column is null predicate",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT n FROM numbers WHERE i = -2",
            .expected = NULL,
            .context = "filtered null value",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT i FROM numbers WHERE n = 9",
            .expected = "1",
            .context = "nullable equal predicate",
        }
    );
    failures += expect_query_empty(
        database,
        (struct expected_empty_query){
            .sql = "SELECT i FROM numbers WHERE n <> 9",
            .context = "nullable not equal predicate excludes null rows",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT i FROM numbers WHERE iu >= 4294967295",
            .expected = "2147483647",
            .context = "unsigned int boundary predicate",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT i FROM numbers WHERE b = -9223372036854775808",
            .expected = "-2",
            .context = "signed bigint minimum predicate",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT i FROM numbers WHERE bu = 9223372036854775807",
            .expected = "2147483647",
            .context = "bigint unsigned signed64 predicate",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT id FROM integer_aliases WHERE ii = -3",
            .expected = "1",
            .context = "integer alias signed predicate",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT id FROM integer_aliases WHERE intu = 4294967295",
            .expected = "1",
            .context = "int unsigned predicate",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT id FROM integer_aliases WHERE integeru = 7",
            .expected = "1",
            .context = "integer unsigned predicate",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT i FROM app.numbers WHERE nn = 6",
            .expected = "1",
            .context = "schema-qualified filtered select",
        }
    );

    failures += execute_ok(database, "SELECT i, i FROM numbers WHERE (i = +1)", &result);
    failures += expect_size(mylite_result_column_count(result), 2U, "duplicate filtered columns");
    failures += expect_text(mylite_result_column_name(result, 0U), "i", "duplicate filtered col 1");
    failures += expect_text(mylite_result_column_name(result, 1U), "i", "duplicate filtered col 2");
    failures += expect_size(mylite_result_row_count(result), 1U, "duplicate filtered rows");
    failures += expect_select_value(result, 0U, 0U, "1", "duplicate filtered value 1");
    failures += expect_select_value(result, 0U, 1U, "1", "duplicate filtered value 2");
    mylite_result_free(result);
    result = NULL;

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_size(
            (size_t)catalog->generation,
            (size_t)catalog_generation_before_select,
            "filtered select leaves catalog generation"
        );
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_size(
            (size_t)session->sqlite_schema_generation,
            (size_t)sqlite_generation_before_select,
            "filtered select leaves SQLite schema generation"
        );
    }

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "filtered select preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT i FROM numbers WHERE nn = 6",
            .expected = "1",
            .context = "reopened filtered row",
        }
    );

    failures += execute_ok(database, "RENAME TABLE numbers TO renamed_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE nn = 6",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.numbers' doesn't exist",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT i FROM renamed_numbers WHERE nn = 6",
            .expected = "1",
            .context = "renamed filtered row",
        }
    );

    failures += execute_ok(database, "DROP TABLE renamed_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "SELECT i FROM renamed_numbers WHERE nn = 6",
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

static int test_filtered_select_diagnostics(void) {
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
    failures += seed_schema(database, "app");

    failures += execute_error(
        database,
        "SELECT * FROM numbers WHERE i = 1",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM missing_schema.numbers WHERE i = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM _mylite_reserved.numbers WHERE i = 1",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name",
        }
    );

    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_select_tables(database);

    failures += execute_error(
        database,
        "SELECT * FROM missing_table WHERE i = 1",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM _mylite_reserved WHERE i = 1",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "SELECT missing FROM numbers WHERE i = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE wrong.i = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'wrong.i' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE i = 2147483648",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE iu = -1",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'iu' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE b = 9223372036854775808",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'b' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE bu = 9223372036854775808",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'bu' in WHERE",
        }
    );

    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE 1 = i",
            .expected = "1",
            .context = "literal-left comparison predicate",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE i + 1 = 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE i = NULL",
            .expected = "0",
            .context = "column equal null predicate",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE i = '1'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE supports only integer or boolean predicate literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE i = 1.0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE i = 0x1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE i = ?",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT COUNT(*) FROM numbers WHERE TRUE OR nn = 6",
            .expected = "3",
            .context = "scalar truth logical predicate",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE ! (i = 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE i XOR nn = 6",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE ABS(i) = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE i = b'1'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE supports only integer or boolean predicate literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE i BETWEEN NULL AND 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE i LIKE 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE i REGEXP '1'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE REGEXP predicates support only string columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers JOIN null_probe USING (i) WHERE i = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE i = 1 ORDER BY nn, i",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE i = 1 LIMIT +1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += expect_query_single_value(
        database,
        (struct expected_single_value_query){
            .sql = "SELECT i AS alias FROM numbers WHERE i = 1",
            .expected = "1",
            .context = "select item alias with where",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers GROUP BY i",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "GROUP BY supports one grouped descriptor column and one or more aggregate results",
        }
    );
    failures += execute_error(
        database,
        "SELECT i FROM numbers WHERE i IN (SELECT 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "IN subqueries support one descriptor table source",
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
        "SELECT i FROM numbers WHERE i = 1",
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

static int test_independent_filtered_select_handles(void) {
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
    failures += seed_schema(first, "app");
    failures += seed_schema(second, "app");
    failures += execute_ok(first, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_select_tables(first);
    failures += create_select_tables(second);

    failures += execute_ok(first, "INSERT INTO numbers (i, nn) VALUES (101, 102)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "INSERT INTO numbers (i, nn) VALUES (201, 202)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_single_value(
        first,
        (struct expected_single_value_query){
            .sql = "SELECT nn FROM numbers WHERE i = 101",
            .expected = "102",
            .context = "first filtered row",
        }
    );
    failures += expect_query_single_value(
        second,
        (struct expected_single_value_query){
            .sql = "SELECT nn FROM numbers WHERE i = 201",
            .expected = "202",
            .context = "second filtered row",
        }
    );
    failures += expect_query_empty(
        first,
        (struct expected_empty_query){
            .sql = "SELECT nn FROM numbers WHERE i = 201",
            .context = "first handle excludes second row",
        }
    );
    failures += expect_query_empty(
        second,
        (struct expected_empty_query){
            .sql = "SELECT nn FROM numbers WHERE i = 101",
            .context = "second handle excludes first row",
        }
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

static int create_select_tables(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = execute_ok(
        database,
        "CREATE TABLE numbers ("
        "i INT, "
        "iu INTEGER UNSIGNED, "
        "b BIGINT, "
        "bu BIGINT UNSIGNED, "
        "n INT NULL, "
        "nn INT NOT NULL)",
        &result
    );

    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE single_values (id INT NOT NULL)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE null_probe (id INT NOT NULL, n INT NULL, nn INT NOT NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE integer_aliases ("
        "id INT NOT NULL, "
        "ii INTEGER, "
        "intu INT UNSIGNED, "
        "integeru INTEGER UNSIGNED)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO numbers VALUES "
        "(-2, 0, -9223372036854775808, 0, NULL, 5), "
        "(1, 2, 3, 4, 9, 6), "
        "(2147483647, 4294967295, 9223372036854775807, 9223372036854775807, NULL, 7)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO single_values VALUES (1)", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "INSERT INTO null_probe VALUES (1, NULL, 10), (2, 20, 20)", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "INSERT INTO integer_aliases VALUES (1, -3, 4294967295, 7)", &result);
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

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);

    return failures;
}

static int expect_query_single_value(
    mylite_db *database,
    struct expected_single_value_query query
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), 1U, query.context);
    failures += expect_size(mylite_result_row_count(result), 1U, query.context);
    failures += expect_select_value(result, 0U, 0U, query.expected, query.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    mylite_result_free(result);

    return failures;
}

static int expect_query_empty(mylite_db *database, struct expected_empty_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), 1U, query.context);
    failures += expect_size(mylite_result_row_count(result), 0U, query.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    mylite_result_free(result);

    return failures;
}

static int expect_select_value(
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
        "%s/mylite_select_where_lifecycle_%d_%s.mylite",
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
    char sql[sqlite_sql_capacity];
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
    if (actual == NULL && expected == NULL) {
        return 0;
    }
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
