#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_between_expression_surfaces(void);
static int test_exists_expression_surfaces(void);
static int test_query_scalar_expression_placeholders(void);
static int expect_statement_kind(struct expected_statement expected);
static int parse_ok(const char *sql);
static int parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_between_expression_surfaces();
    failures += test_exists_expression_surfaces();
    failures += test_query_scalar_expression_placeholders();

    return failures == 0 ? 0 : 1;
}

static int test_between_expression_surfaces(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    int failures = 0;

    failures += parse_ok("SELECT 2 BETWEEN 1 AND 3, 2 NOT BETWEEN 4 AND 8");
    failures += parse_ok("SELECT CAST(100 AS UNSIGNED) BETWEEN 1 AND -1");
    failures += parse_ok("SELECT (184467440737095 BETWEEN 0 AND 18446744073709551500)");
    failures += parse_ok("SELECT IF(2 BETWEEN 1 AND 3, 1, 0)");

    failures += parser_test_parse_sql("SELECT 2 BETWEEN 1 AND 3", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_BETWEEN_PREDICATE,
        "scalar BETWEEN expression"
    );
    failures += parser_test_expect_child_count(expression, 3U, "BETWEEN child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "BETWEEN subject"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "BETWEEN lower bound"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 2U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "BETWEEN upper bound"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT 2 NOT BETWEEN 1 AND 3", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_NOT_PREDICATE, "NOT BETWEEN wrapper");
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_BETWEEN_PREDICATE,
        "NOT BETWEEN inner predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_exists_expression_surfaces(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    int failures = 0;

    failures += parse_ok("SELECT EXISTS (SELECT 1)");
    failures += parse_ok("SELECT EXISTS (VALUES ROW(1))");
    failures += parse_ok("SELECT (VALUES ROW(1))");
    failures += parse_ok("SELECT NOT EXISTS (SELECT 1 FROM DUAL WHERE FALSE)");
    failures += parse_ok("SELECT 1 FROM DUAL WHERE EXISTS (SELECT 1)");
    failures += parse_ok("SELECT 1 FROM DUAL WHERE EXISTS (VALUES ROW(1))");
    failures += parse_ok("SELECT id FROM t_outer WHERE NOT EXISTS (SELECT 1 FROM t_inner)");

    failures += parser_test_parse_sql("SELECT EXISTS (SELECT 1)", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_EXISTS_PREDICATE,
        "scalar EXISTS expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "scalar EXISTS select child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT EXISTS (VALUES ROW(1))", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_EXISTS_PREDICATE,
        "scalar EXISTS VALUES expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_VALUES_STATEMENT,
        "scalar EXISTS VALUES child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT (VALUES ROW(1))", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_SCALAR_SUBQUERY,
        "scalar VALUES subquery"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_VALUES_STATEMENT,
        "scalar VALUES subquery child"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_query_scalar_expression_placeholders(void) {
    static const struct expected_statement statements[] = {
        {.sql = "SELECT 1 IN (1, 2)", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT 1 NOT IN (SELECT 2)", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT 1 = ANY (SELECT 1)", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT 1 <> SOME (SELECT 2)", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT
        },
        {.sql = "SELECT 2 > ALL (SELECT 1)", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t1 JOIN LATERAL (SELECT 1) AS dt ON TRUE",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
    };
    int failures = 0;

    failures += parse_ok("SELECT a, GROUPING(a) FROM t1 GROUP BY a WITH ROLLUP");
    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_kind(statements[index]);
    }
    failures += parse_status(
        "SELECT id FROM t WHERE id IN (nn)",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "malformed descriptor IN predicate"
    );
    failures += parse_status(
        "SELECT id FROM t WHERE id IN (DATABASE())",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "unsupported descriptor IN function operand"
    );
    return failures;
}

static int expect_statement_kind(struct expected_statement expected) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(expected.sql, MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(statement, expected.kind, expected.sql);
    failures += parser_test_expect_child_count(statement, 0U, expected.sql);
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
) {
    struct mylite_sql_parse_result result;
    int failures = parser_test_parse_sql(sql, expected_status, &result);

    if (failures != 0) {
        (void)parser_test_expect_true(0, context);
    }
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
