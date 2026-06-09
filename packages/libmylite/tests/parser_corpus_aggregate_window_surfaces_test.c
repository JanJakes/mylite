#include "parser_test_support.h"

static int test_aggregate_argument_placeholders(void);
static int test_group_concat_and_group_keys(void);
static int test_interval_window_frames(void);
static int test_json_statistical_aggregate_windows(void);
static int parse_ok(const char *sql);

int main(void) {
    int failures = 0;

    failures += test_aggregate_argument_placeholders();
    failures += test_group_concat_and_group_keys();
    failures += test_interval_window_frames();
    failures += test_json_statistical_aggregate_windows();

    return failures == 0 ? 0 : 1;
}

static int test_aggregate_argument_placeholders(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *count_function = NULL;
    const struct mylite_sql_ast_node *count_argument_list = NULL;
    const struct mylite_sql_ast_node *sum_function = NULL;
    const struct mylite_sql_ast_node *sum_argument = NULL;
    const struct mylite_sql_ast_node *window_function = NULL;
    int failures = 0;

    failures += parse_ok("SELECT COUNT(DISTINCT 1,NULL), MIN(7), MAX(7), AVG(2), "
                         "BIT_AND(192), BIT_OR(192), BIT_XOR(192) FROM numbers");
    failures += parse_ok("SELECT SUM(c/d), SUM(k+1), SUM(id+r00+r01) FROM numbers");
    failures +=
        parse_ok("SELECT sex, AVG(id), SUM(AVG(id)) OVER w FROM numbers GROUP BY sex WINDOW w AS ()"
        );
    failures += parse_ok("SELECT SUM(SUM(i)) OVER () FROM numbers GROUP BY j");

    failures += parser_test_parse_sql(
        "SELECT COUNT(DISTINCT 1,NULL) FROM numbers",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    count_function = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    count_argument_list = parser_test_child_at(count_function, 1U);
    failures += parser_test_expect_node(
        count_function,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "COUNT DISTINCT literal placeholder"
    );
    failures += parser_test_expect_node(
        count_argument_list,
        MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST,
        "COUNT DISTINCT literal argument list"
    );
    failures +=
        parser_test_expect_child_count(count_argument_list, 2U, "COUNT DISTINCT argument count");
    failures += parser_test_expect_literal(
        parser_test_child_at(count_argument_list, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "COUNT DISTINCT first literal"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(count_argument_list, 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "COUNT DISTINCT second literal"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT SUM(c/d) FROM numbers", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    sum_function = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    sum_argument = parser_test_child_at(sum_function, 0U);
    failures += parser_test_expect_node(
        sum_function,
        MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION,
        "SUM arithmetic function"
    );
    failures += parser_test_expect_node(
        sum_argument,
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "SUM arithmetic argument"
    );
    failures +=
        parser_test_expect_operator(sum_argument, MYLITE_SQL_AST_OPERATOR_DIVIDE, "SUM divide");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT SUM(1) OVER ()", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    window_function = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        window_function,
        MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION,
        "SUM literal window function"
    );
    failures += parser_test_expect_child_count(window_function, 2U, "SUM window child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(window_function, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "SUM literal window argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(window_function, 1U),
        MYLITE_SQL_AST_WINDOW_SPEC,
        "SUM literal window spec"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_group_concat_and_group_keys(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *function = NULL;
    const struct mylite_sql_ast_node *argument_list = NULL;
    const struct mylite_sql_ast_node *group_clause = NULL;
    int failures = 0;

    failures += parse_ok("SELECT GROUP_CONCAT(a,b ORDER BY c,d SEPARATOR '|') FROM numbers");
    failures += parse_ok("SELECT GROUP_CONCAT(DISTINCT a, b ORDER BY c, d) FROM numbers");
    failures +=
        parse_ok("SELECT GROUP_CONCAT(i,'foo') AS f1 FROM numbers GROUP BY 'a' WITH ROLLUP");
    failures += parse_ok("SELECT k, MIN(i), SUM(j), SUM(k) OVER (ROWS UNBOUNDED PRECEDING) "
                         "FROM numbers GROUP BY (k)");

    failures += parser_test_parse_sql(
        "SELECT GROUP_CONCAT(a,b ORDER BY c,d SEPARATOR '|') FROM numbers",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    function = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    argument_list = parser_test_child_at(function, 1U);
    failures += parser_test_expect_node(
        function,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "multi-argument GROUP_CONCAT placeholder"
    );
    failures += parser_test_expect_node(
        argument_list,
        MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST,
        "multi-argument GROUP_CONCAT arguments"
    );
    failures +=
        parser_test_expect_child_count(argument_list, 2U, "multi-argument GROUP_CONCAT count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(argument_list, 0U),
        "a",
        "first argument"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(argument_list, 1U),
        "b",
        "second argument"
    );
    failures += parser_test_expect_span_text(
        function,
        "GROUP_CONCAT(a,b ORDER BY c,d SEPARATOR '|')",
        "multi-argument GROUP_CONCAT span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT k, SUM(j) FROM numbers GROUP BY (k)",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    group_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_GROUP_BY_CLAUSE);
    failures += parser_test_expect_node(
        parser_test_child_at(group_clause, 0U),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized group key"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT GROUP_CONCAT(i,'foo') FROM numbers GROUP BY 'a'",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    group_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_GROUP_BY_CLAUSE);
    failures += parser_test_expect_literal(
        parser_test_child_at(group_clause, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "string literal group key"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_interval_window_frames(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures +=
        parse_ok("SELECT COUNT(*) OVER (ORDER BY dt RANGE INTERVAL 1 DAY PRECEDING) FROM numbers");
    failures += parse_ok(
        "SELECT COUNT(*) OVER (ORDER BY dt RANGE BETWEEN CURRENT ROW AND INTERVAL 1 DAY FOLLOWING) "
        "FROM numbers"
    );

    failures += parser_test_parse_sql(
        "SELECT COUNT(*) OVER (ORDER BY dt RANGE INTERVAL 1 DAY PRECEDING) FROM numbers",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "interval window frame SELECT"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_json_statistical_aggregate_windows(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *function = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parse_ok("SELECT JSON_ARRAYAGG(j) OVER () FROM numbers");
    failures +=
        parse_ok("SELECT JSON_OBJECTAGG(name, j) OVER w FROM numbers WINDOW w AS (ORDER BY id)");
    failures += parse_ok("SELECT STDDEV_SAMP(j) OVER (ORDER BY id ROWS CURRENT ROW), "
                         "VARIANCE(j) OVER (PARTITION BY k) FROM numbers");

    failures += parser_test_parse_sql(
        "SELECT JSON_ARRAYAGG(j) OVER () FROM numbers",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    function = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    arguments = parser_test_child_at(function, 1U);
    failures += parser_test_expect_node(
        function,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "JSON_ARRAYAGG window placeholder"
    );
    failures += parser_test_expect_child_count(function, 3U, "JSON_ARRAYAGG window child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(function, 0U),
        "JSON_ARRAYAGG",
        "JSON_ARRAYAGG function name"
    );
    failures += parser_test_expect_node(
        arguments,
        MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST,
        "JSON_ARRAYAGG argument list"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(function, 2U),
        MYLITE_SQL_AST_WINDOW_SPEC,
        "JSON_ARRAYAGG window spec"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT FOO(j) OVER () FROM numbers",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int parse_ok(const char *sql) {
    struct mylite_sql_parse_result result;
    int failures = parser_test_parse_sql(sql, MYLITE_SQL_PARSE_OK, &result);

    if (failures != 0) {
        (void)parser_test_expect_true(0, sql);
    }
    mylite_sql_parse_result_deinit(&result);
    return failures;
}
