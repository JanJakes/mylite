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
    sqlite_sql_capacity = 512,
    grouped_multi_aggregate_column_count = 6,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_column_ambiguous = 1052,
    mysql_error_unknown_column = 1054,
    mysql_error_not_group_by = 1055,
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

struct expected_grouped_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static int test_grouped_values_persistence_rename_and_drop(void);
static int test_grouped_diagnostics(void);
static int test_independent_grouped_handles(void);
static int seed_schema(mylite_db *database, const char *name);
static int create_grouped_table(mylite_db *database, const char *table_name);
static int create_empty_grouped_table(mylite_db *database, const char *table_name);
static int create_string_grouped_table(mylite_db *database, const char *table_name);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_grouped_query(mylite_db *database, struct expected_grouped_query query);
static int expect_row_count(mylite_db *database, const char *expected, const char *context);
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
static int drop_physical_table(sqlite3 *connection, const char *physical_name);
static int execute_sql(sqlite3 *connection, const char *sql);
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

    failures += test_grouped_values_persistence_rename_and_drop();
    failures += test_grouped_diagnostics();
    failures += test_independent_grouped_handles();

    return failures == 0 ? 0 : 1;
}

static int test_grouped_values_persistence_rename_and_drop(void) {
    static const char *const g_count_columns[] = {"g", "COUNT(*)"};
    static const char *const g_count_values[] = {NULL, "1", "1", "2", "2", "2"};
    static const char *const g_count_n_columns[] = {"g", "COUNT(n)"};
    static const char *const g_count_n_values[] = {NULL, "0", "1", "1", "2", "2"};
    static const char *const g_min_columns[] = {"g", "MIN(n)"};
    static const char *const g_min_values[] = {NULL, NULL, "1", "10", "2", "20"};
    static const char *const g_max_columns[] = {"g", "MAX(n)"};
    static const char *const g_max_values[] = {NULL, NULL, "1", "10", "2", "30"};
    static const char *const g_sum_columns[] = {"g", "SUM(n)"};
    static const char *const g_sum_values[] = {NULL, NULL, "1", "10", "2", "50"};
    static const char *const g_avg_columns[] = {"g", "AVG(n)"};
    static const char *const g_avg_values[] = {NULL, NULL, "1", "10.0000", "2", "25.0000"};
    static const char *const g_bit_and_columns[] = {"g", "BIT_AND(n)"};
    static const char *const g_bit_and_values[] = {
        NULL,
        "18446744073709551615",
        "1",
        "10",
        "2",
        "20",
    };
    static const char *const g_bit_or_columns[] = {"g", "BIT_OR(nn)"};
    static const char *const g_bit_or_values[] = {NULL, "5", "1", "7", "2", "9"};
    static const char *const g_bit_xor_columns[] = {"g", "BIT_XOR(nn)"};
    static const char *const g_bit_xor_values[] = {NULL, "5", "1", "1", "2", "1"};
    static const char *const multi_columns[] = {"g", "c", "cn", "s", "a", "bo"};
    static const char *const multi_values[] = {
        NULL,
        "1",
        "0",
        NULL,
        NULL,
        "5",
        "1",
        "2",
        "1",
        "10",
        "10.0000",
        "7",
        "2",
        "2",
        "2",
        "50",
        "25.0000",
        "9",
    };
    static const char *const multi_having_columns[] = {"g", "c", "s"};
    static const char *const multi_having_values[] = {"2", "2", "50"};
    static const char *const multi_order_values[] = {"2", "2", "50", "1", "2", "10"};
    static const char *const multi_order_tiebreak_values[] = {
        "2",
        "2",
        "50",
        "1",
        "2",
        "10",
        NULL,
        "1",
        NULL,
    };
    static const char *const min_max_alias_columns[] = {"g", "mn", "mx"};
    static const char *const min_alias_order_values[] = {
        NULL,
        NULL,
        NULL,
        "1",
        "10",
        "10",
        "2",
        "20",
        "30",
    };
    static const char *const max_alias_order_values[] = {
        "2",
        "20",
        "30",
        "1",
        "10",
        "10",
        NULL,
        NULL,
        NULL,
    };
    static const char *const where_columns[] = {"g", "COUNT(*)"};
    static const char *const where_values[] = {"1", "1", "2", "2"};
    static const char *const alias_columns[] = {"k", "s"};
    static const char *const alias_values[] = {"2", "50", "1", "10"};
    static const char *const offset_values[] = {"1", "2"};
    static const char *const having_count_columns[] = {"g", "c"};
    static const char *const having_count_values[] = {"1", "2", "2", "2"};
    static const char *const having_count_n_columns[] = {"g", "c"};
    static const char *const having_count_n_zero_values[] = {NULL, "0"};
    static const char *const having_duplicate_alias_columns[] = {"x", "x"};
    static const char *const having_descriptor_precedence_columns[] = {"g", "g"};
    static const char *const having_sum_columns[] = {"g", "s"};
    static const char *const having_sum_values[] = {"1", "10", "2", "50"};
    static const char *const having_sum_null_values[] = {NULL, NULL};
    static const char *const having_max_columns[] = {"g", "m"};
    static const char *const having_max_values[] = {"2", "30"};
    static const char *const having_min_values[] = {"1", "10"};
    static const char *const having_avg_values[] = {"2", "25.0000"};
    static const char *const having_group_null_values[] = {NULL, "1"};
    static const char *const having_group_alias_columns[] = {"k", "COUNT(*)"};
    static const char *const having_group_alias_values[] = {"1", "2"};
    static const char *const having_where_limit_values[] = {"2", "2"};
    static const char *const name_group_columns[] = {"name", "COUNT(*)", "SUM(n)"};
    static const char *const name_group_values[] = {
        NULL,
        "1",
        "10",
        "alice",
        "2",
        "50",
        "bob",
        "2",
        "5",
        "carol",
        "1",
        "7",
    };
    static const char *const name_count_columns[] = {"name", "COUNT(*)"};
    static const char *const name_count_values[] = {
        NULL,
        "1",
        "alice",
        "2",
        "bob",
        "2",
        "carol",
        "1",
    };
    static const char *const label_group_columns[] = {"label", "COUNT(*)", "SUM(n)"};
    static const char *const label_group_values[] = {
        NULL,
        "1",
        "10",
        "A",
        "2",
        "50",
        "B",
        "2",
        "5",
        "C",
        "1",
        "7",
    };
    static const char *const body_group_columns[] = {"body", "COUNT(*)", "SUM(n)"};
    static const char *const body_group_values[] = {
        NULL,
        "2",
        "17",
        "essay",
        "2",
        "50",
        "note",
        "2",
        "5",
    };
    static const char *const string_having_columns[] = {"k", "c"};
    static const char *const string_having_desc_limit_values[] = {"bob", "2"};
    static const char *const string_having_null_values[] = {NULL, "1"};
    static const char *const string_where_offset_values[] = {"alice", "2", "BOB", "1"};
    static const char *const string_selected_alias_columns[] = {"grouped_name", "c"};
    static const char *const string_selected_alias_values[] = {
        NULL,
        "1",
        "alice",
        "2",
        "bob",
        "2",
        "carol",
        "1",
    };
    static const char *const string_selected_alias_desc_limit_values[] = {
        "carol",
        "1",
        "bob",
        "2",
    };
    static const char *const integer_selected_alias_columns[] = {"grouped_n", "c"};
    static const char *const integer_selected_alias_values[] = {
        NULL,
        "2",
        "10",
        "1",
        "20",
        "1",
        "30",
        "1",
    };
    static const char *const multi_selected_alias_columns[] = {
        "grouped_name",
        "grouped_label",
        "c",
    };
    static const char *const multi_selected_alias_values[] = {
        NULL,
        NULL,
        "1",
        "alice",
        "A",
        "2",
        "bob",
        "B",
        "2",
        "carol",
        "C",
        "1",
    };
    static const char *const string_desc_values[] = {
        "carol",
        "1",
        "bob",
        "2",
        "alice",
        "2",
        NULL,
        "1",
    };
    static const char *const g_n_count_columns[] = {"g", "n", "COUNT(*)"};
    static const char *const g_n_not_null_values[] = {
        "1",
        "10",
        "1",
        "2",
        "20",
        "1",
        "2",
        "30",
        "1",
    };
    static const char *const g_n_null_values[] = {NULL, NULL, "1", "1", NULL, "1"};
    static const char *const qualified_multi_columns[] = {"k", "value", "c"};
    static const char *const qualified_multi_values[] = {"2", "30", "1", "2", "20", "1"};
    static const char *const name_label_columns[] = {"name", "label", "COUNT(*)", "SUM(n)"};
    static const char *const name_label_values[] = {
        NULL,
        NULL,
        "1",
        "10",
        "alice",
        "A",
        "2",
        "50",
        "bob",
        "B",
        "2",
        "5",
        "carol",
        "C",
        "1",
        "7",
    };
    static const char *const count_having_literal_columns[] = {"T"};
    static const char *const count_having_literal_values[] = {"T"};
    static const char *const count_having_name_columns[] = {"name"};
    static const char *const count_having_name_values[] = {"a"};
    static const char *const temporal_group_columns[] = {"year", "month", "posts"};
    static const char *const temporal_group_values[] = {
        "2024",
        "2",
        "1",
        "2024",
        "1",
        "2",
        "2023",
        "12",
        "1",
    };
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
    failures += create_empty_grouped_table(database, "empty_grouped_numbers");
    failures += create_grouped_table(database, "grouped_numbers");
    failures += create_string_grouped_table(database, "string_grouped");
    failures +=
        execute_ok(database, "CREATE TABLE no_group_having_names(name VARCHAR(20))", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO no_group_having_names VALUES ('a'), ('b'), ('b'), ('c'), ('c'), ('c')",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE grouped_posts("
        "ID INT, post_date DATETIME, post_type VARCHAR(20), post_status VARCHAR(20))",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO grouped_posts VALUES "
        "(1,'2024-01-10 12:00:00','post','publish'),"
        "(2,'2024-01-20 09:00:00','post','publish'),"
        "(3,'2024-02-05 12:00:00','post','publish'),"
        "(4,'2023-12-31 23:00:00','post','publish'),"
        "(5,'2024-02-07 12:00:00','page','publish')",
        &result
    );
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

    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(*) FROM grouped_numbers GROUP BY g ORDER BY g",
            .columns = g_count_columns,
            .column_count = 2U,
            .values = g_count_values,
            .row_count = 3U,
            .context = "count star grouped by nullable key",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = ''", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT YEAR(post_date) AS `year`, MONTH(post_date) AS `month`, "
                   "COUNT(ID) AS posts FROM grouped_posts WHERE post_type = 'post' "
                   "AND post_status = 'publish' GROUP BY YEAR(post_date), MONTH(post_date) "
                   "ORDER BY post_date DESC",
            .columns = temporal_group_columns,
            .column_count = 3U,
            .values = temporal_group_values,
            .row_count = 3U,
            .context = "temporal extract grouped archive query",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = DEFAULT", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT 'T' FROM grouped_numbers HAVING COUNT(*) > 1",
            .columns = count_having_literal_columns,
            .column_count = 1U,
            .values = count_having_literal_values,
            .row_count = 1U,
            .context = "aggregate having without group gates row-scalar projection",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT 'T' FROM grouped_numbers HAVING COUNT(*) > 100",
            .columns = count_having_literal_columns,
            .column_count = 1U,
            .values = count_having_literal_values,
            .row_count = 0U,
            .context = "aggregate having without group suppresses row-scalar projection",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT DISTINCT name FROM no_group_having_names HAVING COUNT(*) > 1",
            .columns = count_having_name_columns,
            .column_count = 1U,
            .values = count_having_name_values,
            .row_count = 1U,
            .context = "aggregate having without group keeps MySQL arbitrary descriptor row",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(n) FROM grouped_numbers GROUP BY g ORDER BY g",
            .columns = g_count_n_columns,
            .column_count = 2U,
            .values = g_count_n_values,
            .row_count = 3U,
            .context = "count nullable column grouped by nullable key",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, MIN(n) FROM grouped_numbers GROUP BY g ORDER BY g",
            .columns = g_min_columns,
            .column_count = 2U,
            .values = g_min_values,
            .row_count = 3U,
            .context = "min grouped nullable values",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, MAX(n) FROM grouped_numbers GROUP BY g ORDER BY g",
            .columns = g_max_columns,
            .column_count = 2U,
            .values = g_max_values,
            .row_count = 3U,
            .context = "max grouped nullable values",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, SUM(n) FROM grouped_numbers GROUP BY g ORDER BY g",
            .columns = g_sum_columns,
            .column_count = 2U,
            .values = g_sum_values,
            .row_count = 3U,
            .context = "sum grouped nullable values",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, AVG(n) FROM grouped_numbers GROUP BY g ORDER BY g",
            .columns = g_avg_columns,
            .column_count = 2U,
            .values = g_avg_values,
            .row_count = 3U,
            .context = "avg grouped nullable values",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, BIT_AND(n) FROM grouped_numbers GROUP BY g ORDER BY g",
            .columns = g_bit_and_columns,
            .column_count = 2U,
            .values = g_bit_and_values,
            .row_count = 3U,
            .context = "bit and grouped nullable values",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, BIT_OR(nn) FROM grouped_numbers GROUP BY g ORDER BY g",
            .columns = g_bit_or_columns,
            .column_count = 2U,
            .values = g_bit_or_values,
            .row_count = 3U,
            .context = "bit or grouped not-null values",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, BIT_XOR(nn) FROM grouped_numbers GROUP BY g ORDER BY g",
            .columns = g_bit_xor_columns,
            .column_count = 2U,
            .values = g_bit_xor_values,
            .row_count = 3U,
            .context = "bit xor grouped not-null values",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(*) AS c, COUNT(n) AS cn, SUM(n) AS s, AVG(n) AS a, "
                   "BIT_OR(nn) AS bo FROM grouped_numbers GROUP BY g ORDER BY g",
            .columns = multi_columns,
            .column_count = grouped_multi_aggregate_column_count,
            .values = multi_values,
            .row_count = 3U,
            .context = "multiple grouped aggregate results",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(*) AS c, SUM(n) AS s FROM grouped_numbers "
                   "GROUP BY g HAVING s >= 50 ORDER BY s DESC",
            .columns = multi_having_columns,
            .column_count = 3U,
            .values = multi_having_values,
            .row_count = 1U,
            .context = "multi aggregate having selected alias",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(*) AS c, SUM(n) AS s FROM grouped_numbers "
                   "GROUP BY g ORDER BY s DESC LIMIT 2",
            .columns = multi_having_columns,
            .column_count = 3U,
            .values = multi_order_values,
            .row_count = 2U,
            .context = "multi aggregate order by aggregate alias",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(*) AS c, SUM(n) AS s FROM grouped_numbers "
                   "GROUP BY g ORDER BY c DESC, g DESC",
            .columns = multi_having_columns,
            .column_count = 3U,
            .values = multi_order_tiebreak_values,
            .row_count = 3U,
            .context = "multi aggregate order by aggregate alias and grouped key",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, MIN(n) AS mn, MAX(n) AS mx FROM grouped_numbers "
                   "GROUP BY g ORDER BY mn ASC",
            .columns = min_max_alias_columns,
            .column_count = 3U,
            .values = min_alias_order_values,
            .row_count = 3U,
            .context = "multi aggregate order by min alias",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, MIN(n) AS mn, MAX(n) AS mx FROM grouped_numbers "
                   "GROUP BY g ORDER BY mx DESC",
            .columns = min_max_alias_columns,
            .column_count = 3U,
            .values = max_alias_order_values,
            .row_count = 3U,
            .context = "multi aggregate order by max alias",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(*) FROM grouped_numbers WHERE n IS NOT NULL "
                   "GROUP BY g ORDER BY g",
            .columns = where_columns,
            .column_count = 2U,
            .values = where_values,
            .row_count = 2U,
            .context = "where filter before grouping",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT t.g AS k, SUM(t.n) AS s FROM app.grouped_numbers AS t "
                   "GROUP BY t.g ORDER BY k DESC LIMIT 2",
            .columns = alias_columns,
            .column_count = 2U,
            .values = alias_values,
            .row_count = 2U,
            .context = "schema qualified aliased group order limit",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(*) AS c FROM grouped_numbers "
                   "GROUP BY g HAVING c > 1 ORDER BY g",
            .columns = having_count_columns,
            .column_count = 2U,
            .values = having_count_values,
            .row_count = 2U,
            .context = "having aggregate alias comparison",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g AS x, COUNT(*) AS x FROM grouped_numbers "
                   "GROUP BY g HAVING x > 1 ORDER BY g",
            .columns = having_duplicate_alias_columns,
            .column_count = 2U,
            .values = having_count_values,
            .row_count = 2U,
            .context = "having duplicate alias aggregate precedence",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(*) AS g FROM grouped_numbers "
                   "GROUP BY g HAVING g > 1",
            .columns = having_descriptor_precedence_columns,
            .column_count = 2U,
            .values = having_where_limit_values,
            .row_count = 1U,
            .context = "having descriptor name before aggregate alias",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(*) AS c FROM grouped_numbers "
                   "GROUP BY g HAVING c > TRUE ORDER BY g",
            .columns = having_count_columns,
            .column_count = 2U,
            .values = having_count_values,
            .row_count = 2U,
            .context = "having boolean literal comparison",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(n) AS c FROM grouped_numbers "
                   "GROUP BY g HAVING c = 0 ORDER BY g",
            .columns = having_count_n_columns,
            .column_count = 2U,
            .values = having_count_n_zero_values,
            .row_count = 1U,
            .context = "having count nullable zero comparison",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, SUM(n) AS s FROM grouped_numbers "
                   "GROUP BY g HAVING s IS NOT NULL ORDER BY g",
            .columns = having_sum_columns,
            .column_count = 2U,
            .values = having_sum_values,
            .row_count = 2U,
            .context = "having aggregate alias is not null",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, SUM(n) AS s FROM grouped_numbers "
                   "GROUP BY g HAVING s IS NULL ORDER BY g",
            .columns = having_sum_columns,
            .column_count = 2U,
            .values = having_sum_null_values,
            .row_count = 1U,
            .context = "having aggregate alias is null",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, MIN(n) FROM grouped_numbers "
                   "GROUP BY g HAVING MIN(n) < 20 ORDER BY g",
            .columns = g_min_columns,
            .column_count = 2U,
            .values = having_min_values,
            .row_count = 1U,
            .context = "having selected min expression comparison",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, MAX(n) AS m FROM grouped_numbers "
                   "GROUP BY g HAVING m >= +30 ORDER BY g",
            .columns = having_max_columns,
            .column_count = 2U,
            .values = having_max_values,
            .row_count = 1U,
            .context = "having max alias unary plus comparison",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, AVG(n) FROM grouped_numbers "
                   "GROUP BY g HAVING AVG(n) = 25 ORDER BY g",
            .columns = g_avg_columns,
            .column_count = 2U,
            .values = having_avg_values,
            .row_count = 1U,
            .context = "having selected avg expression comparison",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(*) FROM grouped_numbers GROUP BY g HAVING g IS NULL",
            .columns = g_count_columns,
            .column_count = 2U,
            .values = having_group_null_values,
            .row_count = 1U,
            .context = "having grouped column is null",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g AS k, COUNT(*) FROM grouped_numbers GROUP BY g HAVING k <=> 1",
            .columns = having_group_alias_columns,
            .column_count = 2U,
            .values = having_group_alias_values,
            .row_count = 1U,
            .context = "having grouped alias null-safe comparison",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(*) AS c FROM grouped_numbers WHERE id >= 2 "
                   "GROUP BY g HAVING c > 1 ORDER BY g DESC LIMIT 1",
            .columns = having_count_columns,
            .column_count = 2U,
            .values = having_where_limit_values,
            .row_count = 1U,
            .context = "having with where order and limit",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT t.g AS k, SUM(t.n) AS s FROM app.grouped_numbers AS t "
                   "GROUP BY t.g HAVING s IS NOT NULL ORDER BY k DESC",
            .columns = alias_columns,
            .column_count = 2U,
            .values = alias_values,
            .row_count = 2U,
            .context = "schema qualified aliased group having order",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(*) AS c FROM grouped_numbers "
                   "GROUP BY g HAVING c > 1 ORDER BY g LIMIT 0",
            .columns = having_count_columns,
            .column_count = 2U,
            .values = having_count_values,
            .row_count = 0U,
            .context = "having limit zero",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(*) FROM grouped_numbers GROUP BY g ORDER BY g LIMIT 0",
            .columns = g_count_columns,
            .column_count = 2U,
            .values = g_count_values,
            .row_count = 0U,
            .context = "grouped limit zero",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(*) FROM grouped_numbers GROUP BY g ORDER BY g "
                   "LIMIT 1 OFFSET 1",
            .columns = g_count_columns,
            .column_count = 2U,
            .values = offset_values,
            .row_count = 1U,
            .context = "grouped limit offset",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(*) FROM grouped_numbers WHERE id > 100 GROUP BY g",
            .columns = g_count_columns,
            .column_count = 2U,
            .values = g_count_values,
            .row_count = 0U,
            .context = "grouped no matching rows",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(*) FROM empty_grouped_numbers GROUP BY g",
            .columns = g_count_columns,
            .column_count = 2U,
            .values = g_count_values,
            .row_count = 0U,
            .context = "empty table grouped count",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT name, COUNT(*), SUM(n) FROM string_grouped GROUP BY name "
                   "ORDER BY name",
            .columns = name_group_columns,
            .column_count = 3U,
            .values = name_group_values,
            .row_count = 4U,
            .context = "varchar grouped case-insensitive key",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT label, COUNT(*), SUM(n) FROM string_grouped GROUP BY label "
                   "ORDER BY label",
            .columns = label_group_columns,
            .column_count = 3U,
            .values = label_group_values,
            .row_count = 4U,
            .context = "char grouped trimmed key",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT body, COUNT(*), SUM(n) FROM string_grouped GROUP BY body "
                   "ORDER BY body",
            .columns = body_group_columns,
            .column_count = 3U,
            .values = body_group_values,
            .row_count = 3U,
            .context = "text grouped case-insensitive key",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT name AS k, COUNT(*) AS c FROM string_grouped GROUP BY name "
                   "HAVING c > 1 ORDER BY k DESC LIMIT 1",
            .columns = string_having_columns,
            .column_count = 2U,
            .values = string_having_desc_limit_values,
            .row_count = 1U,
            .context = "string grouped aggregate having order limit",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT name AS grouped_name, COUNT(*) AS c FROM string_grouped "
                   "GROUP BY grouped_name ORDER BY grouped_name",
            .columns = string_selected_alias_columns,
            .column_count = 2U,
            .values = string_selected_alias_values,
            .row_count = 4U,
            .context = "string grouped by selected descriptor alias",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT name AS grouped_name, COUNT(*) AS c FROM string_grouped "
                   "GROUP BY grouped_name HAVING grouped_name IS NOT NULL "
                   "ORDER BY grouped_name DESC LIMIT 2",
            .columns = string_selected_alias_columns,
            .column_count = 2U,
            .values = string_selected_alias_desc_limit_values,
            .row_count = 2U,
            .context = "selected descriptor alias grouped having order limit",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT n AS grouped_n, COUNT(*) AS c FROM grouped_numbers "
                   "GROUP BY grouped_n ORDER BY grouped_n",
            .columns = integer_selected_alias_columns,
            .column_count = 2U,
            .values = integer_selected_alias_values,
            .row_count = 4U,
            .context = "integer grouped by selected descriptor alias",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT name AS grouped_name, label AS grouped_label, COUNT(*) AS c "
                   "FROM string_grouped GROUP BY grouped_name, grouped_label "
                   "ORDER BY grouped_name",
            .columns = multi_selected_alias_columns,
            .column_count = 3U,
            .values = multi_selected_alias_values,
            .row_count = 4U,
            .context = "multiple grouped selected descriptor aliases",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT name AS k, COUNT(*) AS c FROM string_grouped GROUP BY name "
                   "HAVING k IS NULL",
            .columns = string_having_columns,
            .column_count = 2U,
            .values = string_having_null_values,
            .row_count = 1U,
            .context = "string grouped alias having is null",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT name AS k, COUNT(*) AS c FROM string_grouped WHERE n IS NOT NULL "
                   "GROUP BY name HAVING c >= 1 ORDER BY k LIMIT 2 OFFSET 1",
            .columns = string_having_columns,
            .column_count = 2U,
            .values = string_where_offset_values,
            .row_count = 2U,
            .context = "string grouped where before grouping offset",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT name AS k, COUNT(*) AS c FROM string_grouped GROUP BY name "
                   "ORDER BY k DESC",
            .columns = string_having_columns,
            .column_count = 2U,
            .values = string_desc_values,
            .row_count = 4U,
            .context = "string grouped descending order",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT name AS k, COUNT(*) AS c FROM string_grouped GROUP BY name "
                   "ORDER BY k LIMIT 0",
            .columns = string_having_columns,
            .column_count = 2U,
            .values = string_having_columns,
            .row_count = 0U,
            .context = "string grouped limit zero",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, n, COUNT(*) FROM grouped_numbers WHERE n IS NOT NULL "
                   "GROUP BY g, n ORDER BY n",
            .columns = g_n_count_columns,
            .column_count = 3U,
            .values = g_n_not_null_values,
            .row_count = 3U,
            .context = "integer grouped by two descriptor keys",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, n, COUNT(*) FROM grouped_numbers GROUP BY g, n "
                   "HAVING n IS NULL ORDER BY g",
            .columns = g_n_count_columns,
            .column_count = 3U,
            .values = g_n_null_values,
            .row_count = 2U,
            .context = "multiple grouped keys with nullable second key having",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT t.g AS k, t.n AS value, COUNT(*) AS c "
                   "FROM app.grouped_numbers AS t WHERE t.n IS NOT NULL "
                   "GROUP BY t.g, t.n ORDER BY value DESC LIMIT 2",
            .columns = qualified_multi_columns,
            .column_count = 3U,
            .values = qualified_multi_values,
            .row_count = 2U,
            .context = "qualified multiple group keys ordered by second alias",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT name, label, COUNT(*), SUM(n) FROM string_grouped "
                   "GROUP BY name, label ORDER BY name",
            .columns = name_label_columns,
            .column_count = 4U,
            .values = name_label_values,
            .row_count = 4U,
            .context = "string grouped by two descriptor keys",
        }
    );
    failures += expect_row_count(database, "-1", "row count after grouped select");

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_int64(
            (int64_t)catalog->generation,
            (int64_t)catalog_generation_before_select,
            "grouped select catalog generation"
        );
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_int64(
            (int64_t)session->sqlite_schema_generation,
            (int64_t)sqlite_generation_before_select,
            "grouped select sqlite schema generation"
        );
    }
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "grouped select preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen grouped file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, SUM(n) FROM grouped_numbers GROUP BY g ORDER BY g",
            .columns = g_sum_columns,
            .column_count = 2U,
            .values = g_sum_values,
            .row_count = 3U,
            .context = "reopened grouped sum",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT name, COUNT(*), SUM(n) FROM string_grouped GROUP BY name "
                   "ORDER BY name",
            .columns = name_group_columns,
            .column_count = 3U,
            .values = name_group_values,
            .row_count = 4U,
            .context = "reopened string grouped sum",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, n, COUNT(*) FROM grouped_numbers WHERE n IS NOT NULL "
                   "GROUP BY g, n ORDER BY n",
            .columns = g_n_count_columns,
            .column_count = 3U,
            .values = g_n_not_null_values,
            .row_count = 3U,
            .context = "reopened multiple grouped keys",
        }
    );
    failures +=
        execute_ok(database, "RENAME TABLE grouped_numbers TO renamed_grouped_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "RENAME TABLE string_grouped TO renamed_string_grouped", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, COUNT(*) FROM renamed_grouped_numbers GROUP BY g ORDER BY g",
            .columns = g_count_columns,
            .column_count = 2U,
            .values = g_count_values,
            .row_count = 3U,
            .context = "renamed table grouped count",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT name, COUNT(*) FROM renamed_string_grouped GROUP BY name ORDER BY name",
            .columns = name_count_columns,
            .column_count = 2U,
            .values = name_count_values,
            .row_count = 4U,
            .context = "renamed table string grouped count",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, n, COUNT(*) FROM renamed_grouped_numbers WHERE n IS NOT NULL "
                   "GROUP BY g, n ORDER BY n",
            .columns = g_n_count_columns,
            .column_count = 3U,
            .values = g_n_not_null_values,
            .row_count = 3U,
            .context = "renamed table multiple grouped keys",
        }
    );
    failures += execute_ok(database, "DROP TABLE renamed_grouped_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "DROP TABLE renamed_string_grouped", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "SELECT g, COUNT(*) FROM renamed_grouped_numbers GROUP BY g",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_grouped_numbers' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT name, COUNT(*) FROM renamed_string_grouped GROUP BY name",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_string_grouped' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_grouped_diagnostics(void) {
    static const char *const unselected_group_key_columns[] = {"g", "n", "COUNT(*)"};
    static const char *const unselected_group_key_values[] = {
        "1",
        "10",
        "1",
        "2",
        "20",
        "1",
        "2",
        "30",
        "1",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    const struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_schema_descriptor app_schema = schema;
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
        "SELECT g, COUNT(*) FROM grouped_numbers GROUP BY g",
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
    failures += create_grouped_table(database, "grouped_numbers");
    failures += create_string_grouped_table(database, "string_grouped");
    failures += execute_ok(
        database,
        "CREATE TABLE unsupported_group_keys(id INT NOT NULL, d DECIMAL(4,1) NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SELECT g, COUNT(*) FROM missing_schema.grouped_numbers GROUP BY g",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, COUNT(*) FROM missing GROUP BY g",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, COUNT(*) FROM _mylite_reserved.grouped_numbers GROUP BY g",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, COUNT(*) FROM _mylite_reserved GROUP BY g",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "SELECT missing, COUNT(*) FROM grouped_numbers GROUP BY g",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, COUNT(*) FROM grouped_numbers GROUP BY missing",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'group statement'",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, COUNT(*) FROM grouped_numbers GROUP BY g ORDER BY missing",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, COUNT(*) FROM grouped_numbers GROUP BY g ORDER BY g, COUNT(*)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, COUNT(*) FROM grouped_numbers GROUP BY g ORDER BY g, n",
        (struct expected_sql_error){
            .code = mysql_error_not_group_by,
            .sqlstate = "42000",
            .message_part = "Expression #1 of ORDER BY clause is not in GROUP BY clause",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, COUNT(*) FROM grouped_numbers GROUP BY g HAVING missing > 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'having clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, COUNT(*) FROM grouped_numbers GROUP BY g HAVING id > 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'id' in 'having clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, SUM(n) FROM grouped_numbers GROUP BY g HAVING SUM(missing) > 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'having clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, COUNT(*) FROM grouped_numbers GROUP BY g HAVING SUM(n) > 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "HAVING supports only the selected aggregate result",
        }
    );
    failures += execute_error(
        database,
        "SELECT name, COUNT(*) FROM string_grouped GROUP BY name HAVING name > 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "HAVING supports only integer grouped columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, BIT_OR(nn) AS bits FROM grouped_numbers GROUP BY g HAVING bits > 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "HAVING does not support bitwise aggregate predicates",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, COUNT(*) FROM grouped_numbers GROUP BY g HAVING COUNT(*) + 1 > 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, COUNT(*) AS c FROM grouped_numbers "
        "GROUP BY g HAVING c > 9223372036854775808",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for 'aggregate' in HAVING",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, COUNT(*) FROM grouped_numbers GROUP BY g HAVING g < -2147483649",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for 'g' in HAVING",
        }
    );
    failures += execute_error(
        database,
        "SELECT n, COUNT(*) FROM grouped_numbers GROUP BY g",
        (struct expected_sql_error){
            .code = mysql_error_not_group_by,
            .sqlstate = "42000",
            .message_part = "Expression #1 of SELECT list is not in GROUP BY clause",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, n, COUNT(*) FROM grouped_numbers GROUP BY g",
        (struct expected_sql_error){
            .code = mysql_error_not_group_by,
            .sqlstate = "42000",
            .message_part = "Expression #2 of SELECT list is not in GROUP BY clause",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, missing, COUNT(*) FROM grouped_numbers GROUP BY g, missing",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, n, COUNT(*) FROM grouped_numbers GROUP BY g, missing",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'group statement'",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, n, COUNT(*) FROM grouped_numbers GROUP BY g, n ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_not_group_by,
            .sqlstate = "42000",
            .message_part = "Expression #1 of ORDER BY clause is not in GROUP BY clause",
        }
    );
    failures += execute_error(
        database,
        "SELECT g AS x, n AS x, COUNT(*) FROM grouped_numbers GROUP BY g, n HAVING x = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "HAVING supports only unique selected group aliases",
        }
    );
    failures += execute_error(
        database,
        "SELECT g AS x, n AS x, COUNT(*) FROM grouped_numbers GROUP BY g, n ORDER BY x",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GROUP BY supports ORDER BY only on unique selected group aliases",
        }
    );
    failures += execute_error(
        database,
        "SELECT g AS n, COUNT(*) FROM grouped_numbers GROUP BY n",
        (struct expected_sql_error){
            .code = mysql_error_not_group_by,
            .sqlstate = "42000",
            .message_part = "Expression #1 of SELECT list is not in GROUP BY clause",
        }
    );
    failures += execute_error(
        database,
        "SELECT g AS x, n AS x, COUNT(*) FROM grouped_numbers GROUP BY x",
        (struct expected_sql_error){
            .code = mysql_error_column_ambiguous,
            .sqlstate = "23000",
            .message_part = "Column 'x' in group statement is ambiguous",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*) AS x FROM grouped_numbers GROUP BY x",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GROUP BY supports selected descriptor-column aliases only",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*) AS x FROM grouped_numbers GROUP BY x, g",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GROUP BY supports selected descriptor-column aliases only",
        }
    );
    failures += execute_error(
        database,
        "SELECT n + 1 AS x, COUNT(*) FROM grouped_numbers GROUP BY x",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "GROUP BY supports selected descriptor group columns followed by aggregate results",
        }
    );
    failures += expect_grouped_query(
        database,
        (struct expected_grouped_query){
            .sql = "SELECT g, n, COUNT(*) FROM grouped_numbers WHERE n IS NOT NULL "
                   "GROUP BY g, n, nn ORDER BY n",
            .columns = unselected_group_key_columns,
            .column_count = 3U,
            .values = unselected_group_key_values,
            .row_count = 3U,
            .context = "unselected descriptor group key is accepted",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, g, n, nn, id, COUNT(*) FROM grouped_numbers GROUP BY id, g, n, nn, id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GROUP BY supports at most four descriptor group columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT d, COUNT(*) FROM unsupported_group_keys GROUP BY d",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "GROUP BY supports only integer and nonbinary string descriptor group columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT g, COUNT(*) FROM grouped_numbers GROUP BY g",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GROUP BY does not support SELECT DISTINCT",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, COUNT(DISTINCT n) FROM grouped_numbers GROUP BY g",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "GROUP BY supports selected descriptor group columns followed by aggregate results",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, GROUP_CONCAT(n), COUNT(*) FROM grouped_numbers GROUP BY g",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "GROUP_CONCAT(column) does not yet support multiple grouped aggregate results",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, AVG(n) AS a FROM grouped_numbers GROUP BY g ORDER BY a",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GROUP BY does not support ORDER BY on AVG aggregate aliases",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, BIT_OR(nn) AS bo FROM grouped_numbers GROUP BY g ORDER BY bo",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GROUP BY does not support ORDER BY on bitwise aggregate aliases",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, COUNT(*) FROM grouped_numbers GROUP BY 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &app_schema),
        MYLITE_OK,
        "read diagnostics schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(
            database,
            app_schema.schema_id,
            "grouped_numbers",
            &table
        ),
        MYLITE_OK,
        "read diagnostics table"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += drop_physical_table(sqlite, table.physical_name);
    }
    failures += execute_error(
        database,
        "SELECT g, COUNT(*) FROM grouped_numbers GROUP BY g",
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

static int test_independent_grouped_handles(void) {
    static const char *const first_columns[] = {"g", "SUM(n)"};
    static const char *const first_values[] = {NULL, NULL, "1", "10", "2", "50"};
    static const char *const first_having_values[] = {"1", "10", "2", "50"};
    static const char *const second_columns[] = {"g", "SUM(n)"};
    static const char *const second_values[] = {"9", "300"};
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
    failures += create_grouped_table(first, "grouped_numbers");
    failures += create_empty_grouped_table(second, "grouped_numbers");
    failures += execute_ok(
        second,
        "INSERT INTO grouped_numbers VALUES (1, 9, 100, 1), (2, 9, 200, 2)",
        &result
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_grouped_query(
        first,
        (struct expected_grouped_query){
            .sql = "SELECT g, SUM(n) FROM grouped_numbers GROUP BY g ORDER BY g",
            .columns = first_columns,
            .column_count = 2U,
            .values = first_values,
            .row_count = 3U,
            .context = "first handle grouped sum",
        }
    );
    failures += expect_grouped_query(
        first,
        (struct expected_grouped_query){
            .sql = "SELECT g, SUM(n) FROM grouped_numbers "
                   "GROUP BY g HAVING SUM(n) IS NOT NULL ORDER BY g",
            .columns = first_columns,
            .column_count = 2U,
            .values = first_having_values,
            .row_count = 2U,
            .context = "first handle grouped having sum",
        }
    );
    failures += expect_grouped_query(
        second,
        (struct expected_grouped_query){
            .sql = "SELECT g, SUM(n) FROM grouped_numbers GROUP BY g ORDER BY g",
            .columns = second_columns,
            .column_count = 2U,
            .values = second_values,
            .row_count = 1U,
            .context = "second handle grouped sum",
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

static int create_grouped_table(mylite_db *database, const char *table_name) {
    char sql[sqlite_sql_capacity];
    mylite_result *result = NULL;
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s (id INT NOT NULL, g INT NULL, n INT NULL, nn INT NOT NULL)",
        table_name
    );
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "create grouped table SQL is too long for %s\n", table_name);
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);
    result = NULL;

    written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO %s VALUES "
        "(1, NULL, NULL, 5), "
        "(2, 1, 10, 6), "
        "(3, 1, NULL, 7), "
        "(4, 2, 20, 8), "
        "(5, 2, 30, 9)",
        table_name
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "insert grouped table SQL is too long for %s\n", table_name);
        return failures + 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);

    return failures;
}

