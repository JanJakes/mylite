#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
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
    schema_sql_capacity = 128,
    group_table_sql_capacity = 512,
    test_path_suffix_capacity = 16,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_table_does_not_exist = 1146,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static int test_group_concat_values_persistence_rename_and_drop(void);
static int test_group_concat_diagnostics(void);
static int test_independent_group_concat_handles(void);
static int seed_schema(mylite_db *database, const char *name);
static int create_group_concat_table(mylite_db *database, const char *table_name);
static int create_diagnostic_table(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_discard(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
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
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
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

    failures += test_group_concat_values_persistence_rename_and_drop();
    failures += test_group_concat_diagnostics();
    failures += test_independent_group_concat_handles();

    return failures == 0 ? 0 : 1;
}

static int test_group_concat_values_persistence_rename_and_drop(void) {
    static const char *const id_columns[] = {"GROUP_CONCAT(id ORDER BY id)"};
    static const char *const id_values[] = {"1,2,3,4,5,6"};
    static const char *const name_columns[] = {"names"};
    static const char *const pipe_values[] = {"alpha|beta|delta|echo"};
    static const char *const note_columns[] = {"GROUP_CONCAT(note ORDER BY id SEPARATOR ':')"};
    static const char *const note_values[] = {"A:B:D:E"};
    static const char *const asc_columns[] = {"GROUP_CONCAT(name ORDER BY sort_n ASC SEPARATOR ':')"
    };
    static const char *const asc_values[] = {"delta:beta:alpha:echo"};
    static const char *const desc_columns[] = {
        "GROUP_CONCAT(name ORDER BY sort_n DESC SEPARATOR ':')"
    };
    static const char *const desc_values[] = {"echo:alpha:beta:delta"};
    static const char *const empty_separator_columns[] = {
        "GROUP_CONCAT(name ORDER BY id SEPARATOR '')",
    };
    static const char *const empty_separator_values[] = {"alphabetadeltaecho"};
    static const char *const ifnull_columns[] = {
        "GROUP_CONCAT(IFNULL(name, '') ORDER BY id SEPARATOR ':')",
    };
    static const char *const ifnull_values[] = {"alpha:beta::delta:echo:"};
    static const char *const concat_columns[] = {
        "GROUP_CONCAT(CONCAT(name, note) ORDER BY id SEPARATOR '|')",
    };
    static const char *const concat_values[] = {"alphaA|betaB|deltaD|echoE"};
    static const char *const multi_arg_columns[] = {
        "GROUP_CONCAT(name, id ORDER BY id)",
    };
    static const char *const multi_arg_values[] = {"alpha1,beta2,delta4,echo5"};
    static const char *const multi_arg_separator_columns[] = {
        "GROUP_CONCAT(name, ':', id ORDER BY id SEPARATOR '|')",
    };
    static const char *const multi_arg_separator_values[] = {"alpha:1|beta:2|delta:4|echo:5"};
    static const char *const distinct_columns[] = {
        "GROUP_CONCAT(DISTINCT name ORDER BY sort_n)",
    };
    static const char *const distinct_values[] = {"alpha,beta,delta,echo"};
    static const char *const distinct_separator_columns[] = {"names"};
    static const char *const distinct_separator_values[] = {"alpha|beta|delta|echo"};
    static const char *const distinct_ifnull_columns[] = {
        "GROUP_CONCAT(DISTINCT IFNULL(name, '') ORDER BY sort_n SEPARATOR ':')",
    };
    static const char *const distinct_ifnull_values[] = {"alpha:beta:delta:echo:"};
    static const char *const null_values[] = {NULL};
    static const char *const grouped_columns[] = {
        "g",
        "GROUP_CONCAT(name ORDER BY id SEPARATOR ':')"
    };
    static const char *const grouped_values[] = {"1", "alpha:beta", "2", "delta:echo", "3", NULL};
    static const char *const grouped_ifnull_columns[] = {"g", "names"};
    static const char *const grouped_ifnull_values[] = {
        "1",
        "alpha:beta:",
        "2",
        "delta:echo",
        "3",
        "",
    };
    static const char *const grouped_having_columns[] = {"g", "names"};
    static const char *const grouped_having_values[] = {"2", "delta:echo"};
    static const char *const grouped_alias_having_not_null_values[] = {
        "1",
        "alpha:beta",
        "2",
        "delta:echo",
    };
    static const char *const grouped_alias_having_null_values[] = {"3", NULL};
    static const char *const grouped_expr_alias_having_not_null_values[] = {
        "1",
        "alphaA|betaB",
        "2",
        "deltaD|echoE",
    };
    static const char *const grouped_alias_order_values[] = {
        "3",
        NULL,
        "1",
        "alpha:beta",
        "2",
        "delta:echo",
    };
    static const char *const grouped_alias_desc_limit_values[] = {
        "2",
        "delta:echo",
        "1",
        "alpha:beta",
    };
    static const char *const grouped_expression_desc_values[] = {
        "2",
        "delta:echo",
        "1",
        "alpha:beta",
        "3",
        NULL,
    };
    static const char *const grouped_ifnull_alias_order_values[] = {
        "3",
        "",
        "1",
        "alpha:beta:",
        "2",
        "delta:echo",
    };
    static const char *const grouped_alias_case_order_values[] = {
        "3",
        NULL,
        "2",
        "alpha",
        "1",
        "Bravo",
    };
    static const char *const grouped_distinct_columns[] = {"g", "names"};
    static const char *const grouped_distinct_values[] = {
        "1",
        "alpha:beta",
        "2",
        "delta:echo",
    };
    static const char *const row_count_columns[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const row_count_values[] = {"-1", "0"};
    static const char *const all_empty_columns[] = {"GROUP_CONCAT(name ORDER BY id)"};
    static const char *const all_empty_values[] = {","};
    static const char *const all_empty_join_columns[] = {"empty_join"};
    static const char *const all_empty_join_values[] = {""};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
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
    failures += execute_discard(database, "USE app");
    failures += create_group_concat_table(database, "items");
    failures +=
        execute_discard(database, "CREATE TABLE empty_names (id INT NOT NULL, name VARCHAR(20))");
    failures +=
        execute_discard(database, "INSERT INTO empty_names VALUES (1, ''), (2, ''), (3, NULL)");
    failures += execute_discard(
        database,
        "CREATE TABLE case_names (g INT, id INT NOT NULL, name VARCHAR(20))"
    );
    failures += execute_discard(
        database,
        "INSERT INTO case_names VALUES (1, 1, 'Bravo'), (2, 1, 'alpha'), (3, 1, NULL)"
    );
    failures += execute_discard(
        database,
        "CREATE TABLE duplicate_names (g INT, id INT NOT NULL, name VARCHAR(20), "
        "sort_n INT NOT NULL)"
    );
    failures += execute_discard(
        database,
        "INSERT INTO duplicate_names VALUES "
        "(1, 1, 'alpha', 1), "
        "(1, 2, 'beta', 2), "
        "(1, 3, 'alpha', 1), "
        "(2, 4, 'delta', 3), "
        "(2, 5, 'echo', 4), "
        "(2, 6, 'delta', 3), "
        "(2, 7, NULL, 5)"
    );

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        catalog_generation_before_select = catalog->generation;
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        sqlite_generation_before_select = session->sqlite_schema_generation;
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(id ORDER BY id) FROM items",
            .columns = id_columns,
            .column_count = 1U,
            .values = id_values,
            .row_count = 1U,
            .context = "integer value default separator",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name ORDER BY id SEPARATOR '|') AS names FROM items",
            .columns = name_columns,
            .column_count = 1U,
            .values = pipe_values,
            .row_count = 1U,
            .context = "varchar value explicit separator",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name ORDER BY id SEPARATOR \"|\") AS names FROM items",
            .columns = name_columns,
            .column_count = 1U,
            .values = pipe_values,
            .row_count = 1U,
            .context = "varchar value double-quoted separator",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(i.name ORDER BY i.id SEPARATOR '|') AS names "
                   "FROM items AS i",
            .columns = name_columns,
            .column_count = 1U,
            .values = pipe_values,
            .row_count = 1U,
            .context = "source-qualified aggregate columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(note ORDER BY id SEPARATOR ':') FROM items",
            .columns = note_columns,
            .column_count = 1U,
            .values = note_values,
            .row_count = 1U,
            .context = "text value explicit separator",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name ORDER BY sort_n ASC SEPARATOR ':') FROM items",
            .columns = asc_columns,
            .column_count = 1U,
            .values = asc_values,
            .row_count = 1U,
            .context = "ascending aggregate-local order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name ORDER BY sort_n DESC SEPARATOR ':') FROM items",
            .columns = desc_columns,
            .column_count = 1U,
            .values = desc_values,
            .row_count = 1U,
            .context = "descending aggregate-local order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name ORDER BY id SEPARATOR '') FROM items",
            .columns = empty_separator_columns,
            .column_count = 1U,
            .values = empty_separator_values,
            .row_count = 1U,
            .context = "empty separator",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(IFNULL(name, '') ORDER BY id SEPARATOR ':') FROM items",
            .columns = ifnull_columns,
            .column_count = 1U,
            .values = ifnull_values,
            .row_count = 1U,
            .context = "ifnull row-scalar value expression",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(CONCAT(name, note) ORDER BY id SEPARATOR '|') FROM items",
            .columns = concat_columns,
            .column_count = 1U,
            .values = concat_values,
            .row_count = 1U,
            .context = "concat row-scalar value expression",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name, id ORDER BY id) FROM items",
            .columns = multi_arg_columns,
            .column_count = 1U,
            .values = multi_arg_values,
            .row_count = 1U,
            .context = "multi-argument row value expression",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name, ':', id ORDER BY id SEPARATOR '|') FROM items",
            .columns = multi_arg_separator_columns,
            .column_count = 1U,
            .values = multi_arg_separator_values,
            .row_count = 1U,
            .context = "multi-argument row value expression with explicit separator",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(DISTINCT name ORDER BY sort_n) FROM duplicate_names",
            .columns = distinct_columns,
            .column_count = 1U,
            .values = distinct_values,
            .row_count = 1U,
            .context = "distinct row value expression",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(DISTINCT name ORDER BY sort_n SEPARATOR '|') AS names "
                   "FROM duplicate_names",
            .columns = distinct_separator_columns,
            .column_count = 1U,
            .values = distinct_separator_values,
            .row_count = 1U,
            .context = "distinct row value expression with explicit separator",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(DISTINCT IFNULL(name, '') ORDER BY sort_n SEPARATOR ':') "
                   "FROM duplicate_names",
            .columns = distinct_ifnull_columns,
            .column_count = 1U,
            .values = distinct_ifnull_values,
            .row_count = 1U,
            .context = "distinct row-scalar value expression",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name ORDER BY id) FROM items WHERE g = 99",
            .columns = (const char *const[]){"GROUP_CONCAT(name ORDER BY id)"},
            .column_count = 1U,
            .values = null_values,
            .row_count = 1U,
            .context = "no matched rows returns null",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name ORDER BY id) FROM items WHERE g = 3",
            .columns = (const char *const[]){"GROUP_CONCAT(name ORDER BY id)"},
            .column_count = 1U,
            .values = null_values,
            .row_count = 1U,
            .context = "all null values return null",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name ORDER BY id) FROM empty_names",
            .columns = all_empty_columns,
            .column_count = 1U,
            .values = all_empty_values,
            .row_count = 1U,
            .context = "all empty string values preserve separators",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name ORDER BY id SEPARATOR '') AS empty_join "
                   "FROM empty_names",
            .columns = all_empty_join_columns,
            .column_count = 1U,
            .values = all_empty_join_values,
            .row_count = 1U,
            .context = "all empty string values with empty separator",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, GROUP_CONCAT(name ORDER BY id SEPARATOR ':') "
                   "FROM items GROUP BY g ORDER BY g",
            .columns = grouped_columns,
            .column_count = 2U,
            .values = grouped_values,
            .row_count = 3U,
            .context = "grouped group_concat",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, GROUP_CONCAT(IFNULL(name, '') ORDER BY id SEPARATOR ':') AS names "
                   "FROM items GROUP BY g ORDER BY g",
            .columns = grouped_ifnull_columns,
            .column_count = 2U,
            .values = grouped_ifnull_values,
            .row_count = 3U,
            .context = "grouped group_concat row-scalar value expression",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, GROUP_CONCAT(name ORDER BY id SEPARATOR ':') AS names "
                   "FROM items GROUP BY g HAVING g = 2",
            .columns = grouped_having_columns,
            .column_count = 2U,
            .values = grouped_having_values,
            .row_count = 1U,
            .context = "grouped group_concat with group-column having",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, GROUP_CONCAT(name ORDER BY id SEPARATOR ':') AS names "
                   "FROM items GROUP BY g HAVING names IS NOT NULL ORDER BY g",
            .columns = grouped_having_columns,
            .column_count = 2U,
            .values = grouped_alias_having_not_null_values,
            .row_count = 2U,
            .context = "grouped group_concat aggregate alias having is not null",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, GROUP_CONCAT(name ORDER BY id SEPARATOR ':') AS names "
                   "FROM items GROUP BY g HAVING names IS NULL ORDER BY g",
            .columns = grouped_having_columns,
            .column_count = 2U,
            .values = grouped_alias_having_null_values,
            .row_count = 1U,
            .context = "grouped group_concat aggregate alias having is null",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, GROUP_CONCAT(CONCAT(name, note) ORDER BY id SEPARATOR '|') AS names "
                   "FROM items GROUP BY g HAVING names IS NOT NULL ORDER BY g",
            .columns = grouped_having_columns,
            .column_count = 2U,
            .values = grouped_expr_alias_having_not_null_values,
            .row_count = 2U,
            .context = "grouped group_concat row-scalar aggregate alias having is not null",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, GROUP_CONCAT(name ORDER BY id SEPARATOR ':') AS names "
                   "FROM items GROUP BY g ORDER BY names",
            .columns = grouped_having_columns,
            .column_count = 2U,
            .values = grouped_alias_order_values,
            .row_count = 3U,
            .context = "grouped group_concat aggregate alias order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, GROUP_CONCAT(name ORDER BY id SEPARATOR ':') AS names "
                   "FROM items GROUP BY g "
                   "ORDER BY GROUP_CONCAT(name ORDER BY id SEPARATOR ':')",
            .columns = grouped_having_columns,
            .column_count = 2U,
            .values = grouped_alias_order_values,
            .row_count = 3U,
            .context = "grouped group_concat aggregate expression order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, GROUP_CONCAT(name ORDER BY id SEPARATOR ':') AS names "
                   "FROM items GROUP BY g "
                   "ORDER BY GROUP_CONCAT(name ORDER BY id SEPARATOR ':') DESC",
            .columns = grouped_having_columns,
            .column_count = 2U,
            .values = grouped_expression_desc_values,
            .row_count = 3U,
            .context = "grouped group_concat aggregate expression descending order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, GROUP_CONCAT(name ORDER BY id SEPARATOR ':') AS names "
                   "FROM items GROUP BY g ORDER BY names DESC LIMIT 2",
            .columns = grouped_having_columns,
            .column_count = 2U,
            .values = grouped_alias_desc_limit_values,
            .row_count = 2U,
            .context = "grouped group_concat aggregate alias descending limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, GROUP_CONCAT(IFNULL(name, '') ORDER BY id SEPARATOR ':') AS names "
                   "FROM items GROUP BY g ORDER BY names",
            .columns = grouped_ifnull_columns,
            .column_count = 2U,
            .values = grouped_ifnull_alias_order_values,
            .row_count = 3U,
            .context = "grouped group_concat row-scalar aggregate alias order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, GROUP_CONCAT(IFNULL(name, '') ORDER BY id SEPARATOR ':') AS names "
                   "FROM items GROUP BY g "
                   "ORDER BY GROUP_CONCAT(IFNULL(name, '') ORDER BY id SEPARATOR ':')",
            .columns = grouped_ifnull_columns,
            .column_count = 2U,
            .values = grouped_ifnull_alias_order_values,
            .row_count = 3U,
            .context = "grouped group_concat row-scalar aggregate expression order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, GROUP_CONCAT(name ORDER BY id) AS names "
                   "FROM case_names GROUP BY g ORDER BY names",
            .columns = grouped_having_columns,
            .column_count = 2U,
            .values = grouped_alias_case_order_values,
            .row_count = 3U,
            .context = "grouped group_concat aggregate alias collation order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, GROUP_CONCAT(DISTINCT name ORDER BY sort_n SEPARATOR ':') AS names "
                   "FROM duplicate_names GROUP BY g ORDER BY g",
            .columns = grouped_distinct_columns,
            .column_count = 2U,
            .values = grouped_distinct_values,
            .row_count = 2U,
            .context = "grouped distinct group_concat",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, GROUP_CONCAT(DISTINCT name ORDER BY sort_n SEPARATOR ':') AS names "
                   "FROM duplicate_names GROUP BY g "
                   "ORDER BY GROUP_CONCAT(DISTINCT name ORDER BY sort_n SEPARATOR ':')",
            .columns = grouped_distinct_columns,
            .column_count = 2U,
            .values = grouped_distinct_values,
            .row_count = 2U,
            .context = "grouped distinct group_concat aggregate expression order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, GROUP_CONCAT(name ORDER BY id SEPARATOR ':') AS names "
                   "FROM items GROUP BY g ORDER BY g LIMIT 1 OFFSET 1",
            .columns = grouped_having_columns,
            .column_count = 2U,
            .values = grouped_having_values,
            .row_count = 1U,
            .context = "grouped group_concat with limit offset",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = row_count_columns,
            .column_count = 2U,
            .values = row_count_values,
            .row_count = 1U,
            .context = "row count and warnings after group_concat",
        }
    );

    if (catalog != NULL) {
        failures += expect_int64(
            (int64_t)catalog->generation,
            (int64_t)catalog_generation_before_select,
            "catalog generation unchanged by group_concat"
        );
    }
    if (session != NULL) {
        failures += expect_int64(
            (int64_t)session->sqlite_schema_generation,
            (int64_t)sqlite_generation_before_select,
            "sqlite schema generation unchanged by group_concat"
        );
    }
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read file preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble unchanged"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values file");
    failures += execute_discard(database, "USE app");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name ORDER BY id SEPARATOR '|') AS names FROM items",
            .columns = name_columns,
            .column_count = 1U,
            .values = pipe_values,
            .row_count = 1U,
            .context = "group_concat persists after reopen",
        }
    );
    failures += execute_discard(database, "RENAME TABLE items TO renamed_items");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql =
                "SELECT GROUP_CONCAT(name ORDER BY id SEPARATOR '|') AS names FROM renamed_items",
            .columns = name_columns,
            .column_count = 1U,
            .values = pipe_values,
            .row_count = 1U,
            .context = "group_concat after table rename",
        }
    );
    failures += execute_discard(database, "DROP TABLE renamed_items");
    failures += execute_error(
        database,
        "SELECT GROUP_CONCAT(name ORDER BY id) FROM renamed_items",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_group_concat_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_error(
        database,
        "SELECT GROUP_CONCAT(id) FROM diag",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += seed_schema(database, "app");
    failures += create_diagnostic_table(database);
    failures += execute_error(
        database,
        "SELECT GROUP_CONCAT(id) FROM missing.diag",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );
    failures += execute_error(
        database,
        "SELECT GROUP_CONCAT(id) FROM app.missing",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "doesn't exist",
        }
    );

    failures += execute_discard(database, "USE app");
    failures += execute_error(
        database,
        "SELECT GROUP_CONCAT(missing) FROM diag",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column",
        }
    );
    failures += execute_error(
        database,
        "SELECT GROUP_CONCAT(name ORDER BY missing) FROM diag",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column",
        }
    );
    failures += execute_error(
        database,
        "SELECT GROUP_CONCAT(name ORDER BY nullable_order) FROM diag",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "only NOT NULL descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT GROUP_CONCAT(name ORDER BY sort_text) FROM diag",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GROUP_CONCAT ORDER BY supports only NOT NULL integer",
        }
    );
    failures += execute_ok(
        database,
        "SELECT GROUP_CONCAT(IFNULL(name, '') ORDER BY id) FROM diag",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "SELECT GROUP_CONCAT(name ORDER BY id, sort_n) FROM diag",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GROUP_CONCAT ORDER BY supports only one descriptor column",
        }
    );
    failures += execute_error(
        database,
        "SELECT GROUP_CONCAT(blob_col) FROM diag",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "integer and nonbinary string",
        }
    );
    failures += execute_error(
        database,
        "SELECT GROUP_CONCAT(id SEPARATOR NULL) FROM diag",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT GROUP_CONCAT (name ORDER BY id) FROM diag",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_discard(database, "SET SESSION sql_mode = 'IGNORE_SPACE'");
    failures += execute_ok(database, "SELECT GROUP_CONCAT (name ORDER BY id) FROM diag", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_discard(database, "SET SESSION sql_mode = ''");
    failures += execute_error(
        database,
        "SELECT g, GROUP_CONCAT(name ORDER BY id) AS names FROM diag "
        "GROUP BY g HAVING GROUP_CONCAT(name ORDER BY id) IS NOT NULL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "HAVING does not support GROUP_CONCAT",
        }
    );
    failures += execute_error(
        database,
        "SELECT g FROM diag GROUP BY g ORDER BY GROUP_CONCAT(name)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "does not support hidden GROUP_CONCAT() ORDER BY keys",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, GROUP_CONCAT(name ORDER BY id SEPARATOR ':') AS names FROM diag "
        "GROUP BY g ORDER BY GROUP_CONCAT(name ORDER BY id SEPARATOR '|')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "does not support hidden GROUP_CONCAT() ORDER BY keys",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, GROUP_CONCAT(name ORDER BY id SEPARATOR ':') AS names FROM diag "
        "GROUP BY g ORDER BY GROUP_CONCAT(name ORDER BY id DESC SEPARATOR ':')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "does not support hidden GROUP_CONCAT() ORDER BY keys",
        }
    );
    failures += execute_error(
        database,
        "SELECT a.g, GROUP_CONCAT(a.name ORDER BY a.id) FROM diag AS a "
        "JOIN diag AS b ON a.id = b.id GROUP BY a.g",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "does not yet support joined GROUP BY sources",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_group_concat_handles(void) {
    static const char *const columns[] = {"names"};
    static const char *const first_values[] = {"alpha|beta|delta|echo"};
    static const char *const second_values[] = {"theta|zeta"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
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
    failures += execute_discard(first, "USE app");
    failures += execute_discard(second, "USE app");
    failures += create_group_concat_table(first, "items");
    failures += execute_discard(
        second,
        "CREATE TABLE items (g INT, id INT NOT NULL, name VARCHAR(20), note TEXT, "
        "sort_n INT NOT NULL, nullable_order INT)"
    );
    failures += execute_discard(
        second,
        "INSERT INTO items (g, id, name, note, sort_n, nullable_order) "
        "VALUES (1, 2, 'zeta', 'Z', 2, NULL), (1, 1, 'theta', 'T', 1, 1)"
    );

    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name ORDER BY id SEPARATOR '|') AS names FROM items",
            .columns = columns,
            .column_count = 1U,
            .values = first_values,
            .row_count = 1U,
            .context = "first handle group_concat state",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name ORDER BY id SEPARATOR '|') AS names FROM items",
            .columns = columns,
            .column_count = 1U,
            .values = second_values,
            .row_count = 1U,
            .context = "second handle group_concat state",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int seed_schema(mylite_db *database, const char *name) {
    char sql[schema_sql_capacity];
    int written = snprintf(sql, sizeof(sql), "CREATE DATABASE %s", name);

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        return 1;
    }
    return execute_discard(database, sql);
}

