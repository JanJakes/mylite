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
    size_t value_count;
    const char *context;
};

static int test_delete_success_persistence_rename_and_drop(void);
static int test_delete_diagnostics(void);
static int test_independent_delete_handles(void);
static int seed_schema(mylite_db *database, const char *name);
static int create_numbers_table(mylite_db *database, const char *table_name);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_delete_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_delete_remaining(
    mylite_db *database,
    const char *table_name,
    int64_t affected_rows,
    const char *sql,
    const char *const *expected_ids,
    size_t expected_id_count,
    const char *context
);
static int expect_table_ids(
    mylite_db *database,
    const char *table_name,
    const char *const *expected_ids,
    size_t expected_id_count,
    const char *context
);
static int expect_query_row_count(
    mylite_db *database,
    const char *sql,
    size_t expected_row_count,
    const char *context
);
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

    failures += test_delete_success_persistence_rename_and_drop();
    failures += test_delete_diagnostics();
    failures += test_independent_delete_handles();

    return failures == 0 ? 0 : 1;
}

static int test_delete_success_persistence_rename_and_drop(void) {
    static const char *const all_ids[] = {"1", "2", "3", "4"};
    static const char *const ids_1_2_4[] = {"1", "2", "4"};
    static const char *const ids_1_3[] = {"1", "3"};
    static const char *const ids_1_3_4[] = {"1", "3", "4"};
    static const char *const ids_1_4[] = {"1", "4"};
    static const char *const ids_2[] = {"2"};
    static const char *const ids_2_3[] = {"2", "3"};
    static const char *const ids_2_3_4[] = {"2", "3", "4"};
    static const char *const ids_2_4[] = {"2", "4"};
    static const char *const ids_3_4[] = {"3", "4"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation_before_delete = 0U;
    uint64_t sqlite_generation_before_delete = 0U;
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

    failures += create_numbers_table(database, "del_full");
    failures += expect_delete_remaining(
        database,
        "del_full",
        4,
        "DELETE FROM del_full",
        NULL,
        0U,
        "full-table delete"
    );

    failures += create_numbers_table(database, "del_no_match");
    failures += expect_delete_remaining(
        database,
        "del_no_match",
        0,
        "DELETE FROM del_no_match WHERE i < -100",
        all_ids,
        sizeof(all_ids) / sizeof(all_ids[0]),
        "no-match delete"
    );

    failures += create_numbers_table(database, "del_equal");
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        catalog_generation_before_delete = catalog->generation;
    }
    if (session != NULL) {
        sqlite_generation_before_delete = session->sqlite_schema_generation;
    }
    failures += expect_delete_remaining(
        database,
        "del_equal",
        1,
        "DELETE FROM del_equal WHERE i = 1",
        ids_1_3_4,
        sizeof(ids_1_3_4) / sizeof(ids_1_3_4[0]),
        "equal predicate delete"
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += expect_size(
            (size_t)catalog->generation,
            (size_t)catalog_generation_before_delete,
            "delete leaves catalog generation"
        );
    }
    if (session != NULL) {
        failures += expect_size(
            (size_t)session->sqlite_schema_generation,
            (size_t)sqlite_generation_before_delete,
            "delete leaves SQLite schema generation"
        );
    }

    failures += create_numbers_table(database, "del_not_equal");
    failures += expect_delete_remaining(
        database,
        "del_not_equal",
        3,
        "DELETE FROM del_not_equal WHERE i <> 1",
        ids_2,
        sizeof(ids_2) / sizeof(ids_2[0]),
        "not-equal predicate delete"
    );

    failures += create_numbers_table(database, "del_bang_equal");
    failures += expect_delete_remaining(
        database,
        "del_bang_equal",
        3,
        "DELETE FROM del_bang_equal WHERE i != 1",
        ids_2,
        sizeof(ids_2) / sizeof(ids_2[0]),
        "bang-equal predicate delete"
    );

    failures += create_numbers_table(database, "del_less");
    failures += expect_delete_remaining(
        database,
        "del_less",
        1,
        "DELETE FROM del_less WHERE i < 0",
        ids_2_3_4,
        sizeof(ids_2_3_4) / sizeof(ids_2_3_4[0]),
        "less-than predicate delete"
    );

    failures += create_numbers_table(database, "del_less_equal");
    failures += expect_delete_remaining(
        database,
        "del_less_equal",
        2,
        "DELETE FROM del_less_equal WHERE i <= 0",
        ids_2_3,
        sizeof(ids_2_3) / sizeof(ids_2_3[0]),
        "less-equal predicate delete"
    );

    failures += create_numbers_table(database, "del_greater");
    failures += expect_delete_remaining(
        database,
        "del_greater",
        1,
        "DELETE FROM del_greater WHERE i > 1",
        ids_1_2_4,
        sizeof(ids_1_2_4) / sizeof(ids_1_2_4[0]),
        "greater-than predicate delete"
    );

    failures += create_numbers_table(database, "del_greater_equal");
    failures += expect_delete_remaining(
        database,
        "del_greater_equal",
        2,
        "DELETE FROM del_greater_equal WHERE i >= 1",
        ids_1_4,
        sizeof(ids_1_4) / sizeof(ids_1_4[0]),
        "greater-equal predicate delete"
    );

    failures += create_numbers_table(database, "del_null_safe");
    failures += expect_delete_remaining(
        database,
        "del_null_safe",
        1,
        "DELETE FROM del_null_safe WHERE i <=> 1",
        ids_1_3_4,
        sizeof(ids_1_3_4) / sizeof(ids_1_3_4[0]),
        "null-safe predicate delete"
    );

    failures += create_numbers_table(database, "del_alias");
    failures += expect_delete_remaining(
        database,
        "del_alias",
        1,
        "DELETE FROM del_alias AS d WHERE d.id = 1 LIMIT 1",
        ids_2_3_4,
        sizeof(ids_2_3_4) / sizeof(ids_2_3_4[0]),
        "aliased single-table delete"
    );

    failures += create_numbers_table(database, "del_is_null");
    failures += expect_delete_remaining(
        database,
        "del_is_null",
        2,
        "DELETE FROM del_is_null WHERE n IS NULL",
        ids_2_4,
        sizeof(ids_2_4) / sizeof(ids_2_4[0]),
        "is-null predicate delete"
    );

    failures += create_numbers_table(database, "del_is_not_null");
    failures += expect_delete_remaining(
        database,
        "del_is_not_null",
        2,
        "DELETE FROM del_is_not_null WHERE n IS NOT NULL",
        ids_1_3,
        sizeof(ids_1_3) / sizeof(ids_1_3[0]),
        "is-not-null predicate delete"
    );

    failures += create_numbers_table(database, "del_order_default");
    failures += expect_delete_remaining(
        database,
        "del_order_default",
        2,
        "DELETE FROM del_order_default ORDER BY i LIMIT 2",
        ids_2_3,
        sizeof(ids_2_3) / sizeof(ids_2_3[0]),
        "default order limited delete"
    );

    failures += create_numbers_table(database, "del_order_asc");
    failures += expect_delete_remaining(
        database,
        "del_order_asc",
        2,
        "DELETE FROM del_order_asc ORDER BY i ASC LIMIT 2",
        ids_2_3,
        sizeof(ids_2_3) / sizeof(ids_2_3[0]),
        "asc order limited delete"
    );

    failures += create_numbers_table(database, "del_order_desc");
    failures += expect_delete_remaining(
        database,
        "del_order_desc",
        1,
        "DELETE FROM del_order_desc ORDER BY i DESC LIMIT 1",
        ids_1_2_4,
        sizeof(ids_1_2_4) / sizeof(ids_1_2_4[0]),
        "desc order limited delete"
    );

    failures += create_numbers_table(database, "del_null_order_asc");
    failures += expect_delete_remaining(
        database,
        "del_null_order_asc",
        2,
        "DELETE FROM del_null_order_asc ORDER BY n LIMIT 2",
        ids_2_4,
        sizeof(ids_2_4) / sizeof(ids_2_4[0]),
        "null ascending order limited delete"
    );

    failures += create_numbers_table(database, "del_null_order_desc");
    failures += expect_delete_remaining(
        database,
        "del_null_order_desc",
        2,
        "DELETE FROM del_null_order_desc ORDER BY n DESC LIMIT 2",
        ids_1_3,
        sizeof(ids_1_3) / sizeof(ids_1_3[0]),
        "null descending order limited delete"
    );

    failures += create_numbers_table(database, "del_tie_group");
    failures += expect_delete_remaining(
        database,
        "del_tie_group",
        2,
        "DELETE FROM del_tie_group ORDER BY tie LIMIT 2",
        ids_3_4,
        sizeof(ids_3_4) / sizeof(ids_3_4[0]),
        "duplicate tie group limited delete"
    );

    failures += create_numbers_table(database, "del_order_no_limit");
    failures += expect_delete_remaining(
        database,
        "del_order_no_limit",
        4,
        "DELETE FROM del_order_no_limit ORDER BY i",
        NULL,
        0U,
        "order without limit delete"
    );

    failures += create_numbers_table(database, "del_limit_zero");
    failures += expect_delete_remaining(
        database,
        "del_limit_zero",
        0,
        "DELETE FROM del_limit_zero LIMIT 0",
        all_ids,
        sizeof(all_ids) / sizeof(all_ids[0]),
        "limit zero delete"
    );

    failures += create_numbers_table(database, "del_limit_exact");
    failures += expect_delete_ok(database, "DELETE FROM del_limit_exact LIMIT 2", 2);
    failures += expect_query_row_count(
        database,
        "SELECT id FROM del_limit_exact ORDER BY id LIMIT 10",
        2U,
        "exact unordered limit delete remaining count"
    );

    failures += create_numbers_table(database, "del_limit_large");
    failures += expect_delete_remaining(
        database,
        "del_limit_large",
        4,
        "DELETE FROM del_limit_large LIMIT 10",
        NULL,
        0U,
        "large limit delete"
    );

    failures += create_numbers_table(database, "del_limit_max");
    failures += expect_delete_remaining(
        database,
        "del_limit_max",
        4,
        "DELETE FROM del_limit_max ORDER BY id LIMIT 9223372036854775807",
        NULL,
        0U,
        "maximum supported limit delete"
    );

    failures += create_numbers_table(database, "del_integer_family");
    failures += expect_delete_remaining(
        database,
        "del_integer_family",
        1,
        "DELETE FROM del_integer_family WHERE ii = -3",
        ids_2_3_4,
        sizeof(ids_2_3_4) / sizeof(ids_2_3_4[0]),
        "integer alias predicate delete"
    );
    failures += expect_delete_remaining(
        database,
        "del_integer_family",
        1,
        "DELETE FROM del_integer_family WHERE iu = 4294967295",
        ids_2_4,
        sizeof(ids_2_4) / sizeof(ids_2_4[0]),
        "unsigned int predicate delete"
    );
    failures += expect_delete_remaining(
        database,
        "del_integer_family",
        1,
        "DELETE FROM del_integer_family WHERE integeru = 10",
        ids_2,
        sizeof(ids_2) / sizeof(ids_2[0]),
        "integer unsigned alias predicate delete"
    );
    failures += expect_delete_remaining(
        database,
        "del_integer_family",
        1,
        "DELETE FROM del_integer_family WHERE bu = 4",
        NULL,
        0U,
        "bigint unsigned predicate delete"
    );

    failures += create_numbers_table(database, "del_bigint_family");
    failures += expect_delete_remaining(
        database,
        "del_bigint_family",
        1,
        "DELETE FROM del_bigint_family WHERE b = -9223372036854775808",
        ids_2_3_4,
        sizeof(ids_2_3_4) / sizeof(ids_2_3_4[0]),
        "bigint minimum predicate delete"
    );

    failures += create_numbers_table(database, "del_bigint_unsigned_max");
    failures += expect_delete_remaining(
        database,
        "del_bigint_unsigned_max",
        1,
        "DELETE FROM del_bigint_unsigned_max WHERE bu = 9223372036854775807",
        ids_1_2_4,
        sizeof(ids_1_2_4) / sizeof(ids_1_2_4[0]),
        "bigint unsigned maximum supported predicate delete"
    );

    failures += create_numbers_table(database, "del_schema_qualified");
    failures += expect_delete_remaining(
        database,
        "del_schema_qualified",
        1,
        "DELETE FROM app.del_schema_qualified WHERE nn >= 6 ORDER BY i DESC LIMIT 1",
        ids_1_2_4,
        sizeof(ids_1_2_4) / sizeof(ids_1_2_4[0]),
        "schema-qualified delete"
    );

    failures += create_numbers_table(database, "del_subquery_source");
    failures += create_numbers_table(database, "del_subquery_target");
    failures += expect_delete_remaining(
        database,
        "del_subquery_target",
        2,
        "DELETE FROM del_subquery_target "
        "WHERE id IN (SELECT id FROM del_subquery_source WHERE i IN (-2, 1))",
        ids_3_4,
        sizeof(ids_3_4) / sizeof(ids_3_4[0]),
        "subquery IN predicate delete"
    );

    failures += create_numbers_table(database, "del_persist");
    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "delete preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_delete_remaining(
        database,
        "del_persist",
        1,
        "DELETE FROM del_persist WHERE id = 2",
        ids_1_3_4,
        sizeof(ids_1_3_4) / sizeof(ids_1_3_4[0]),
        "reopened delete"
    );
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "second reopen success file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_table_ids(
        database,
        "del_persist",
        ids_1_3_4,
        sizeof(ids_1_3_4) / sizeof(ids_1_3_4[0]),
        "delete persisted after reopen"
    );

    failures += create_numbers_table(database, "del_rename");
    failures += execute_ok(database, "RENAME TABLE del_rename TO del_renamed", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "DELETE FROM del_rename",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.del_rename' doesn't exist",
        }
    );
    failures += expect_delete_remaining(
        database,
        "del_renamed",
        1,
        "DELETE FROM del_renamed WHERE id = 1",
        ids_2_3_4,
        sizeof(ids_2_3_4) / sizeof(ids_2_3_4[0]),
        "delete after rename"
    );
    failures += execute_ok(database, "DROP TABLE del_renamed", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "DELETE FROM del_renamed",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.del_renamed' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_delete_diagnostics(void) {
    static const char *const ids_2_3_4[] = {"2", "3", "4"};
    static const char *const ids_1_3_4[] = {"1", "3", "4"};
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
        "DELETE FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM missing_schema.numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM _mylite_reserved.numbers",
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
        "DELETE FROM missing_table",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers WHERE missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers ORDER BY missing LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers WHERE numbers.id = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE supports only unqualified predicate columns",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers ORDER BY numbers.id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ORDER BY supports only unqualified descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers ORDER BY id LIMIT 9223372036854775808",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "LIMIT literal is outside the supported range",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers ORDER BY id LIMIT 18446744073709551615",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "LIMIT literal is outside the supported range",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers ORDER BY id LIMIT +1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers ORDER BY id LIMIT -1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers ORDER BY id LIMIT 1.0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers ORDER BY id LIMIT '1'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers ORDER BY id LIMIT 0x1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers ORDER BY id LIMIT b'1'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers ORDER BY id LIMIT ?",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers ORDER BY id LIMIT 1 OFFSET 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers ORDER BY id LIMIT 1, 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers ORDER BY id, nn LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers ORDER BY id + 1 LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers ORDER BY 1 LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += create_numbers_table(database, "delete_low_priority");
    failures += expect_delete_remaining(
        database,
        "delete_low_priority",
        1,
        "DELETE LOW_PRIORITY FROM delete_low_priority WHERE id = 1",
        ids_2_3_4,
        sizeof(ids_2_3_4) / sizeof(ids_2_3_4[0]),
        "DELETE LOW_PRIORITY no-op modifier"
    );
    failures += create_numbers_table(database, "delete_quick");
    failures += expect_delete_remaining(
        database,
        "delete_quick",
        1,
        "DELETE QUICK FROM delete_quick WHERE id = 2",
        ids_1_3_4,
        sizeof(ids_1_3_4) / sizeof(ids_1_3_4[0]),
        "DELETE QUICK no-op modifier"
    );
    failures += execute_error(
        database,
        "DELETE IGNORE FROM numbers WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "DELETE numbers FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers USING numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "WITH doomed AS (SELECT id FROM numbers) DELETE FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM numbers WHERE ABS(id) = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "DELETE numbers FROM numbers JOIN other_numbers ON numbers.id = other_numbers.id",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.other_numbers' doesn't exist",
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
        "DELETE FROM rowid_shadow LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DELETE LIMIT requires an unshadowed SQLite rowid alias",
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
        "DELETE FROM numbers WHERE id = 1",
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

static int test_independent_delete_handles(void) {
    static const char *const first_expected[] = {"2", "3", "4"};
    static const char *const second_expected[] = {"1", "2", "3", "4"};
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
    failures += expect_delete_remaining(
        first,
        "numbers",
        1,
        "DELETE FROM numbers WHERE id = 1",
        first_expected,
        sizeof(first_expected) / sizeof(first_expected[0]),
        "first independent delete"
    );
    failures += expect_table_ids(
        second,
        "numbers",
        second_expected,
        sizeof(second_expected) / sizeof(second_expected[0]),
        "second handle remains unchanged"
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
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);

    return failures;
}

static int expect_delete_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "delete column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "delete row count");
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, "delete affected");
    failures += expect_size(mylite_result_warning_count(result), 0U, "delete warning count");
    mylite_result_free(result);

    return failures;
}

