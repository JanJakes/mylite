#include <mylite/mylite.h>

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
    path_suffix_capacity = 16,
    mysql_error_native_function_argument_count = 1582,
    mysql_error_bigint_out_of_range = 1690,
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
    literal_binary_cast_column = 8,
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
    size_t warning_count;
    const char *context;
};

static int test_no_source_and_dual_concat(void);
static int test_table_backed_concat(void);
static int test_table_backed_control_flow(void);
static int test_table_backed_signed_integer_arithmetic(void);
static int test_table_backed_wildcard_aggregates_and_constants(void);
static int test_table_backed_literal_expression_metadata(void);
static int test_concat_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_uint32(uint32_t actual, uint32_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_no_source_and_dual_concat();
    failures += test_table_backed_concat();
    failures += test_table_backed_control_flow();
    failures += test_table_backed_signed_integer_arithmetic();
    failures += test_table_backed_wildcard_aggregates_and_constants();
    failures += test_table_backed_literal_expression_metadata();
    failures += test_concat_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_and_dual_concat(void) {
    static const char *const columns_no_source[] = {
        "DATABASE()",
        "SCHEMA()",
        "CONCAT('test-', DATABASE())",
        "CONCAT('a', 'b')",
        "CONCAT('solo')",
        "CONCAT(NULL)",
        "CONCAT('a', NULL)",
        "CONCAT(1, 2, 3)",
        "CONCAT(TRUE, FALSE)",
        "@@warning_count",
    };
    static const char *const values_no_source[] = {
        "app",
        "app",
        "test-app",
        "ab",
        "solo",
        NULL,
        NULL,
        "123",
        "10",
        "0",
    };
    static const char *const columns_dual[] = {"CONCAT('du', 'al')", "xy", "CONCAT('z')"};
    static const char *const values_dual[] = {"dual", "xy", "z"};
    static const char *const column_row_count[] = {"ROW_COUNT()"};
    static const char *const value_negative_one[] = {"-1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATABASE(), SCHEMA(), CONCAT('test-', DATABASE()), CONCAT('a', 'b'), "
                   "CONCAT('solo'), CONCAT(NULL), CONCAT('a', NULL), CONCAT(1, 2, 3), "
                   "CONCAT(TRUE, FALSE), @@warning_count",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source concat",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONCAT('du', 'al'), CONCAT('x', 'y') AS xy, CONCAT('z') FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual concat",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .columns = column_row_count,
            .column_count = 1U,
            .values = value_negative_one,
            .row_count = 1U,
            .context = "row count after concat select",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_concat(void) {
    static const char *const columns_mixed[] = {"id", "label"};
    static const char *const values_mixed[] = {"1", "a-7", "2", "b--3", "3", "-0"};
    static const char *const columns_nulls[] = {"id", "merged"};
    static const char *const values_nulls[] = {"1", "[ax]", "2", NULL, "3", "[]"};
    static const char *const columns_typed[] = {"id", "mixed"};
    static const char *const values_typed[] = {
        "1",
        "12.30:2024-01-02:01:02:03:2024-01-02 03:04:05:2024-01-02 03:04:05:alpha",
        "2",
        NULL,
        "3",
        NULL,
    };
    static const char *const columns_one_argument[] = {"id", "CONCAT(v)", "CONCAT('x')"};
    static const char *const values_one_argument[] = {"1", "a", "x"};
    static const char *const columns_limited[] = {"CONCAT(v, ':', id)"};
    static const char *const values_limited[] = {":3", "b:2"};
    static const char *const values_multi_order[] = {":3", "a:1", "b:2"};
    static const char *const columns_order_ids[] = {"id"};
    static const char *const values_concat_order[] = {"2", "3", "1"};
    static const char *const values_nested_order[] = {"1", "3", "2"};
    static const char *const values_multi_row_scalar_order[] = {"3", "1", "2"};
    static const char *const columns_labels[] = {"CONCAT(v, '-', id)", "alias_name"};
    static const char *const values_labels[] = {"a-1", "xapp"};
    static const char *const columns_nested_functions[] = {
        "id",
        "mixed_case",
        "lower_concat",
        "length_concat",
    };
    static const char *const values_nested_functions[] = {
        "1",
        "a:X:5",
        "ax",
        "2",
        "2",
        NULL,
        NULL,
        NULL,
        "3",
        NULL,
        "",
        "0",
    };
    static const char *const columns_distinct_date_parts[] = {"year", "month"};
    static const char *const values_distinct_date_parts[] = {
        "2024",
        "2",
        "2024",
        "1",
        "2023",
        "12",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, v VARCHAR(20), n VARCHAR(20), i INT, "
        "d DECIMAL(6,2), dt DATE, tm TIME, dttm DATETIME, ts TIMESTAMP NULL, txt TEXT"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 'a', 'x', 7, 12.30, '2024-01-02', '01:02:03', "
        "'2024-01-02 03:04:05', '2024-01-02 03:04:05', 'alpha'), "
        "(2, 'b', NULL, -3, -4.50, NULL, NULL, NULL, NULL, 'beta'), "
        "(3, '', '', 0, 0.00, '2024-12-31', '00:00:00', "
        "'2024-12-31 23:59:58', '2024-12-31 23:59:58', NULL)",
        NULL
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, CONCAT(v, '-', i) AS label FROM t ORDER BY id",
            .columns = columns_mixed,
            .column_count = sizeof(columns_mixed) / sizeof(columns_mixed[0]),
            .values = values_mixed,
            .row_count = 3U,
            .context = "table concat projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, CONCAT('[', v, n, ']') AS merged FROM t ORDER BY id",
            .columns = columns_nulls,
            .column_count = sizeof(columns_nulls) / sizeof(columns_nulls[0]),
            .values = values_nulls,
            .row_count = 3U,
            .context = "table concat null propagation",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, CONCAT(d, ':', dt, ':', tm, ':', dttm, ':', ts, ':', txt) "
                   "AS mixed FROM t ORDER BY id",
            .columns = columns_typed,
            .column_count = sizeof(columns_typed) / sizeof(columns_typed[0]),
            .values = values_typed,
            .row_count = 3U,
            .context = "table concat typed values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, CONCAT(v), CONCAT('x') FROM t WHERE id = 1",
            .columns = columns_one_argument,
            .column_count = sizeof(columns_one_argument) / sizeof(columns_one_argument[0]),
            .values = values_one_argument,
            .row_count = 1U,
            .context = "table concat one argument",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONCAT(v, ':', id) FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table concat where order limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONCAT(v, ':', id) FROM t WHERE id >= 1 ORDER BY v, id DESC",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_multi_order,
            .row_count = 3U,
            .context = "table concat multi-key order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY CONCAT(v, n), id",
            .columns = columns_order_ids,
            .column_count = sizeof(columns_order_ids) / sizeof(columns_order_ids[0]),
            .values = values_concat_order,
            .row_count = 3U,
            .context = "table concat order expression",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY LOWER(CONCAT(v, n)) DESC, id",
            .columns = columns_order_ids,
            .column_count = sizeof(columns_order_ids) / sizeof(columns_order_ids[0]),
            .values = values_nested_order,
            .row_count = 3U,
            .context = "nested string order expression",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY IFNULL(n,'z'), CONCAT(v, id)",
            .columns = columns_order_ids,
            .column_count = sizeof(columns_order_ids) / sizeof(columns_order_ids[0]),
            .values = values_multi_row_scalar_order,
            .row_count = 3U,
            .context = "multiple row-scalar order expressions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONCAT(v, '-', id), CONCAT('x', DATABASE()) AS alias_name "
                   "FROM t WHERE id = 1",
            .columns = columns_labels,
            .column_count = sizeof(columns_labels) / sizeof(columns_labels[0]),
            .values = values_labels,
            .row_count = 1U,
            .context = "concat labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, CONCAT(LOWER(v), ':', UPPER(n), ':', LENGTH(txt)) "
                   "AS mixed_case, LOWER(CONCAT(v, n)) AS lower_concat, "
                   "LENGTH(CONCAT(v, n)) AS length_concat FROM t ORDER BY id",
            .columns = columns_nested_functions,
            .column_count = sizeof(columns_nested_functions) / sizeof(columns_nested_functions[0]),
            .values = values_nested_functions,
            .row_count = 3U,
            .context = "nested row-scalar string functions",
        }
    );
    failures += execute_ok(database, "SET sql_mode = ''", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE posts(id INT, post_date DATETIME, post_type VARCHAR(20), "
        "post_status VARCHAR(20))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO posts VALUES "
        "(1, '2024-01-10 10:00:00', 'foo', 'publish'), "
        "(2, '2024-01-20 10:00:00', 'foo', 'publish'), "
        "(3, '2024-02-01 10:00:00', 'foo', 'publish'), "
        "(4, '2023-12-31 10:00:00', 'foo', 'publish'), "
        "(5, '2024-03-01 10:00:00', 'foo', 'trash'), "
        "(6, '2024-02-02 10:00:00', 'bar', 'publish')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT YEAR(post_date) AS year, MONTH(post_date) AS month "
                   "FROM posts WHERE post_type = 'foo' AND post_status != 'auto-draft' "
                   "AND post_status != 'trash' ORDER BY post_date DESC",
            .columns = columns_distinct_date_parts,
            .column_count =
                sizeof(columns_distinct_date_parts) / sizeof(columns_distinct_date_parts[0]),
            .values = values_distinct_date_parts,
            .row_count = 3U,
            .context = "distinct row-scalar temporal parts",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_control_flow(void) {
    static const char *const columns_string[] = {
        "id",
        "IFNULL(v,'x')",
        "COALESCE(n,v,'z')",
        "NULLIF(v,n)",
        "ISNULL(v)",
        "IF(nn, v, n)",
    };
    static const char *const values_string[] = {
        "1",
        "a",
        "a",
        "a",
        "0",
        "a",
        "2",
        "x",
        "fallback",
        NULL,
        "1",
        "fallback",
        "3",
        "A",
        "a",
        NULL,
        "0",
        "A",
    };
    static const char *const columns_integer[] = {
        "id",
        "IFNULL(i,-1)",
        "COALESCE(i,nn,99)",
        "NULLIF(i,0)",
        "ISNULL(i)",
        "IF(i, 'yes', 'no')",
    };
    static const char *const values_integer[] = {
        "1",
        "7",
        "7",
        "7",
        "0",
        "yes",
        "2",
        "-1",
        "0",
        NULL,
        "1",
        "no",
        "3",
        "0",
        "0",
        NULL,
        "0",
        "no",
    };
    static const char *const columns_temporal[] = {
        "id",
        "IFNULL(d,'2000-01-01')",
        "COALESCE(dt,'2000-01-01 00:00:00')",
        "IFNULL(txt,'missing')",
    };
    static const char *const values_temporal[] = {
        "1",
        "2024-01-02",
        "2024-01-02 03:04:05",
        "alpha",
        "2",
        "2000-01-01",
        "2000-01-01 00:00:00",
        "missing",
        "3",
        "2024-12-31",
        "2024-12-31 23:59:58",
        "beta",
    };
    static const char *const columns_limited[] = {"id", "IFNULL(v,'x')", "ISNULL(n)"};
    static const char *const values_limited[] = {"3", "A", "0", "2", "x", "0"};
    static const char *const columns_order_ids[] = {"id"};
    static const char *const values_ifnull_order[] = {"2", "3", "1"};
    static const char *const values_case_order[] = {"1", "3", "2"};
    static const char *const columns_labels[] = {"IFNULL(v,'x')", "alias_name", "ISNULL(n)"};
    static const char *const values_labels[] = {"a", "a", "1"};
    static const char *const columns_qualified[] = {"id", "ifn"};
    static const char *const values_qualified[] = {"2", "x"};
    static const char *const columns_nested[] = {"id", "nested"};
    static const char *const values_nested[] = {"1", "a", "2", "fallback", "3", "a"};
    static const char *const columns_arithmetic[] = {
        "id",
        "ifnull_arith",
        "coalesce_arith",
        "nullif_arith",
        "if_condition_arith",
        "if_value_arith",
        "case_condition_arith",
    };
    static const char *const values_arithmetic[] = {
        "1",  "8",  "8", "8", "a", "yes", "nz", "2", "-1",  "10", NULL,
        NULL, "no", "z", "3", "1", "15",  NULL, "a", "yes", "nz",
    };
    static const char *const columns_comparisons[] = {"id", "cmp_gt", "cmp_lt"};
    static const char *const values_comparisons[] = {
        "1",
        "no",
        "string",
        "2",
        "no",
        "123",
        "3",
        "no",
        "123",
    };
    static const char *const columns_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_status[] = {"-1", "0"};
    static const char *const columns_string_truth[] = {"id", "truthy"};
    static const char *const values_string_truth[] = {"1", "true", "2", "false", "3", "true"};
    static const char *const columns_warning_status[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const values_string_truth_status[] = {"2", "-1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "control-flow", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, v VARCHAR(20), n VARCHAR(20), i INT, nn INT NOT NULL, "
        "d DATE, dt DATETIME, txt TEXT"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 'a', NULL, 7, 1, '2024-01-02', '2024-01-02 03:04:05', 'alpha'), "
        "(2, NULL, 'fallback', NULL, 0, NULL, NULL, NULL), "
        "(3, 'A', 'a', 0, 5, '2024-12-31', '2024-12-31 23:59:58', 'beta')",
        NULL
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, IFNULL(v,'x'), COALESCE(n,v,'z'), NULLIF(v,n), "
                   "ISNULL(v), IF(nn, v, n) FROM t ORDER BY id",
            .columns = columns_string,
            .column_count = sizeof(columns_string) / sizeof(columns_string[0]),
            .values = values_string,
            .row_count = 3U,
            .context = "table control-flow string projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, IFNULL(i,-1), COALESCE(i,nn,99), NULLIF(i,0), "
                   "ISNULL(i), IF(i, 'yes', 'no') FROM t ORDER BY id",
            .columns = columns_integer,
            .column_count = sizeof(columns_integer) / sizeof(columns_integer[0]),
            .values = values_integer,
            .row_count = 3U,
            .context = "table control-flow integer projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, IFNULL(d,'2000-01-01'), "
                   "COALESCE(dt,'2000-01-01 00:00:00'), IFNULL(txt,'missing') "
                   "FROM t ORDER BY id",
            .columns = columns_temporal,
            .column_count = sizeof(columns_temporal) / sizeof(columns_temporal[0]),
            .values = values_temporal,
            .row_count = 3U,
            .context = "table control-flow temporal projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, IFNULL(v,'x'), ISNULL(n) FROM t "
                   "WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table control-flow where order limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY IFNULL(i,-1), id",
            .columns = columns_order_ids,
            .column_count = sizeof(columns_order_ids) / sizeof(columns_order_ids[0]),
            .values = values_ifnull_order,
            .row_count = 3U,
            .context = "table IFNULL order expression",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY CASE WHEN i > 0 THEN 9 ELSE i END DESC, id",
            .columns = columns_order_ids,
            .column_count = sizeof(columns_order_ids) / sizeof(columns_order_ids[0]),
            .values = values_case_order,
            .row_count = 3U,
            .context = "table CASE order expression",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT IFNULL(v,'x'), COALESCE(n,v) AS alias_name, ISNULL(n) "
                   "FROM t WHERE id = 1",
            .columns = columns_labels,
            .column_count = sizeof(columns_labels) / sizeof(columns_labels[0]),
            .values = values_labels,
            .row_count = 1U,
            .context = "control-flow labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT x.id, IFNULL(x.v,'x') AS ifn FROM t AS x WHERE x.id = 2",
            .columns = columns_qualified,
            .column_count = sizeof(columns_qualified) / sizeof(columns_qualified[0]),
            .values = values_qualified,
            .row_count = 1U,
            .context = "control-flow qualified columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, IFNULL(NULLIF(v,n), COALESCE(n,'z')) AS nested "
                   "FROM t ORDER BY id",
            .columns = columns_nested,
            .column_count = sizeof(columns_nested) / sizeof(columns_nested[0]),
            .values = values_nested,
            .row_count = 3U,
            .context = "control-flow nested projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, IFNULL(i+1,-1) AS ifnull_arith, "
                   "COALESCE(NULLIF(i+1,1),nn+10,99) AS coalesce_arith, "
                   "NULLIF(i+1,1) AS nullif_arith, IF(nn-5,v,n) AS if_condition_arith, "
                   "IF(i+1,'yes','no') AS if_value_arith, "
                   "CASE WHEN i+1 THEN 'nz' ELSE 'z' END AS case_condition_arith "
                   "FROM t ORDER BY id",
            .columns = columns_arithmetic,
            .column_count = sizeof(columns_arithmetic) / sizeof(columns_arithmetic[0]),
            .values = values_arithmetic,
            .row_count = 3U,
            .context = "control-flow arithmetic projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, CASE WHEN id > 5 THEN 'yes' ELSE 'no' END AS cmp_gt, "
                   "CASE WHEN id < 2 THEN 'string' ELSE 123 END AS cmp_lt FROM t ORDER BY id",
            .columns = columns_comparisons,
            .column_count = sizeof(columns_comparisons) / sizeof(columns_comparisons[0]),
            .values = values_comparisons,
            .row_count = 3U,
            .context = "control-flow searched CASE integer comparisons",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_status,
            .column_count = sizeof(columns_status) / sizeof(columns_status[0]),
            .values = values_status,
            .row_count = 1U,
            .context = "control-flow status after select",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, IF(v+1,'true','false') AS truthy FROM t ORDER BY id",
            .columns = columns_string_truth,
            .column_count = sizeof(columns_string_truth) / sizeof(columns_string_truth[0]),
            .values = values_string_truth,
            .row_count = 3U,
            .warning_count = 2U,
            .context = "control-flow string arithmetic truth warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .columns = columns_warning_status,
            .column_count = sizeof(columns_warning_status) / sizeof(columns_warning_status[0]),
            .values = values_string_truth_status,
            .row_count = 1U,
            .context = "control-flow warning status after string arithmetic truth",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_signed_integer_arithmetic(void) {
    static const char *const columns_core[] = {
        "a+i",
        "a-i",
        "a*i",
        "a+5",
        "5+a",
        "(a+i)*2",
        "a+i*2",
        "n+1",
        "TRUE+a",
        "FALSE+a",
        "a+i+1",
        "a-i-1",
        "a+NULL",
        "NULL+a",
    };
    static const char *const values_core[] = {
        "5",  "-1",  "6",   "7", "7", "10", "8",  NULL, "3",  "2",  "6",  "-2",  NULL, NULL,
        "2",  "-12", "-35", "0", "0", "4",  "9",  "11", "-4", "-5", "3",  "-13", NULL, NULL,
        "-2", "2",   "0",   "5", "5", "-4", "-4", NULL, "1",  "0",  "-1", "1",   NULL, NULL,
    };
    static const char *const columns_families[] = {
        "ti+s",
        "tb+s",
        "s+mi",
        "mi+a",
        "i+(b-b)",
    };
    static const char *const values_families[] = {"4", "5", "7", "6", "3"};
    static const char *const columns_signed_literal[] = {"signed_literal", "signed_subtract"};
    static const char *const values_signed_literal[] = {"-3", "7"};
    static const char *const columns_qualified_table[] = {"t.a+t.i"};
    static const char *const columns_qualified_alias[] = {"x.a+x.i"};
    static const char *const values_qualified[] = {"5"};
    static const char *const columns_label[] = {"expr_alias", "(a+i)*2"};
    static const char *const values_label[] = {"5", "10"};
    static const char *const columns_envelope[] = {"limited"};
    static const char *const values_envelope[] = {"-2", "2"};
    static const char *const columns_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_status[] = {"-1", "0"};
    static const char *const columns_no_match[] = {"b+2"};
    static const char *const columns_unsigned_arithmetic[] = {"u+1"};
    static const char *const values_unsigned_arithmetic[] = {"5", "6", "7"};
    static const char *const columns_string_arithmetic[] = {"v+1"};
    static const char *const values_string_arithmetic[] = {"1", "1", "1"};
    static const char *const columns_division[] = {
        "quotient",
        "int_quotient",
        "remainder",
        "mod_function",
        "negative_int_quotient",
        "negative_remainder",
        "divide_zero",
        "int_divide_zero",
        "mod_zero",
        "mod_function_zero",
    };
    static const char *const values_division[] = {
        "3.5",
        "3",
        "1",
        "1",
        "-3",
        "-1",
        NULL,
        NULL,
        NULL,
        NULL,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "integer-arithmetic", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, ti TINYINT, tb TINYINT(1), s SMALLINT, mi MEDIUMINT, "
        "a INT, i INTEGER, b BIGINT, n INT NULL, u INT UNSIGNED, v VARCHAR(10)"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 1, 2, 3, 4, 2, 3, 9223372036854775806, NULL, 4, 'x'), "
        "(2, -1, 0, -3, 6, -5, 7, -9223372036854775807, 10, 5, 'y'), "
        "(3, 0, 1, 2, -4, 0, -2, 0, NULL, 6, 'z')",
        NULL
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT a+i, a-i, a*i, a+5, 5+a, (a+i)*2, a+i*2, n+1, TRUE+a, "
                   "FALSE+a, a+i+1, a-i-1, a+NULL, NULL+a FROM t ORDER BY id",
            .columns = columns_core,
            .column_count = sizeof(columns_core) / sizeof(columns_core[0]),
            .values = values_core,
            .row_count = 3U,
            .context = "table signed integer arithmetic projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ti+s, tb+s, s+mi, mi+a, i+(b-b) FROM t WHERE id = 1",
            .columns = columns_families,
            .column_count = sizeof(columns_families) / sizeof(columns_families[0]),
            .values = values_families,
            .row_count = 1U,
            .context = "signed integer family arithmetic projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT a+-5 AS signed_literal, a- -5 AS signed_subtract FROM t WHERE id = 1",
            .columns = columns_signed_literal,
            .column_count = sizeof(columns_signed_literal) / sizeof(columns_signed_literal[0]),
            .values = values_signed_literal,
            .row_count = 1U,
            .context = "signed integer literal arithmetic projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT t.a+t.i FROM t WHERE t.id = 1",
            .columns = columns_qualified_table,
            .column_count = sizeof(columns_qualified_table) / sizeof(columns_qualified_table[0]),
            .values = values_qualified,
            .row_count = 1U,
            .context = "table-qualified arithmetic projection operands",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT x.a+x.i FROM t AS x WHERE x.id = 1",
            .columns = columns_qualified_alias,
            .column_count = sizeof(columns_qualified_alias) / sizeof(columns_qualified_alias[0]),
            .values = values_qualified,
            .row_count = 1U,
            .context = "alias-qualified arithmetic projection operands",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT a+i AS expr_alias, (a+i)*2 FROM t WHERE id = 1",
            .columns = columns_label,
            .column_count = sizeof(columns_label) / sizeof(columns_label[0]),
            .values = values_label,
            .row_count = 1U,
            .context = "arithmetic projection labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT a+i AS limited FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_envelope,
            .column_count = sizeof(columns_envelope) / sizeof(columns_envelope[0]),
            .values = values_envelope,
            .row_count = 2U,
            .context = "arithmetic projection row envelope",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT b+2 FROM t WHERE id = 99",
            .columns = columns_no_match,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "arithmetic projection skips unmatched overflow",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_status,
            .column_count = sizeof(columns_status) / sizeof(columns_status[0]),
            .values = values_status,
            .row_count = 1U,
            .context = "arithmetic projection status after select",
        }
    );
    failures += execute_error(
        database,
        "SELECT b+2 FROM t WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range in scalar arithmetic expression",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u+1 FROM t ORDER BY id",
            .columns = columns_unsigned_arithmetic,
            .column_count =
                sizeof(columns_unsigned_arithmetic) / sizeof(columns_unsigned_arithmetic[0]),
            .values = values_unsigned_arithmetic,
            .row_count = 3U,
            .context = "unsigned integer arithmetic projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT v+1 FROM t ORDER BY id",
            .columns = columns_string_arithmetic,
            .column_count =
                sizeof(columns_string_arithmetic) / sizeof(columns_string_arithmetic[0]),
            .values = values_string_arithmetic,
            .row_count = 3U,
            .warning_count = 3U,
            .context = "string column integer arithmetic projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 7 / 2 AS quotient, 7 DIV 2 AS int_quotient, "
                   "7 % 2 AS remainder, MOD(7, 2) AS mod_function, "
                   "-7 DIV 2 AS negative_int_quotient, -7 % 2 AS negative_remainder, "
                   "7 / 0 AS divide_zero, 7 DIV 0 AS int_divide_zero, "
                   "7 % 0 AS mod_zero, MOD(7, 0) AS mod_function_zero FROM t WHERE id = 1",
            .columns = columns_division,
            .column_count = sizeof(columns_division) / sizeof(columns_division[0]),
            .values = values_division,
            .row_count = 1U,
            .warning_count = 4U,
            .context = "table arithmetic division and modulo projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT a + -i FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "arithmetic expression supports unary signs only on numeric literals",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_wildcard_aggregates_and_constants(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "wildcard-aggregates", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, label VARCHAR(20))", NULL);
    failures += execute_error(
        database,
        "SELECT *, COUNT(*) AS row_count, SUM(id) AS id_total, "
        "CAST(X'68656C6C6F' AS BINARY) AS payload, (SELECT 1) AS scalar_one, "
        "CASE WHEN id > 5 THEN 'yes' ELSE 'no' END AS cmp_gt, "
        "CASE WHEN id < 5 THEN 'string' ELSE 123 END AS cmp_lt FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUM(column) supports exactly one aggregate select item",
        }
    );
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_literal_expression_metadata(void) {
    static const char *const columns[] = {
        "true_value",
        "false_value",
        "one_value",
        "plus_value",
        "text_value",
        "joined_value",
        "year_value",
        "date_value",
        "binary_value",
        "char_value",
        "char_not_date",
        "fallback_value",
        "case_text",
        "case_mixed",
        "abs_value",
        "scalar_one",
    };
    static const char *const values[] = {
        "1",
        "0",
        "1",
        "2",
        "abc",
        "ab",
        "2025",
        "2025-01-01",
        "hello",
        "123",
        "AS DATE",
        "fallback",
        "no",
        "string",
        "7",
        "1",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "literal-expression-metadata", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1)", NULL);
    failures += execute_ok(
        database,
        "SELECT TRUE AS true_value, FALSE AS false_value, 1 AS one_value, "
        "(1+1) AS plus_value, 'abc' AS text_value, CONCAT('a','b') AS joined_value, "
        "YEAR('2025-01-01') AS year_value, CAST('2025-01-01' AS DATE) AS date_value, "
        "CAST(X'68656C6C6F' AS BINARY) AS binary_value, CAST('123' AS CHAR) AS char_value, "
        "CAST('AS DATE' AS CHAR) AS char_not_date, "
        "COALESCE(NULL,'fallback') AS fallback_value, "
        "CASE WHEN id > 5 THEN 'yes' ELSE 'no' END AS case_text, "
        "CASE WHEN id < 5 THEN 'string' ELSE 123 END AS case_mixed, "
        "ABS(-7) AS abs_value, (SELECT 1) AS scalar_one FROM t",
        &result
    );

    if (failures == 0) {
        static const enum mylite_result_column_type types[] = {
            MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            MYLITE_RESULT_COLUMN_TYPE_YEAR,
            MYLITE_RESULT_COLUMN_TYPE_DATE,
            MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
        };

        static const uint64_t display_lengths[] = {
            1U,
            1U,
            2U,
            3U,
            12U,
            8U,
            4U,
            10U,
            5U,
            12U,
            28U,
            32U,
            12U,
            24U,
            2U,
            2U,
        };

        failures += expect_size(
            mylite_result_column_count(result),
            sizeof(columns) / sizeof(columns[0]),
            "literal expression metadata column count"
        );
        failures += expect_size(
            mylite_result_row_count(result),
            1U,
            "literal expression metadata row count"
        );
        for (size_t column = 0U; column < sizeof(columns) / sizeof(columns[0]); ++column) {
            failures += expect_text(
                mylite_result_column_name(result, column),
                columns[column],
                "literal expression metadata column name"
            );
            failures += expect_result_value(
                result,
                0U,
                column,
                values[column],
                "literal expression metadata value"
            );
            failures += expect_int(
                (int)mylite_result_column_type(result, column),
                (int)types[column],
                "literal expression metadata type"
            );
            failures += expect_uint64(
                mylite_result_column_display_length(result, column),
                display_lengths[column],
                "literal expression metadata display length"
            );
        }
        failures += expect_uint32(
            mylite_result_column_flags(result, literal_binary_cast_column) &
                MYLITE_RESULT_COLUMN_FLAG_BINARY,
            MYLITE_RESULT_COLUMN_FLAG_BINARY,
            "literal expression binary CAST flags"
        );
    }

    mylite_result_free(result);
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_concat_diagnostics(void) {
    static const char *const arithmetic_columns[] = {"v+1"};
    static const char *const arithmetic_values[] = {"1"};
    static const char *const control_flow_columns[] = {"IFNULL(1 + 2, 3)"};
    static const char *const control_flow_values[] = {"3"};
    static const char *const nested_concat_columns[] = {"CONCAT(CONCAT('a', 'b'))"};
    static const char *const nested_concat_values[] = {"ab"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, v VARCHAR(20), d DECIMAL(6,2))", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 'a', 1.00)", NULL);
    failures += execute_error(
        database,
        "SELECT CONCAT()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'CONCAT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONCAT(v, missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONCAT(CONCAT('a', 'b')) FROM t",
            .columns = nested_concat_columns,
            .column_count = sizeof(nested_concat_columns) / sizeof(nested_concat_columns[0]),
            .values = nested_concat_values,
            .row_count = 1U,
            .context = "nested concat",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT v+1 FROM t",
            .columns = arithmetic_columns,
            .column_count = sizeof(arithmetic_columns) / sizeof(arithmetic_columns[0]),
            .values = arithmetic_values,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "string integer arithmetic projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT IFNULL(v, missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(v, 'yes', 'no') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "IF() row conditions support only integer descriptor columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT IFNULL(1 + 2, 3) FROM t",
            .columns = control_flow_columns,
            .column_count = sizeof(control_flow_columns) / sizeof(control_flow_columns[0]),
            .values = control_flow_values,
            .row_count = 1U,
            .context = "constant control-flow arithmetic projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT NULLIF(id, v) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "NULLIF() row projection does not support mixed string and numeric",
        }
    );
    failures += execute_error(
        database,
        "SELECT NULLIF(d, '1.0') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "row-scalar SELECT control-flow functions do not support DECIMAL "
                            "columns",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
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

static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
) {
    int failures = 0;

    if (make_test_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-row-scalar-%s-%d.mylite",
        name,
        current_process_id()
    );

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
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
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[value_index],
                expected.context
            );
        }
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

static int expect_uint32(uint32_t actual, uint32_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %u, got %u\n", context, expected, actual);
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "(null)" : expected,
                actual == NULL ? "(null)" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected %s, got %s\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing %s, got %s\n",
            context,
            needle == NULL ? "(null)" : needle,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}