static int create_group_concat_table(mylite_db *database, const char *table_name) {
    char sql[group_table_sql_capacity];
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s (g INT, id INT NOT NULL, name VARCHAR(20), note TEXT, "
        "sort_n INT NOT NULL, nullable_order INT)",
        table_name
    );

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        return 1;
    }
    if (execute_discard(database, sql) != 0) {
        return 1;
    }
    written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO %s (g, id, name, note, sort_n, nullable_order) VALUES "
        "(1, 2, 'beta', 'B', 0, NULL), "
        "(1, 1, 'alpha', 'A', 5, 1), "
        "(1, 3, NULL, NULL, 4, 2), "
        "(2, 4, 'delta', 'D', -1, 3), "
        "(2, 5, 'echo', 'E', 7, 4), "
        "(3, 6, NULL, NULL, 8, 5)",
        table_name
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        return 1;
    }
    return execute_discard(database, sql);
}

static int create_diagnostic_table(mylite_db *database) {
    int failures = 0;

    failures += execute_discard(database, "USE app");
    failures += execute_discard(
        database,
        "CREATE TABLE diag (g INT, id INT NOT NULL, name VARCHAR(20) NOT NULL, "
        "nullable_order INT, sort_text VARCHAR(20) NOT NULL, blob_col BINARY(2))"
    );
    failures += execute_discard(
        database,
        "INSERT INTO diag (g, id, name, nullable_order, sort_text, blob_col) "
        "VALUES (1, 1, 'alpha', NULL, 'a', X'6162'), (1, 2, 'beta', 2, 'b', X'6364')"
    );
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s/%s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result == NULL || *out_result == NULL) {
        fprintf(stderr, "%s: expected result object\n", sql);
        return 1;
    }
    return 0;
}

static int execute_discard(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (failures == 0) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    }
    for (size_t column = 0U; failures == 0 && column < query.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            query.columns[column],
            query.context
        );
    }
    for (size_t row = 0U; failures == 0 && row < query.row_count; ++row) {
        for (size_t column = 0U; failures == 0 && column < query.column_count; ++column) {
            size_t index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[index], query.context);
        }
    }
    if (failures == 0) {
        failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
        failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
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

    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_group_concat_%s_%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
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
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        (void)fclose(file);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    if (fclose(file) != 0) {
        return 1;
    }
    return read_count == size ? 0 : 1;
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL) {
        if (actual == expected) {
            return 0;
        }
        fprintf(
            stderr,
            "%s: expected %s, got %s\n",
            context,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", context, expected, actual);
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
            actual == NULL ? "NULL" : actual,
            needle == NULL ? "NULL" : needle
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
