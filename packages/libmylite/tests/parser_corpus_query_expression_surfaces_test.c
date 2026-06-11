#include "parser_test_support.h"

static int test_parenthesized_query_expressions(void);
static int test_query_block_set_operands(void);
static int test_view_query_expression_bodies(void);
static int test_derived_values_aliases(void);
static int test_query_final_tail_placeholders(void);
static int test_natural_and_using_joins(void);
static int test_window_null_treatment(void);
static int expect_unsupported_statement(const char *sql);
static int parse_ok(const char *sql);
static int parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_parenthesized_query_expressions();
    failures += test_query_block_set_operands();
    failures += test_view_query_expression_bodies();
    failures += test_derived_values_aliases();
    failures += test_query_final_tail_placeholders();
    failures += test_natural_and_using_joins();
    failures += test_window_null_treatment();

    return failures == 0 ? 0 : 1;
}

static int test_parenthesized_query_expressions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_ok("(SELECT 1)");
    failures += parse_ok("((SELECT 1))");
    failures += parse_ok("(TABLE t)");
    failures += parse_ok("(VALUES ROW(1), ROW(2))");

    failures +=
        parser_test_parse_sql("(SELECT 1 AS x) ORDER BY x LIMIT 1", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_PARENTHESIZED_QUERY_EXPRESSION,
        "parenthesized query wrapper"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "wrapped select"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_ORDER_BY_CLAUSE,
        "wrapper order clause"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_LIMIT_CLAUSE,
        "wrapper limit clause"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_query_block_set_operands(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *terms = NULL;
    int failures = 0;

    failures += parse_ok("(SELECT 1) UNION (SELECT 2)");
    failures += parse_ok("VALUES ROW(1), ROW(2) UNION SELECT 2");
    failures += parse_ok("TABLE t UNION SELECT 1");
    failures += parse_ok("SELECT 1 UNION VALUES ROW(1), ROW(2)");

    failures += parser_test_parse_sql("VALUES ROW(1) UNION SELECT 1", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT,
        "VALUES-started compound"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_VALUES_STATEMENT,
        "compound first VALUES"
    );
    terms = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(terms, MYLITE_SQL_AST_UNION_TERM_LIST, "compound terms");
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(terms, 0U), 0U),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "compound SELECT term"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_view_query_expression_bodies(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *view_body = NULL;
    int failures = 0;

    failures += parse_ok("CREATE VIEW v AS SELECT 1 UNION SELECT 2");
    failures += parse_ok("CREATE ALGORITHM=TEMPTABLE VIEW v AS (SELECT 1 UNION SELECT 2)");
    failures += parse_ok("CREATE VIEW v AS TABLE t");
    failures += parse_ok("ALTER VIEW v AS VALUES ROW(1), ROW(2)");

    failures += parser_test_parse_sql(
        "CREATE VIEW v AS SELECT 1 UNION SELECT 2",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    view_body = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_VIEW_STATEMENT,
        "compound create view statement"
    );
    failures += parser_test_expect_node(
        view_body,
        MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT,
        "compound create view body"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("ALTER VIEW v AS VALUES ROW(1)", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    view_body = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_VIEW_STATEMENT,
        "VALUES alter view statement"
    );
    failures += parser_test_expect_node(
        view_body,
        MYLITE_SQL_AST_VALUES_STATEMENT,
        "VALUES alter view body"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_derived_values_aliases(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *derived = NULL;
    int failures = 0;

    failures += parse_ok("SELECT * FROM (VALUES ROW(1, 10), ROW(2, 20)) AS dt(a,b)");
    failures += parse_ok("SELECT * FROM (TABLE t) dt");
    failures += parse_ok("SELECT * FROM ((SELECT 1)) AS dt(a)");
    failures += parse_ok("SELECT * FROM (VALUES ROW(1) UNION SELECT 2) AS dt(a)");

    failures += parser_test_parse_sql(
        "SELECT * FROM (VALUES ROW(1, 10)) AS dt(a,b)",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    derived = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(derived, MYLITE_SQL_AST_FROM_DERIVED, "derived VALUES");
    failures += parser_test_expect_node(
        parser_test_child_at(derived, 0U),
        MYLITE_SQL_AST_VALUES_STATEMENT,
        "derived VALUES inner"
    );
    failures +=
        parser_test_expect_span_text(parser_test_child_at(derived, 1U), "dt", "derived alias");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_query_final_tail_placeholders(void) {
    int failures = 0;

    failures += expect_unsupported_statement("SELECT 1 UNION SELECT 1 LIMIT 0");
    failures += expect_unsupported_statement("select id from t1 union all select 99 order by 1");
    failures += expect_unsupported_statement("SELECT 1 FROM DUAL LIMIT 1 INTO @var FOR UPDATE");
    failures += expect_unsupported_statement("SELECT 1 FROM DUAL LIMIT 1 FOR UPDATE INTO @var");
    failures += expect_unsupported_statement("SELECT 1 UNION SELECT 1 FOR UPDATE INTO @var");
    failures +=
        expect_unsupported_statement("SELECT 1 UNION SELECT 1 FROM DUAL INTO @var FOR UPDATE");

    failures += parse_status(
        "SELECT 1 UNION SELECT 1 LIMIT",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete compound LIMIT"
    );
    failures += parse_status(
        "SELECT 1 FROM DUAL LIMIT 1 INTO",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete SELECT INTO tail"
    );
    failures += parse_status(
        "SELECT 1 UNION SELECT 1 FOR UPDATE OF",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete compound locking tail"
    );

    return failures;
}

static int test_natural_and_using_joins(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *from_join = NULL;
    const struct mylite_sql_ast_node *condition = NULL;
    int failures = 0;

    failures += parse_ok("SELECT * FROM t1 JOIN t2 USING (shared)");
    failures += parse_ok("SELECT * FROM t1 LEFT JOIN t2 USING (shared)");
    failures += parse_ok("SELECT * FROM t1 STRAIGHT_JOIN t2 USING (shared)");
    failures += parse_ok("SELECT * FROM t1 NATURAL JOIN t2");
    failures += parse_ok("SELECT * FROM t1 NATURAL LEFT OUTER JOIN t2");
    failures += parse_ok("SELECT * FROM t1 NATURAL RIGHT JOIN t2");

    failures += parser_test_parse_sql(
        "SELECT * FROM t1 JOIN t2 USING (shared)",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_join = parser_test_child_at(statement, 1U);
    condition = parser_test_child_at(from_join, 2U);
    failures += parser_test_expect_node(condition, MYLITE_SQL_AST_JOIN_USING_CLAUSE, "USING");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(condition, 0U), 0U),
        "shared",
        "USING column"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT * FROM t1 NATURAL LEFT OUTER JOIN t2",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    from_join = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_join_kind(from_join) == MYLITE_SQL_AST_JOIN_KIND_NATURAL_LEFT_OUTER,
        "natural left outer join kind"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_window_null_treatment(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *function = NULL;
    int failures = 0;

    failures += parse_ok("SELECT FIRST_VALUE(id) RESPECT NULLS OVER (ORDER BY id) FROM t");
    failures += parse_ok("SELECT LAST_VALUE(id) IGNORE NULLS OVER (ORDER BY id) FROM t");
    failures += parse_ok("SELECT NTH_VALUE(id, 1) RESPECT NULLS OVER (ORDER BY id) FROM t");
    failures += parse_ok("SELECT NTH_VALUE(id, 1) FROM FIRST OVER (ORDER BY id) FROM t");
    failures += parse_ok("SELECT LAG(id, 1, 0) IGNORE NULLS OVER (ORDER BY id) FROM t");
    failures += parse_ok("SELECT LEAD(id) RESPECT NULLS OVER (ORDER BY id) FROM t");

    failures += parser_test_parse_sql(
        "SELECT LEAD(id) IGNORE NULLS OVER (ORDER BY id) FROM t",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    function = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(function, MYLITE_SQL_AST_LEAD_FUNCTION, "LEAD");
    failures += parser_test_expect_node(
        parser_test_child_at(function, 2U),
        MYLITE_SQL_AST_WINDOW_IGNORE_NULLS,
        "IGNORE NULLS marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_status(
        "SELECT ROW_NUMBER() RESPECT NULLS OVER () FROM t",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "ROW_NUMBER null treatment"
    );
    failures += parse_status(
        "SELECT NTH_VALUE(id, 1 FROM FIRST) OVER (ORDER BY id) FROM t",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "NTH_VALUE FROM FIRST"
    );

    return failures;
}

static int expect_unsupported_statement(const char *sql) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = parser_test_parse_sql(sql, MYLITE_SQL_PARSE_OK, &result);

    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT, sql);
    failures += parser_test_expect_child_count(statement, 0U, sql);
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
