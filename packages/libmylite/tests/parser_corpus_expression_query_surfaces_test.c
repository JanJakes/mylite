#include "parser_test_support.h"

static int test_unary_binary_and_regexp(void);
static int test_keyword_function_placeholders(void);
static int test_group_and_order_keys(void);
static int test_distinct_aggregate_placeholders(void);
static int parse_ok(const char *sql);

int main(void) {
    int failures = 0;

    failures += test_unary_binary_and_regexp();
    failures += test_keyword_function_placeholders();
    failures += test_group_and_order_keys();
    failures += test_distinct_aggregate_placeholders();

    return failures == 0 ? 0 : 1;
}

static int test_unary_binary_and_regexp(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    int failures = 0;

    failures += parse_ok("SELECT BINARY name FROM t");
    failures += parse_ok("SELECT CAST(BINARY 'a' AS CHAR)");
    failures += parse_ok("SELECT 'abc' REGEXP 'b', 'abc' NOT RLIKE BINARY 'B'");
    failures += parse_ok("SELECT CASE BINARY 'b' WHEN 'b' THEN 1 ELSE 0 END");

    failures += parser_test_parse_sql("SELECT BINARY name FROM t", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_CAST_BINARY_EXPRESSION,
        "unary BINARY expression"
    );
    failures += parser_test_expect_span_text(expression, "BINARY name", "unary BINARY span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT 'abc' REGEXP 'b'", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_BINARY_EXPRESSION, "REGEXP expression");
    failures += parser_test_expect_true(
        mylite_sql_ast_node_operator(expression) == MYLITE_SQL_AST_OPERATOR_REGEXP,
        "REGEXP operator"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_keyword_function_placeholders(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    int failures = 0;

    failures += parse_ok("SELECT ROW(1, 2), VALUES(id), GROUPING(id), POINT(1, 1) FROM t");
    failures += parse_ok("SELECT ROW(1, 2) = ROW(1, 2), ROW(1, 2) <> ROW(1, 3)");
    failures += parse_ok("SELECT (1, 2) = (1, 2), (1, NULL) <=> (1, NULL)");
    failures += parse_ok("SELECT NOT ROW(1, 2) = ROW(1, 3)");
    failures += parse_ok("SELECT GEOMETRYCOLLECTION(POINT(1,1)) FROM t");
    failures += parse_ok("SELECT CHAR(0x41 USING ucs2)");
    failures +=
        parse_ok("INSERT INTO t VALUES (1, 2) ON DUPLICATE KEY UPDATE n = GREATEST(n, VALUES(n))");
    failures += parser_test_parse_sql("SELECT ROW(1)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT GROUPING(id) FROM t", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "GROUPING placeholder"
    );
    failures += parser_test_expect_span_text(expression, "GROUPING(id)", "GROUPING span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ROW(1, 2)", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_ROW_CONSTRUCTOR, "ROW constructor");
    failures += parser_test_expect_span_text(expression, "ROW(1, 2)", "ROW constructor span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT (1, 2)", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_ROW_CONSTRUCTOR,
        "parenthesized row constructor"
    );
    failures +=
        parser_test_expect_span_text(expression, "(1, 2)", "parenthesized row constructor span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT CHAR(0x41 USING ucs2)", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_CHAR_FUNCTION, "CHAR USING");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_group_and_order_keys(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *group_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    int failures = 0;

    failures += parse_ok("SELECT grp, COUNT(*) FROM t GROUP BY 1");
    failures +=
        parse_ok("SELECT c, COUNT(*) FROM t GROUP BY c COLLATE utf8mb4_0900_ai_ci ORDER BY BINARY c"
        );
    failures += parse_ok("SELECT c, SUM(n) FROM t GROUP BY c ORDER BY SUM(n) DESC");
    failures += parse_ok("SELECT c, COUNT(*) FROM t GROUP BY BINARY c WITH ROLLUP");

    failures += parser_test_parse_sql(
        "SELECT c, COUNT(*) FROM t GROUP BY c COLLATE utf8mb4_0900_ai_ci ORDER BY BINARY c",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    group_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_GROUP_BY_CLAUSE);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    failures += parser_test_expect_node(
        parser_test_child_at(group_clause, 0U),
        MYLITE_SQL_AST_COLLATE_EXPRESSION,
        "group COLLATE key"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(order_clause, 0U),
        MYLITE_SQL_AST_CAST_BINARY_EXPRESSION,
        "order unary BINARY key"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_distinct_aggregate_placeholders(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    int failures = 0;

    failures += parse_ok("SELECT COUNT(DISTINCT CONCAT(c, 'x')), COUNT(DISTINCT c, n) FROM t");
    failures += parse_ok("SELECT SUM(DISTINCT n), AVG(DISTINCT n) FROM t");
    failures +=
        parse_ok("SELECT GROUP_CONCAT(DISTINCT c ORDER BY c SEPARATOR ',') FROM t GROUP BY grp");

    failures += parser_test_parse_sql(
        "SELECT GROUP_CONCAT(DISTINCT c ORDER BY c SEPARATOR ',') FROM t GROUP BY grp",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "GROUP_CONCAT DISTINCT placeholder"
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