static int create_empty_grouped_table(mylite_db *database, const char *table_name) {
    char sql[sqlite_sql_capacity];
    mylite_result *result = NULL;
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s (id INT NOT NULL, g INT NULL, n INT NULL, nn INT NOT NULL)",
        table_name
    );
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "create empty grouped table SQL is too long for %s\n", table_name);
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);

    return failures;
}

static int create_string_grouped_table(mylite_db *database, const char *table_name) {
    char sql[sqlite_sql_capacity];
    mylite_result *result = NULL;
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s (id INT NOT NULL, name VARCHAR(20) NULL, label CHAR(5) NULL, "
        "body TEXT NULL, n INT NULL)",
        table_name
    );
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "create string grouped table SQL is too long for %s\n", table_name);
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);
    result = NULL;

    written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO %s VALUES "
        "(1, NULL, NULL, NULL, 10), "
        "(2, 'alice', 'A', 'essay', 20), "
        "(3, 'Alice', 'A   ', 'Essay', 30), "
        "(4, 'bob', 'B', 'note', NULL), "
        "(5, 'BOB', 'B    ', 'Note', 5), "
        "(6, 'carol', 'C', NULL, 7)",
        table_name
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "insert string grouped table SQL is too long for %s\n", table_name);
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
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int expect_grouped_query(mylite_db *database, struct expected_grouped_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    for (size_t column_index = 0U; column_index < query.column_count; ++column_index) {
        failures += expect_text(
            mylite_result_column_name(result, column_index),
            query.columns[column_index],
            query.context
        );
    }
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row_index = 0U; row_index < query.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < query.column_count; ++column_index) {
            size_t value_index = (row_index * query.column_count) + column_index;

            failures += expect_result_value(
                result,
                row_index,
                column_index,
                query.values[value_index],
                query.context
            );
        }
    }
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
        "%s/mylite_group_by_single_column_aggregate_%d_%s.mylite",
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
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