static int expect_delete_remaining(
    mylite_db *database,
    const char *table_name,
    int64_t affected_rows,
    const char *sql,
    const char *const *expected_ids,
    size_t expected_id_count,
    const char *context
) {
    int failures = expect_delete_ok(database, sql, affected_rows);

    failures += expect_table_ids(database, table_name, expected_ids, expected_id_count, context);

    return failures;
}

static int expect_table_ids(
    mylite_db *database,
    const char *table_name,
    const char *const *expected_ids,
    size_t expected_id_count,
    const char *context
) {
    char sql[sql_capacity];
    int written = snprintf(sql, sizeof(sql), "SELECT id FROM %s ORDER BY id", table_name);

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "select ids SQL is too long for %s\n", table_name);
        return 1;
    }

    return expect_query_values(
        database,
        (struct expected_query){
            .sql = sql,
            .values = expected_ids,
            .value_count = expected_id_count,
            .context = context,
        }
    );
}

static int expect_query_row_count(
    mylite_db *database,
    const char *sql,
    size_t expected_row_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures += expect_size(mylite_result_row_count(result), expected_row_count, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), 1U, query.context);
    failures += expect_size(mylite_result_row_count(result), query.value_count, query.context);
    for (size_t index = 0U; index < query.value_count; ++index) {
        failures += expect_result_value(result, index, 0U, query.values[index], query.context);
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
        "%s/mylite_delete_lifecycle_%d_%s.mylite",
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
