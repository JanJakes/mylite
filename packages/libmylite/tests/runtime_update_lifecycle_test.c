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
    sql_capacity = 2048,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_unknown = 1105,
    mysql_error_update_table_used = 1093,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_bad_null = 1048,
    mysql_error_data_out_of_range = 1264,
    mysql_error_truncated_wrong_value = 1366,
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

static int test_update_success_persistence_rename_and_drop(void);
static int test_update_diagnostics(void);
static int test_independent_update_handles(void);
static int seed_schema(mylite_db *database, const char *name);
static int create_numbers_table(mylite_db *database, const char *table_name);
static int create_null_order_table(mylite_db *database, const char *table_name);
static int create_string_order_table(mylite_db *database, const char *table_name);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_update_ok(mylite_db *database, const char *sql, int64_t affected_rows);
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

    failures += test_update_success_persistence_rename_and_drop();
    failures += test_update_diagnostics();
    failures += test_independent_update_handles();

    return failures == 0 ? 0 : 1;
}

static int test_update_success_persistence_rename_and_drop(void) {
    static const char *const all_i_five[] = {"5", "5", "5", "5"};
    static const char *const all_i_eleven[] = {"11", "11", "11", "11"};
    static const char *const all_nulls[] = {NULL, NULL, NULL, NULL};
    static const char *const all_i_original[] = {"-2", "1", "2147483647", "0"};
    static const char *const all_iu_original[] = {"0", "2", "4294967295", "8"};
    static const char *const all_nn_original[] = {"5", "6", "7", "8"};
    static const char *const order_default_ids[] = {"1", "4"};
    static const char *const order_desc_ids[] = {"3"};
    static const char *const tie_group_ids[] = {"1", "2"};
    static const char *const schema_update[] = {"3", "77"};
    static const char *const integer_family_row_one[] = {
        "-2147483648",
        "4294967295",
        "0",
        "9223372036854775807",
    };
    static const char *const bigint_minimum[] = {"-9223372036854775808"};
    static const char *const null_order_asc[] = {"1", "10", "2", "0", "3", "0", "4", "0"};
    static const char *const null_order_desc[] = {"1", "0", "2", "0", "3", "0", "4", "10"};
    static const char *const string_where_order[] = {
        "1",
        "b",
        "old",
        "2",
        "a",
        "x",
        "3",
        "c",
        "old",
        "4",
        NULL,
        "old",
    };
    static const char *const string_order_asc[] = {
        "1",
        "b",
        "old",
        "2",
        "a",
        "old",
        "3",
        "c",
        "old",
        "4",
        NULL,
        "first",
    };
    static const char *const string_order_desc[] = {
        "1",
        "b",
        "old",
        "2",
        "a",
        "old",
        "3",
        "c",
        "last",
        "4",
        NULL,
        "old",
    };
    static const char *const persisted_i[] = {"42"};
    static const char *const rename_i[] = {"33"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation_before_update = 0U;
    uint64_t sqlite_generation_before_update = 0U;
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

    failures += create_numbers_table(database, "upd_full");
    failures += expect_update_ok(database, "UPDATE upd_full SET i = 5", 4);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i FROM upd_full ORDER BY id",
            .values = all_i_five,
            .column_count = 1U,
            .row_count = 4U,
            .context = "full-table update values",
        }
    );
    failures += expect_update_ok(database, "UPDATE upd_full SET i = 5", 0);

    failures += create_numbers_table(database, "upd_null");
    failures += expect_update_ok(database, "UPDATE upd_null SET n = NULL WHERE n IS NULL", 0);
    failures += expect_update_ok(database, "UPDATE upd_null SET n = NULL WHERE n IS NOT NULL", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT n FROM upd_null ORDER BY id",
            .values = all_nulls,
            .column_count = 1U,
            .row_count = 4U,
            .context = "nullable update values",
        }
    );

    failures += create_numbers_table(database, "upd_catalog");
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        catalog_generation_before_update = catalog->generation;
    }
    if (session != NULL) {
        sqlite_generation_before_update = session->sqlite_schema_generation;
    }
    failures += expect_update_ok(database, "UPDATE upd_catalog SET i = 10 WHERE i = 1", 1);
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += expect_size(
            (size_t)catalog->generation,
            (size_t)catalog_generation_before_update,
            "update leaves catalog generation"
        );
    }
    if (session != NULL) {
        failures += expect_size(
            (size_t)session->sqlite_schema_generation,
            (size_t)sqlite_generation_before_update,
            "update leaves SQLite schema generation"
        );
    }

    failures += create_numbers_table(database, "upd_not_equal");
    failures += expect_update_ok(database, "UPDATE upd_not_equal SET i = 10 WHERE i <> 1", 3);
    failures += create_numbers_table(database, "upd_bang_equal");
    failures += expect_update_ok(database, "UPDATE upd_bang_equal SET i = 10 WHERE i != 1", 3);
    failures += create_numbers_table(database, "upd_less");
    failures += expect_update_ok(database, "UPDATE upd_less SET i = 10 WHERE i < 0", 1);
    failures += create_numbers_table(database, "upd_less_equal");
    failures += expect_update_ok(database, "UPDATE upd_less_equal SET i = 10 WHERE i <= 0", 2);
    failures += create_numbers_table(database, "upd_greater");
    failures += expect_update_ok(database, "UPDATE upd_greater SET i = 10 WHERE i > 1", 1);
    failures += create_numbers_table(database, "upd_greater_equal");
    failures += expect_update_ok(database, "UPDATE upd_greater_equal SET i = 10 WHERE i >= 1", 2);
    failures += create_numbers_table(database, "upd_null_safe");
    failures += expect_update_ok(database, "UPDATE upd_null_safe SET i = 10 WHERE i <=> 1", 1);

    failures += create_numbers_table(database, "upd_integer_family");
    failures += expect_update_ok(
        database,
        "UPDATE upd_integer_family SET ii = -2147483648 WHERE id = 1",
        1
    );
    failures +=
        expect_update_ok(database, "UPDATE upd_integer_family SET iu = 4294967295 WHERE id = 1", 1);
    failures +=
        expect_update_ok(database, "UPDATE upd_integer_family SET integeru = 0 WHERE id = 1", 1);
    failures += expect_update_ok(
        database,
        "UPDATE upd_integer_family SET b = -9223372036854775808 WHERE id = 2",
        1
    );
    failures += expect_update_ok(
        database,
        "UPDATE upd_integer_family SET bu = 9223372036854775807 WHERE id = 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ii, iu, integeru, bu FROM upd_integer_family WHERE id = 1 ORDER BY id",
            .values = integer_family_row_one,
            .column_count = 4U,
            .row_count = 1U,
            .context = "integer family update values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT b FROM upd_integer_family WHERE id = 2 ORDER BY id",
            .values = bigint_minimum,
            .column_count = 1U,
            .row_count = 1U,
            .context = "bigint minimum update value",
        }
    );

    failures += create_numbers_table(database, "upd_order_default");
    failures +=
        expect_update_ok(database, "UPDATE upd_order_default SET i = 100 ORDER BY i LIMIT 2", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM upd_order_default WHERE i = 100 ORDER BY id",
            .values = order_default_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "default order limited update",
        }
    );

    failures += create_numbers_table(database, "upd_order_desc");
    failures +=
        expect_update_ok(database, "UPDATE upd_order_desc SET i = 100 ORDER BY i DESC LIMIT 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM upd_order_desc WHERE i = 100 ORDER BY id",
            .values = order_desc_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "desc order limited update",
        }
    );

    failures += create_numbers_table(database, "upd_tie_group");
    failures +=
        expect_update_ok(database, "UPDATE upd_tie_group SET i = 100 ORDER BY tie LIMIT 2", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM upd_tie_group WHERE i = 100 ORDER BY id",
            .values = tie_group_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "duplicate tie group limited update",
        }
    );

    failures += create_null_order_table(database, "upd_null_order_asc");
    failures += expect_update_ok(
        database,
        "UPDATE upd_null_order_asc SET i = 10 ORDER BY n ASC LIMIT 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i FROM upd_null_order_asc ORDER BY id",
            .values = null_order_asc,
            .column_count = 2U,
            .row_count = 4U,
            .context = "NULL ascending order limited update",
        }
    );

    failures += create_null_order_table(database, "upd_null_order_desc");
    failures += expect_update_ok(
        database,
        "UPDATE upd_null_order_desc SET i = 10 ORDER BY n DESC LIMIT 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i FROM upd_null_order_desc ORDER BY id",
            .values = null_order_desc,
            .column_count = 2U,
            .row_count = 4U,
            .context = "NULL descending order limited update",
        }
    );

    failures += create_numbers_table(database, "upd_order_no_limit");
    failures += expect_update_ok(database, "UPDATE upd_order_no_limit SET i = 11 ORDER BY i", 4);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i FROM upd_order_no_limit ORDER BY id",
            .values = all_i_eleven,
            .column_count = 1U,
            .row_count = 4U,
            .context = "order without limit update",
        }
    );

    failures += create_string_order_table(database, "upd_string_where_order");
    failures += expect_update_ok(
        database,
        "UPDATE upd_string_where_order SET v = 'x' WHERE k = 'a' ORDER BY k LIMIT 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, k, v FROM upd_string_where_order ORDER BY id",
            .values = string_where_order,
            .column_count = 3U,
            .row_count = 4U,
            .context = "string WHERE ORDER BY LIMIT update",
        }
    );

    failures += create_string_order_table(database, "upd_string_order_asc");
    failures += expect_update_ok(
        database,
        "UPDATE upd_string_order_asc SET v = 'first' ORDER BY k ASC LIMIT 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, k, v FROM upd_string_order_asc ORDER BY id",
            .values = string_order_asc,
            .column_count = 3U,
            .row_count = 4U,
            .context = "nullable string ascending order limited update",
        }
    );

    failures += create_string_order_table(database, "upd_string_order_desc");
    failures += expect_update_ok(
        database,
        "UPDATE upd_string_order_desc SET v = 'last' ORDER BY k DESC LIMIT 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, k, v FROM upd_string_order_desc ORDER BY id",
            .values = string_order_desc,
            .column_count = 3U,
            .row_count = 4U,
            .context = "nullable string descending order limited update",
        }
    );

    failures += create_numbers_table(database, "upd_limit_zero");
    failures += expect_update_ok(database, "UPDATE upd_limit_zero SET i = 10 LIMIT 0", 0);
    failures += create_numbers_table(database, "upd_limit_exact");
    failures += expect_update_ok(database, "UPDATE upd_limit_exact SET i = 10 LIMIT 2", 2);
    failures += create_numbers_table(database, "upd_limit_large");
    failures += expect_update_ok(database, "UPDATE upd_limit_large SET i = 10 LIMIT 10", 4);
    failures += create_numbers_table(database, "upd_limit_max");
    failures += expect_update_ok(
        database,
        "UPDATE upd_limit_max SET i = 10 ORDER BY id LIMIT 9223372036854775807",
        4
    );
    failures += create_numbers_table(database, "upd_limit_noop");
    failures +=
        expect_update_ok(database, "UPDATE upd_limit_noop SET i = -2 ORDER BY id LIMIT 1", 0);

    failures += create_numbers_table(database, "upd_bad_null_no_match");
    failures +=
        expect_update_ok(database, "UPDATE upd_bad_null_no_match SET nn = NULL WHERE id = 999", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT nn FROM upd_bad_null_no_match ORDER BY id",
            .values = all_nn_original,
            .column_count = 1U,
            .row_count = 4U,
            .context = "NOT NULL assignment skipped by no-match predicate",
        }
    );

    failures += create_numbers_table(database, "upd_bad_null_limit_zero");
    failures +=
        expect_update_ok(database, "UPDATE upd_bad_null_limit_zero SET nn = NULL LIMIT 0", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT nn FROM upd_bad_null_limit_zero ORDER BY id",
            .values = all_nn_original,
            .column_count = 1U,
            .row_count = 4U,
            .context = "NOT NULL assignment skipped by LIMIT zero",
        }
    );

    failures += create_numbers_table(database, "upd_range_no_match");
    failures += expect_update_ok(
        database,
        "UPDATE upd_range_no_match SET i = 2147483648 WHERE id = 999",
        0
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i FROM upd_range_no_match ORDER BY id",
            .values = all_i_original,
            .column_count = 1U,
            .row_count = 4U,
            .context = "signed range assignment skipped by no-match predicate",
        }
    );

    failures += create_numbers_table(database, "upd_range_limit_zero");
    failures +=
        expect_update_ok(database, "UPDATE upd_range_limit_zero SET i = 2147483648 LIMIT 0", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i FROM upd_range_limit_zero ORDER BY id",
            .values = all_i_original,
            .column_count = 1U,
            .row_count = 4U,
            .context = "signed range assignment skipped by LIMIT zero",
        }
    );

    failures += create_numbers_table(database, "upd_unsigned_no_match");
    failures +=
        expect_update_ok(database, "UPDATE upd_unsigned_no_match SET iu = -1 WHERE id = 999", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT iu FROM upd_unsigned_no_match ORDER BY id",
            .values = all_iu_original,
            .column_count = 1U,
            .row_count = 4U,
            .context = "unsigned range assignment skipped by no-match predicate",
        }
    );

    failures += create_numbers_table(database, "upd_unsigned_limit_zero");
    failures += expect_update_ok(database, "UPDATE upd_unsigned_limit_zero SET iu = -1 LIMIT 0", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT iu FROM upd_unsigned_limit_zero ORDER BY id",
            .values = all_iu_original,
            .column_count = 1U,
            .row_count = 4U,
            .context = "unsigned range assignment skipped by LIMIT zero",
        }
    );

    failures += create_numbers_table(database, "upd_schema");
    failures += expect_update_ok(
        database,
        "UPDATE app.upd_schema SET nn = 77 WHERE nn >= 6 ORDER BY i DESC LIMIT 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM upd_schema WHERE nn = 77 ORDER BY id",
            .values = schema_update,
            .column_count = 2U,
            .row_count = 1U,
            .context = "schema-qualified update",
        }
    );

    failures += create_numbers_table(database, "upd_persist");
    failures += expect_update_ok(database, "UPDATE upd_persist SET i = 42 WHERE id = 2", 1);
    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "update preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i FROM upd_persist WHERE id = 2",
            .values = persisted_i,
            .column_count = 1U,
            .row_count = 1U,
            .context = "update persisted after reopen",
        }
    );

    failures += create_numbers_table(database, "upd_rename");
    failures += execute_ok(database, "RENAME TABLE upd_rename TO upd_renamed", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "UPDATE upd_rename SET i = 33",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.upd_rename' doesn't exist",
        }
    );
    failures += expect_update_ok(database, "UPDATE upd_renamed SET i = 33 WHERE id = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i FROM upd_renamed WHERE id = 1",
            .values = rename_i,
            .column_count = 1U,
            .row_count = 1U,
            .context = "update after rename",
        }
    );
    failures += execute_ok(database, "DROP TABLE upd_renamed", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "UPDATE upd_renamed SET i = 33",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.upd_renamed' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_update_diagnostics(void) {
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
        "UPDATE numbers SET i = 1",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "UPDATE missing_schema.numbers SET i = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );
    failures += execute_error(
        database,
        "UPDATE _mylite_reserved.numbers SET i = 1",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name",
        }
    );

    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_numbers_table(database, "numbers");

    failures += execute_error(
        database,
        "UPDATE missing_table SET i = 1",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "UPDATE _mylite_reserved SET i = 1",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = 1 WHERE missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = 1 ORDER BY missing LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET numbers.i = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UPDATE supports only unqualified assignment columns",
        }
    );
    failures += expect_update_ok(database, "UPDATE numbers SET i = 1 WHERE numbers.id = -999", 0);
    failures += execute_error(
        database,
        "UPDATE numbers SET i = 1 ORDER BY numbers.id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ORDER BY supports only unqualified descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = 1, i = 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UPDATE multiple assignments do not support duplicate targets",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET nn = NULL",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'nn' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = 2147483648",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' at row 1",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET iu = -1",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'iu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET bu = 9223372036854775808",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'bu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = 1 ORDER BY id LIMIT 9223372036854775808",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "LIMIT literal is outside the supported range",
        }
    );

    failures += execute_error(
        database,
        "UPDATE numbers SET i = 1 / 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = nn",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = 'abc'",
        (struct expected_sql_error){
            .code = mysql_error_truncated_wrong_value,
            .sqlstate = "HY000",
            .message_part = "Incorrect integer value: 'abc' for column 'i' at row 1",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = 1 ORDER BY 1 LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = 1 ORDER BY id + 1 LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = 1 ORDER BY id, nn LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = 1 ORDER BY id LIMIT +1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = 1 ORDER BY id LIMIT -1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = 1 ORDER BY id LIMIT 1 OFFSET 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = 1 ORDER BY id LIMIT 1, 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = 1.0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "UPDATE supports only integer, boolean, NULL, and DEFAULT assignment values",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = 0x1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "UPDATE supports only integer, boolean, NULL, and DEFAULT assignment values",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = b'1'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "UPDATE supports only integer, boolean, NULL, and DEFAULT assignment values",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = ?",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers AS n SET i = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures +=
        expect_update_ok(database, "UPDATE LOW_PRIORITY numbers SET i = 1 WHERE id = -999", 0);
    failures += expect_update_ok(database, "UPDATE IGNORE numbers SET i = 1 WHERE id = -999", 0);
    failures += execute_error(
        database,
        "UPDATE numbers PARTITION (p0) SET i = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers, other_numbers SET i = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers JOIN other_numbers SET i = 1",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.other_numbers' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "WITH ids AS (SELECT id FROM numbers) UPDATE numbers SET i = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = (SELECT id FROM numbers)",
        (struct expected_sql_error){
            .code = mysql_error_update_table_used,
            .sqlstate = "HY000",
            .message_part = "You can't specify target table 'numbers' for update in FROM clause",
        }
    );
    failures += execute_error(
        database,
        "UPDATE numbers SET i = ABS(id)",
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
        "UPDATE rowid_shadow SET rowid = 4 LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UPDATE LIMIT requires an unshadowed SQLite rowid alias",
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
        "UPDATE numbers SET i = 1 WHERE id = 1",
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

static int test_independent_update_handles(void) {
    static const char *const first_expected[] = {"77"};
    static const char *const second_expected[] = {"-2"};
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
    failures += create_numbers_table(first, "numbers");
    failures += create_numbers_table(second, "numbers");
    failures += expect_update_ok(first, "UPDATE numbers SET i = 77 WHERE id = 1", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT i FROM numbers WHERE id = 1",
            .values = first_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first independent update",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT i FROM numbers WHERE id = 1",
            .values = second_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second handle remains unchanged",
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

static int create_numbers_table(mylite_db *database, const char *table_name) {
    char sql[sql_capacity];
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
        "(1, -2, -3, 0, 7, -9223372036854775808, 0, NULL, 5, 1), "
        "(2, 1, 5, 2, 8, 3, 4, 9, 6, 1), "
        "(3, 2147483647, 6, 4294967295, 9, "
        "9223372036854775807, 9223372036854775807, NULL, 7, 2), "
        "(4, 0, 7, 8, 10, 8, 8, 9, 8, 2)",
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

static int create_null_order_table(mylite_db *database, const char *table_name) {
    char sql[sql_capacity];
    mylite_result *result = NULL;
    int written =
        snprintf(sql, sizeof(sql), "CREATE TABLE %s (id INT NOT NULL, i INT, n INT)", table_name);
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "create NULL-order table SQL is too long for %s\n", table_name);
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);
    result = NULL;

    written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO %s VALUES (1, 0, NULL), (2, 0, -1), (3, 0, 5), (4, 0, 9)",
        table_name
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "insert NULL-order SQL is too long for %s\n", table_name);
        return failures + 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);

    return failures;
}

static int create_string_order_table(mylite_db *database, const char *table_name) {
    char sql[sql_capacity];
    mylite_result *result = NULL;
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s (id INT NOT NULL, k VARCHAR(16) NULL, v VARCHAR(16) NULL)",
        table_name
    );
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "create string-order table SQL is too long for %s\n", table_name);
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);
    result = NULL;

    written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO %s VALUES (1, 'b', 'old'), (2, 'a', 'old'), "
        "(3, 'c', 'old'), (4, NULL, 'old')",
        table_name
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "insert string-order SQL is too long for %s\n", table_name);
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

static int expect_update_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "update column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "update row count");
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, "update affected");
    failures += expect_size(mylite_result_warning_count(result), 0U, "update warning count");
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
        "%s/mylite_update_lifecycle_%d_%s.mylite",
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
            "%s: expected text '%s', got '%s'\n",
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
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
