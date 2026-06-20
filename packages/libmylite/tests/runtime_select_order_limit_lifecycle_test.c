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
    ordered_numbers_column_count = 10,
    ordered_numbers_nullable_column_index = 5,
    ordered_numbers_not_null_column_index = 6,
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

static int test_order_limit_success_persistence_rename_and_drop(void);
static int test_order_limit_diagnostics(void);
static int test_independent_order_limit_handles(void);
static int seed_schema(mylite_db *database, const char *name);
static int create_order_tables(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
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

    failures += test_order_limit_success_persistence_rename_and_drop();
    failures += test_order_limit_diagnostics();
    failures += test_independent_order_limit_handles();

    return failures == 0 ? 0 : 1;
}

static int test_order_limit_success_persistence_rename_and_drop(void) {
    static const char *const ordered_i[] = {"-2", "0", "1", "2147483647"};
    static const char *const desc_i[] = {"2147483647", "1", "0", "-2"};
    static const char *const ordered_n[] = {NULL, NULL, "9", "9"};
    static const char *const desc_n[] = {"9", "9", NULL, NULL};
    static const char *const all_desc_limit_two[] = {"9", "9"};
    static const char *const all_limit_three[] = {NULL, NULL, "9"};
    static const char *const distinct_n[] = {NULL, "9"};
    static const char *const distinct_n_desc[] = {"9", NULL};
    static const char *const distinct_i[] = {"-2", "0", "1", "2147483647"};
    static const char *const distinct_ii[] = {"-2147483648", "0", "2147483647"};
    static const char *const distinct_iu[] = {"0", "2", "8", "4294967295"};
    static const char *const distinct_b[] = {
        "-9223372036854775808",
        "3",
        "8",
        "9223372036854775807",
    };
    static const char *const distinct_bu[] = {"0", "4", "8", "9223372036854775807"};
    static const char *const distinct_bool[] = {NULL, "0", "1"};
    static const char *const distinct_nn[] = {"5", "6", "7", "8"};
    static const char *const distinct_limit_one[] = {NULL};
    static const char *const distinct_offset_one[] = {"9"};
    static const char *const ordered_iu[] = {"0", "2", "8", "4294967295"};
    static const char *const ordered_b[] = {
        "-9223372036854775808",
        "3",
        "8",
        "9223372036854775807",
    };
    static const char *const ordered_bu[] = {"0", "4", "8", "9223372036854775807"};
    static const char *const alias_first[] = {"1"};
    static const char *const alias_unsigned_last[] = {"2"};
    static const char *const limit_two[] = {"1", "2"};
    static const char *const all_ids[] = {"1", "2", "3", "4"};
    static const char *const offset_two[] = {"2", "3"};
    static const char *const single_three[] = {"3"};
    static const char *const multi_n_nn_desc[] = {"3", "1", "4", "2"};
    static const char *const multi_n_desc_id_desc[] = {"4", "2", "3", "1"};
    static const char *const multi_n_id_desc_limit[] = {"3", "1", "4"};
    static const char *const multi_bool_id[] = {"4", "2", "3", "1"};
    static const char *const ordinal_id_desc[] = {"4", "3", "2", "1"};
    static const char *const ordinal_distinct_n_desc[] = {"9", NULL};
    static const char *const row_scalar_ordinal_desc[] = {
        "4-qux",
        "3-food new",
        "2-bar",
        "1-foo old",
    };
    static const char *const like_title_nn_desc[] = {"3", "1", "4", "2"};
    static const char *const case_title_id[] = {"3", "1", "2", "4"};
    static const char *const case_logical_title_id[] = {"3", "1", "2", "4"};
    static const char *const multi_copy_ids[] = {"1", "3", "4"};
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
    failures += create_order_tables(database);

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        catalog_generation_before_select = catalog->generation;
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        sqlite_generation_before_select = session->sqlite_schema_generation;
    }

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i FROM ordered_numbers ORDER BY i",
            .values = ordered_i,
            .value_count = sizeof(ordered_i) / sizeof(ordered_i[0]),
            .context = "default ascending order",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i FROM ordered_numbers ORDER BY i ASC",
            .values = ordered_i,
            .value_count = sizeof(ordered_i) / sizeof(ordered_i[0]),
            .context = "explicit ascending order",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i FROM ordered_numbers ORDER BY i DESC",
            .values = desc_i,
            .value_count = sizeof(desc_i) / sizeof(desc_i[0]),
            .context = "descending order",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY NULL, id DESC",
            .values = ordinal_id_desc,
            .value_count = sizeof(ordinal_id_desc) / sizeof(ordinal_id_desc[0]),
            .context = "constant null order before descriptor order",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY 'a' DESC, id",
            .values = all_ids,
            .value_count = sizeof(all_ids) / sizeof(all_ids[0]),
            .context = "constant string order before descriptor order",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT n FROM ordered_numbers ORDER BY n",
            .values = ordered_n,
            .value_count = sizeof(ordered_n) / sizeof(ordered_n[0]),
            .context = "ascending null ordering",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ALL n FROM ordered_numbers ORDER BY n",
            .values = ordered_n,
            .value_count = sizeof(ordered_n) / sizeof(ordered_n[0]),
            .context = "all ascending null ordering",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT n FROM ordered_numbers ORDER BY n DESC",
            .values = desc_n,
            .value_count = sizeof(desc_n) / sizeof(desc_n[0]),
            .context = "descending null ordering",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ALL n FROM ordered_numbers WHERE n IS NOT NULL ORDER BY n DESC LIMIT 2",
            .values = all_desc_limit_two,
            .value_count = sizeof(all_desc_limit_two) / sizeof(all_desc_limit_two[0]),
            .context = "all where order limit",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT n FROM ordered_numbers AS nums ORDER BY n",
            .values = ordered_n,
            .value_count = sizeof(ordered_n) / sizeof(ordered_n[0]),
            .context = "aliased ascending null ordering",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT n FROM ordered_numbers nums WHERE n IS NOT NULL ORDER BY n DESC LIMIT 2",
            .values = all_desc_limit_two,
            .value_count = sizeof(all_desc_limit_two) / sizeof(all_desc_limit_two[0]),
            .context = "bare alias where order limit",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ALL n FROM ordered_numbers AS nums ORDER BY n LIMIT 3",
            .values = all_limit_three,
            .value_count = sizeof(all_limit_three) / sizeof(all_limit_three[0]),
            .context = "all aliased limit",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT n FROM ordered_numbers ORDER BY n",
            .values = distinct_n,
            .value_count = sizeof(distinct_n) / sizeof(distinct_n[0]),
            .context = "distinct nullable ascending",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT n FROM ordered_numbers AS nums ORDER BY n",
            .values = distinct_n,
            .value_count = sizeof(distinct_n) / sizeof(distinct_n[0]),
            .context = "distinct aliased nullable ascending",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCTROW n FROM ordered_numbers ORDER BY n",
            .values = distinct_n,
            .value_count = sizeof(distinct_n) / sizeof(distinct_n[0]),
            .context = "distinctrow nullable ascending",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCTROW n FROM ordered_numbers nums ORDER BY n",
            .values = distinct_n,
            .value_count = sizeof(distinct_n) / sizeof(distinct_n[0]),
            .context = "distinctrow aliased nullable ascending",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT n FROM ordered_numbers ORDER BY n DESC",
            .values = distinct_n_desc,
            .value_count = sizeof(distinct_n_desc) / sizeof(distinct_n_desc[0]),
            .context = "distinct nullable descending",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT i FROM ordered_numbers ORDER BY i",
            .values = distinct_i,
            .value_count = sizeof(distinct_i) / sizeof(distinct_i[0]),
            .context = "distinct int values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT ii FROM ordered_numbers ORDER BY ii",
            .values = distinct_ii,
            .value_count = sizeof(distinct_ii) / sizeof(distinct_ii[0]),
            .context = "distinct integer alias values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT iu FROM ordered_numbers ORDER BY iu",
            .values = ordered_iu,
            .value_count = sizeof(ordered_iu) / sizeof(ordered_iu[0]),
            .context = "unsigned int ordering",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT iu FROM ordered_numbers ORDER BY iu",
            .values = distinct_iu,
            .value_count = sizeof(distinct_iu) / sizeof(distinct_iu[0]),
            .context = "distinct unsigned int values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT b FROM ordered_numbers ORDER BY b",
            .values = ordered_b,
            .value_count = sizeof(ordered_b) / sizeof(ordered_b[0]),
            .context = "signed bigint ordering",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT b FROM ordered_numbers ORDER BY b",
            .values = distinct_b,
            .value_count = sizeof(distinct_b) / sizeof(distinct_b[0]),
            .context = "distinct signed bigint values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT bu FROM ordered_numbers ORDER BY bu",
            .values = ordered_bu,
            .value_count = sizeof(ordered_bu) / sizeof(ordered_bu[0]),
            .context = "bigint unsigned signed64 ordering",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT bu FROM ordered_numbers ORDER BY bu",
            .values = distinct_bu,
            .value_count = sizeof(distinct_bu) / sizeof(distinct_bu[0]),
            .context = "distinct unsigned bigint values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT bool_col FROM ordered_numbers ORDER BY bool_col",
            .values = distinct_bool,
            .value_count = sizeof(distinct_bool) / sizeof(distinct_bool[0]),
            .context = "distinct bool alias values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCTROW bool_col FROM ordered_numbers ORDER BY bool_col",
            .values = distinct_bool,
            .value_count = sizeof(distinct_bool) / sizeof(distinct_bool[0]),
            .context = "distinctrow bool alias values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT n FROM ordered_numbers WHERE n IS NULL ORDER BY n",
            .values = distinct_limit_one,
            .value_count = sizeof(distinct_limit_one) / sizeof(distinct_limit_one[0]),
            .context = "distinct where is null",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT n FROM ordered_numbers WHERE n IS NOT NULL ORDER BY n",
            .values = distinct_offset_one,
            .value_count = sizeof(distinct_offset_one) / sizeof(distinct_offset_one[0]),
            .context = "distinct where is not null",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT n FROM ordered_numbers WHERE n <=> 9 ORDER BY n",
            .values = distinct_offset_one,
            .value_count = sizeof(distinct_offset_one) / sizeof(distinct_offset_one[0]),
            .context = "distinct null-safe equal",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT n FROM ordered_numbers WHERE nn >= 6 ORDER BY n",
            .values = distinct_n,
            .value_count = sizeof(distinct_n) / sizeof(distinct_n[0]),
            .context = "distinct comparison predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT n FROM ordered_numbers ORDER BY n LIMIT 1",
            .values = distinct_limit_one,
            .value_count = sizeof(distinct_limit_one) / sizeof(distinct_limit_one[0]),
            .context = "distinct limit one",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT n FROM ordered_numbers ORDER BY n LIMIT 1 OFFSET 1",
            .values = distinct_offset_one,
            .value_count = sizeof(distinct_offset_one) / sizeof(distinct_offset_one[0]),
            .context = "distinct limit offset",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCTROW n FROM ordered_numbers WHERE n IS NOT NULL "
                   "ORDER BY n DESC LIMIT 1",
            .values = distinct_offset_one,
            .value_count = sizeof(distinct_offset_one) / sizeof(distinct_offset_one[0]),
            .context = "distinctrow where order limit",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT n FROM ordered_numbers ORDER BY n LIMIT 1, 1",
            .values = distinct_offset_one,
            .value_count = sizeof(distinct_offset_one) / sizeof(distinct_offset_one[0]),
            .context = "distinct limit comma offset",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT n FROM ordered_numbers ORDER BY n LIMIT 10",
            .values = distinct_n,
            .value_count = sizeof(distinct_n) / sizeof(distinct_n[0]),
            .context = "distinct limit larger than result",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM integer_aliases ORDER BY ii LIMIT 1",
            .values = alias_first,
            .value_count = sizeof(alias_first) / sizeof(alias_first[0]),
            .context = "integer alias ordering",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM integer_aliases ORDER BY integeru DESC LIMIT 1",
            .values = alias_unsigned_last,
            .value_count = sizeof(alias_unsigned_last) / sizeof(alias_unsigned_last[0]),
            .context = "integer unsigned alias ordering",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY n, nn DESC",
            .values = multi_n_nn_desc,
            .value_count = sizeof(multi_n_nn_desc) / sizeof(multi_n_nn_desc[0]),
            .context = "multi-key nullable and descending later key",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY n DESC, id DESC",
            .values = multi_n_desc_id_desc,
            .value_count = sizeof(multi_n_desc_id_desc) / sizeof(multi_n_desc_id_desc[0]),
            .context = "multi-key descending null ordering",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY n ASC, id DESC LIMIT 3",
            .values = multi_n_id_desc_limit,
            .value_count = sizeof(multi_n_id_desc_limit) / sizeof(multi_n_id_desc_limit[0]),
            .context = "multi-key order before limit",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY bool_col, id",
            .values = multi_bool_id,
            .value_count = sizeof(multi_bool_id) / sizeof(multi_bool_id[0]),
            .context = "multi-key boolean ordering",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY 1 DESC",
            .values = ordinal_id_desc,
            .value_count = sizeof(ordinal_id_desc) / sizeof(ordinal_id_desc[0]),
            .context = "ordinal descriptor ordering",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT n FROM ordered_numbers ORDER BY 1 DESC",
            .values = ordinal_distinct_n_desc,
            .value_count = sizeof(ordinal_distinct_n_desc) / sizeof(ordinal_distinct_n_desc[0]),
            .context = "distinct ordinal ordering",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONCAT(id, '-', title) FROM ordered_numbers ORDER BY 1 DESC",
            .values = row_scalar_ordinal_desc,
            .value_count = sizeof(row_scalar_ordinal_desc) / sizeof(row_scalar_ordinal_desc[0]),
            .context = "row-scalar ordinal ordering",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers "
                   "ORDER BY title LIKE '%foo%' DESC, nn DESC",
            .values = like_title_nn_desc,
            .value_count = sizeof(like_title_nn_desc) / sizeof(like_title_nn_desc[0]),
            .context = "like predicate order key",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers "
                   "ORDER BY CASE "
                   "WHEN title LIKE '%food%' THEN 1 "
                   "WHEN title LIKE '%foo%' THEN 2 "
                   "ELSE 3 END, id",
            .values = case_title_id,
            .value_count = sizeof(case_title_id) / sizeof(case_title_id[0]),
            .context = "searched CASE order key",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers "
                   "ORDER BY CASE "
                   "WHEN title LIKE '%food%' THEN 1 "
                   "WHEN title LIKE '%foo%' AND title LIKE '%old%' THEN 2 "
                   "WHEN title LIKE '%bar%' OR title LIKE '%qux%' THEN 3 "
                   "ELSE 4 END, id",
            .values = case_logical_title_id,
            .value_count = sizeof(case_logical_title_id) / sizeof(case_logical_title_id[0]),
            .context = "searched CASE logical order key",
        }
    );

    failures += execute_ok(database, "SELECT * FROM ordered_numbers ORDER BY nn LIMIT 1", &result);
    failures += expect_size(
        mylite_result_column_count(result),
        ordered_numbers_column_count,
        "ordered star column count"
    );
    failures += expect_size(mylite_result_row_count(result), 1U, "ordered star row count");
    failures += expect_text(mylite_result_column_name(result, 0U), "id", "ordered star id name");
    failures += expect_text(mylite_result_column_name(result, 1U), "i", "ordered star i name");
    failures += expect_text(
        mylite_result_column_name(result, ordered_numbers_not_null_column_index),
        "nn",
        "ordered star nn name"
    );
    failures += expect_result_value(result, 0U, 0U, "1", "ordered star id value");
    failures += expect_result_value(result, 0U, 1U, "-2", "ordered star i value");
    failures += expect_result_value(
        result,
        0U,
        ordered_numbers_nullable_column_index,
        NULL,
        "ordered star null value"
    );
    failures += expect_int64(mylite_result_affected_rows(result), 0, "ordered star affected rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "ordered star warnings");
    mylite_result_free(result);
    result = NULL;

    failures +=
        execute_ok(database, "SELECT ALL * FROM ordered_numbers ORDER BY nn LIMIT 1", &result);
    failures += expect_size(
        mylite_result_column_count(result),
        ordered_numbers_column_count,
        "all ordered star column count"
    );
    failures += expect_size(mylite_result_row_count(result), 1U, "all ordered star row count");
    failures +=
        expect_text(mylite_result_column_name(result, 0U), "id", "all ordered star id name");
    failures += expect_result_value(result, 0U, 0U, "1", "all ordered star id value");
    failures += expect_result_value(
        result,
        0U,
        ordered_numbers_nullable_column_index,
        NULL,
        "all ordered star null value"
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 0, "all ordered star affected rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "all ordered star warnings");
    mylite_result_free(result);
    result = NULL;

    failures +=
        execute_ok(database, "SELECT * FROM ordered_numbers AS nums ORDER BY nn LIMIT 1", &result);
    failures += expect_size(
        mylite_result_column_count(result),
        ordered_numbers_column_count,
        "aliased ordered star column count"
    );
    failures += expect_size(mylite_result_row_count(result), 1U, "aliased ordered star row count");
    failures +=
        expect_text(mylite_result_column_name(result, 0U), "id", "aliased ordered star id name");
    failures += expect_result_value(result, 0U, 0U, "1", "aliased ordered star id value");
    failures += expect_result_value(
        result,
        0U,
        ordered_numbers_nullable_column_index,
        NULL,
        "aliased ordered star null value"
    );
    failures +=
        expect_int64(mylite_result_affected_rows(result), 0, "aliased ordered star affected rows");
    failures +=
        expect_size(mylite_result_warning_count(result), 0U, "aliased ordered star warnings");
    mylite_result_free(result);
    result = NULL;

    failures +=
        execute_ok(database, "SELECT i, i FROM ordered_numbers ORDER BY nn LIMIT 1", &result);
    failures += expect_size(mylite_result_column_count(result), 2U, "duplicate ordered columns");
    failures += expect_text(mylite_result_column_name(result, 0U), "i", "duplicate ordered col 1");
    failures += expect_text(mylite_result_column_name(result, 1U), "i", "duplicate ordered col 2");
    failures += expect_result_value(result, 0U, 0U, "-2", "duplicate ordered value 1");
    failures += expect_result_value(result, 0U, 1U, "-2", "duplicate ordered value 2");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT id FROM ordered_numbers ORDER BY id LIMIT 0", &result);
    failures += expect_size(mylite_result_column_count(result), 1U, "limit zero column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "limit zero row count");
    failures += expect_int64(mylite_result_affected_rows(result), 0, "limit zero affected rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "limit zero warnings");
    mylite_result_free(result);
    result = NULL;

    failures +=
        execute_ok(database, "SELECT DISTINCT n FROM ordered_numbers ORDER BY n LIMIT 0", &result);
    failures +=
        expect_size(mylite_result_column_count(result), 1U, "distinct limit zero column count");
    failures += expect_text(mylite_result_column_name(result, 0U), "n", "distinct column name");
    failures += expect_size(mylite_result_row_count(result), 0U, "distinct limit zero row count");
    failures +=
        expect_int64(mylite_result_affected_rows(result), 0, "distinct limit zero affected rows");
    failures +=
        expect_size(mylite_result_warning_count(result), 0U, "distinct limit zero warnings");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT DISTINCT n FROM ordered_numbers ORDER BY n", &result);
    failures += expect_size(mylite_result_column_count(result), 1U, "distinct row count column");
    failures += expect_size(mylite_result_row_count(result), 2U, "distinct row count rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "distinct select warnings");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT ROW_COUNT()", &result);
    failures += expect_result_value(result, 0U, 0U, "-1", "distinct select following row count");
    mylite_result_free(result);
    result = NULL;

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY id LIMIT 2",
            .values = limit_two,
            .value_count = sizeof(limit_two) / sizeof(limit_two[0]),
            .context = "limit exact count",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY id LIMIT 10",
            .values = all_ids,
            .value_count = sizeof(all_ids) / sizeof(all_ids[0]),
            .context = "limit larger than result",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY id LIMIT 9223372036854775807",
            .values = all_ids,
            .value_count = sizeof(all_ids) / sizeof(all_ids[0]),
            .context = "maximum supported limit",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY id LIMIT 2 OFFSET 1",
            .values = offset_two,
            .value_count = sizeof(offset_two) / sizeof(offset_two[0]),
            .context = "limit offset form",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY id LIMIT 1, 2",
            .values = offset_two,
            .value_count = sizeof(offset_two) / sizeof(offset_two[0]),
            .context = "limit comma form",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY id LIMIT 2 OFFSET 10",
            .values = NULL,
            .value_count = 0U,
            .context = "limit offset beyond result",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY id LIMIT 1 OFFSET 9223372036854775807",
            .values = NULL,
            .value_count = 0U,
            .context = "maximum supported offset",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY id LIMIT 1, 0",
            .values = NULL,
            .value_count = 0U,
            .context = "limit zero count after offset",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers WHERE n IS NULL ORDER BY id DESC LIMIT 1",
            .values = single_three,
            .value_count = sizeof(single_three) / sizeof(single_three[0]),
            .context = "where order limit composition",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM app.ordered_numbers WHERE nn >= 6 ORDER BY i DESC LIMIT 1",
            .values = single_three,
            .value_count = sizeof(single_three) / sizeof(single_three[0]),
            .context = "schema-qualified ordered limited select",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM app.ordered_numbers AS nums WHERE nn >= 6 ORDER BY i DESC "
                   "LIMIT 1",
            .values = single_three,
            .value_count = sizeof(single_three) / sizeof(single_three[0]),
            .context = "schema-qualified aliased ordered limited select",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ALL n FROM app.ordered_numbers ORDER BY n LIMIT 3",
            .values = all_limit_three,
            .value_count = sizeof(all_limit_three) / sizeof(all_limit_three[0]),
            .context = "schema-qualified all select",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT n FROM app.ordered_numbers ORDER BY n",
            .values = distinct_n,
            .value_count = sizeof(distinct_n) / sizeof(distinct_n[0]),
            .context = "schema-qualified distinct select",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT n FROM app.ordered_numbers AS nums ORDER BY n",
            .values = distinct_n,
            .value_count = sizeof(distinct_n) / sizeof(distinct_n[0]),
            .context = "schema-qualified aliased distinct select",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCTROW n FROM app.ordered_numbers ORDER BY n",
            .values = distinct_n,
            .value_count = sizeof(distinct_n) / sizeof(distinct_n[0]),
            .context = "schema-qualified distinctrow select",
        }
    );

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_size(
            (size_t)catalog->generation,
            (size_t)catalog_generation_before_select,
            "ordered select leaves catalog generation"
        );
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_size(
            (size_t)session->sqlite_schema_generation,
            (size_t)sqlite_generation_before_select,
            "ordered select leaves SQLite schema generation"
        );
    }

    failures += execute_ok(
        database,
        "CREATE TABLE multi_order_copy AS "
        "SELECT id FROM ordered_numbers ORDER BY n ASC, id DESC LIMIT 3",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM multi_order_copy ORDER BY id",
            .values = multi_copy_ids,
            .value_count = sizeof(multi_copy_ids) / sizeof(multi_copy_ids[0]),
            .context = "ctas multi-key source order limit",
        }
    );
    failures += execute_ok(database, "CREATE TABLE multi_order_inserted (id INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO multi_order_inserted "
        "SELECT id FROM ordered_numbers ORDER BY n ASC, id DESC LIMIT 3",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM multi_order_inserted ORDER BY id",
            .values = multi_copy_ids,
            .value_count = sizeof(multi_copy_ids) / sizeof(multi_copy_ids[0]),
            .context = "insert-select multi-key source order limit",
        }
    );
    failures += execute_ok(database, "CREATE TABLE multi_order_replaced (id INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "REPLACE INTO multi_order_replaced "
        "SELECT id FROM ordered_numbers ORDER BY n ASC, id DESC LIMIT 3",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM multi_order_replaced ORDER BY id",
            .values = multi_copy_ids,
            .value_count = sizeof(multi_copy_ids) / sizeof(multi_copy_ids[0]),
            .context = "replace-select multi-key source order limit",
        }
    );

    failures +=
        execute_ok(database, "ALTER TABLE ordered_numbers ALTER COLUMN nn SET INVISIBLE", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT nn FROM ordered_numbers ORDER BY nn",
            .values = distinct_nn,
            .value_count = sizeof(distinct_nn) / sizeof(distinct_nn[0]),
            .context = "distinct explicit invisible column",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "ordered select preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY i DESC LIMIT 1",
            .values = single_three,
            .value_count = sizeof(single_three) / sizeof(single_three[0]),
            .context = "reopened ordered limited row",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ALL n FROM ordered_numbers ORDER BY n",
            .values = ordered_n,
            .value_count = sizeof(ordered_n) / sizeof(ordered_n[0]),
            .context = "reopened all values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT n FROM ordered_numbers AS nums ORDER BY n",
            .values = ordered_n,
            .value_count = sizeof(ordered_n) / sizeof(ordered_n[0]),
            .context = "reopened aliased values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT n FROM ordered_numbers ORDER BY n",
            .values = distinct_n,
            .value_count = sizeof(distinct_n) / sizeof(distinct_n[0]),
            .context = "reopened distinct values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCTROW n FROM ordered_numbers ORDER BY n",
            .values = distinct_n,
            .value_count = sizeof(distinct_n) / sizeof(distinct_n[0]),
            .context = "reopened distinctrow values",
        }
    );

    failures +=
        execute_ok(database, "RENAME TABLE ordered_numbers TO renamed_ordered_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers ORDER BY id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.ordered_numbers' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT n FROM ordered_numbers ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.ordered_numbers' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCTROW n FROM ordered_numbers ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.ordered_numbers' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT ALL n FROM ordered_numbers ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.ordered_numbers' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT n FROM ordered_numbers AS nums ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.ordered_numbers' doesn't exist",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM renamed_ordered_numbers ORDER BY i DESC LIMIT 1",
            .values = single_three,
            .value_count = sizeof(single_three) / sizeof(single_three[0]),
            .context = "renamed ordered limited row",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT n FROM renamed_ordered_numbers AS nums ORDER BY n",
            .values = ordered_n,
            .value_count = sizeof(ordered_n) / sizeof(ordered_n[0]),
            .context = "renamed aliased values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ALL n FROM renamed_ordered_numbers ORDER BY n",
            .values = ordered_n,
            .value_count = sizeof(ordered_n) / sizeof(ordered_n[0]),
            .context = "renamed all values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT n FROM renamed_ordered_numbers ORDER BY n",
            .values = distinct_n,
            .value_count = sizeof(distinct_n) / sizeof(distinct_n[0]),
            .context = "renamed distinct values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCTROW n FROM renamed_ordered_numbers ORDER BY n",
            .values = distinct_n,
            .value_count = sizeof(distinct_n) / sizeof(distinct_n[0]),
            .context = "renamed distinctrow values",
        }
    );

    failures += execute_ok(database, "DROP TABLE renamed_ordered_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "SELECT id FROM renamed_ordered_numbers ORDER BY id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_ordered_numbers' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT n FROM renamed_ordered_numbers ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_ordered_numbers' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCTROW n FROM renamed_ordered_numbers ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_ordered_numbers' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT ALL n FROM renamed_ordered_numbers ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_ordered_numbers' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT n FROM renamed_ordered_numbers AS nums ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_ordered_numbers' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_order_limit_diagnostics(void) {
    static const char *const alias_first[] = {"1"};
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
        "SELECT * FROM ordered_numbers ORDER BY id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "SELECT n FROM ordered_numbers AS nums ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT n FROM ordered_numbers ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "SELECT ALL n FROM ordered_numbers ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM missing_schema.ordered_numbers ORDER BY id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT n FROM missing_schema.ordered_numbers ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );
    failures += execute_error(
        database,
        "SELECT ALL n FROM missing_schema.ordered_numbers ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );
    failures += execute_error(
        database,
        "SELECT n FROM missing_schema.ordered_numbers AS nums ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM _mylite_reserved.ordered_numbers ORDER BY id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT n FROM _mylite_reserved.ordered_numbers ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name",
        }
    );
    failures += execute_error(
        database,
        "SELECT ALL n FROM _mylite_reserved.ordered_numbers ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name",
        }
    );
    failures += execute_error(
        database,
        "SELECT n FROM _mylite_reserved.ordered_numbers AS nums ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name",
        }
    );

    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_order_tables(database);

    failures += execute_error(
        database,
        "SELECT * FROM missing_table ORDER BY id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT n FROM missing_table ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT ALL n FROM missing_table ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT n FROM missing_table AS nums ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM _mylite_reserved ORDER BY id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT n FROM _mylite_reserved ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "SELECT ALL n FROM _mylite_reserved ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "SELECT n FROM _mylite_reserved AS nums ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "SELECT missing FROM ordered_numbers ORDER BY id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ALL missing FROM ordered_numbers ORDER BY id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT missing FROM ordered_numbers AS nums ORDER BY id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers WHERE missing = 1 ORDER BY id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ALL id FROM ordered_numbers WHERE missing = 1 ORDER BY id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers AS nums WHERE missing = 1 ORDER BY id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers ORDER BY missing LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers ORDER BY id, missing LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ALL id FROM ordered_numbers ORDER BY missing LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers AS nums ORDER BY missing LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT wrong.id FROM ordered_numbers AS nums ORDER BY id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'wrong.id' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers AS nums WHERE wrong.id = 1 ORDER BY id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'wrong.id' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers AS nums ORDER BY wrong.id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'wrong.id' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers ORDER BY wrong.id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'wrong.id' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT missing FROM ordered_numbers ORDER BY missing",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT n FROM ordered_numbers WHERE missing = 1 ORDER BY n",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT n FROM ordered_numbers ORDER BY missing",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT n FROM ordered_numbers ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT DISTINCT supports ORDER BY only on selected columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCTROW n FROM ordered_numbers ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT DISTINCT supports ORDER BY only on selected columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT n FROM ordered_numbers ORDER BY n, nn",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT DISTINCT supports ORDER BY only on selected columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers ORDER BY 0",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column '0' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers ORDER BY 2",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column '2' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONCAT(id, '-', title) FROM ordered_numbers ORDER BY 2",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column '2' in 'order clause'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT 1 FROM ordered_numbers",
            .values = alias_first,
            .value_count = sizeof(alias_first) / sizeof(alias_first[0]),
            .context = "constant row-scalar distinct",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT wrong.n FROM ordered_numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'wrong.n' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCTROW wrong.n FROM ordered_numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'wrong.n' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers ORDER BY id LIMIT 9223372036854775808",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "LIMIT literal is outside the supported range",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers ORDER BY id LIMIT 1 OFFSET 9223372036854775808",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "LIMIT literal is outside the supported range",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers ORDER BY id LIMIT +1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers ORDER BY id LIMIT -1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers ORDER BY id LIMIT 1.0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers ORDER BY id LIMIT '1'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers ORDER BY id LIMIT 0x1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers ORDER BY id LIMIT b'1'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers ORDER BY id LIMIT ?",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures +=
        execute_ok(database, "SELECT id FROM ordered_numbers ORDER BY id, nn LIMIT 1", &result);
    failures += expect_size(mylite_result_row_count(result), 1U, "multi-key limit row count");
    failures += expect_result_value(result, 0U, 0U, "1", "multi-key limit first row");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT id FROM ordered_numbers ORDER BY 1 LIMIT 1", &result);
    failures += expect_size(mylite_result_row_count(result), 1U, "ordinal limit row count");
    failures += expect_result_value(result, 0U, 0U, "1", "ordinal limit first row");
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY id + 1 LIMIT 1",
            .values = alias_first,
            .value_count = sizeof(alias_first) / sizeof(alias_first[0]),
            .context = "expression order by with limit",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id AS x FROM ordered_numbers ORDER BY x LIMIT 1",
            .values = alias_first,
            .value_count = sizeof(alias_first) / sizeof(alias_first[0]),
            .context = "select item alias order by",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers GROUP BY id ORDER BY id LIMIT 1",
            .values = alias_first,
            .value_count = sizeof(alias_first) / sizeof(alias_first[0]),
            .context = "grouped selected column with ordered limit",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers HAVING id > 0 ORDER BY id LIMIT 1",
            .values = alias_first,
            .value_count = sizeof(alias_first) / sizeof(alias_first[0]),
            .context = "HAVING with ordered limit",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers WHERE id IN "
        "(SELECT id FROM ordered_numbers ORDER BY id) ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "IN subqueries support only WHERE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY id LIMIT 1 FOR UPDATE",
            .values = alias_first,
            .value_count = sizeof(alias_first) / sizeof(alias_first[0]),
            .context = "order limit locking clause no-op",
        }
    );

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read diagnostics schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "ordered_numbers", &table),
        MYLITE_OK,
        "read diagnostics table"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += drop_physical_table(sqlite, table.physical_name);
    }
    failures += execute_error(
        database,
        "SELECT id FROM ordered_numbers ORDER BY id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite row operation failed",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT id FROM ordered_numbers ORDER BY id LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite row operation failed",
        }
    );
    failures += execute_error(
        database,
        "SELECT ALL id FROM ordered_numbers ORDER BY id LIMIT 1",
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

static int test_independent_order_limit_handles(void) {
    static const char *const first_expected[] = {"10"};
    static const char *const second_expected[] = {"20"};
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
    failures += create_order_tables(first);
    failures += create_order_tables(second);

    failures +=
        execute_ok(first, "INSERT INTO ordered_numbers (id, i, nn) VALUES (10, 10, 10)", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(second, "INSERT INTO ordered_numbers (id, i, nn) VALUES (20, 20, 20)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY id DESC LIMIT 1",
            .values = first_expected,
            .value_count = sizeof(first_expected) / sizeof(first_expected[0]),
            .context = "first ordered limited row",
        }
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT ALL id FROM ordered_numbers ORDER BY id DESC LIMIT 1",
            .values = first_expected,
            .value_count = sizeof(first_expected) / sizeof(first_expected[0]),
            .context = "first all ordered limited row",
        }
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers AS nums ORDER BY id DESC LIMIT 1",
            .values = first_expected,
            .value_count = sizeof(first_expected) / sizeof(first_expected[0]),
            .context = "first aliased ordered limited row",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers ORDER BY id DESC LIMIT 1",
            .values = second_expected,
            .value_count = sizeof(second_expected) / sizeof(second_expected[0]),
            .context = "second ordered limited row",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT ALL id FROM ordered_numbers ORDER BY id DESC LIMIT 1",
            .values = second_expected,
            .value_count = sizeof(second_expected) / sizeof(second_expected[0]),
            .context = "second all ordered limited row",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id FROM ordered_numbers AS nums ORDER BY id DESC LIMIT 1",
            .values = second_expected,
            .value_count = sizeof(second_expected) / sizeof(second_expected[0]),
            .context = "second aliased ordered limited row",
        }
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT DISTINCT id FROM ordered_numbers ORDER BY id DESC LIMIT 1",
            .values = first_expected,
            .value_count = sizeof(first_expected) / sizeof(first_expected[0]),
            .context = "first distinct ordered limited row",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT DISTINCT id FROM ordered_numbers ORDER BY id DESC LIMIT 1",
            .values = second_expected,
            .value_count = sizeof(second_expected) / sizeof(second_expected[0]),
            .context = "second distinct ordered limited row",
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

static int create_order_tables(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = execute_ok(
        database,
        "CREATE TABLE ordered_numbers ("
        "id INT NOT NULL, "
        "i INT, "
        "iu INTEGER UNSIGNED, "
        "b BIGINT, "
        "bu BIGINT UNSIGNED, "
        "n INT NULL, "
        "nn INT NOT NULL, "
        "ii INTEGER, "
        "bool_col BOOL, "
        "title VARCHAR(64))",
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
        "INSERT INTO ordered_numbers VALUES "
        "(1, -2, 0, -9223372036854775808, 0, NULL, 5, -2147483648, 1, 'foo old'), "
        "(2, 1, 2, 3, 4, 9, 6, 0, 0, 'bar'), "
        "(3, 2147483647, 4294967295, 9223372036854775807, 9223372036854775807, "
        "NULL, 7, 2147483647, 0, 'food new'), "
        "(4, 0, 8, 8, 8, 9, 8, 0, NULL, 'qux')",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO integer_aliases VALUES (1, -3, 4294967295, 7), (2, 5, 0, 8)",
        &result
    );
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
        "%s/mylite_select_order_limit_lifecycle_%d_%s.mylite",
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
