#include "parser_test_support.h"

static int test_explain_query_forms(void);
static int test_window_grammar_forms(void);
static int test_type_and_literal_forms(void);
static int test_group_concat_expression_forms(void);
static int parse_ok(const char *sql, const char *context);
static int parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
);
static int parse_status_with_modes(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    struct parser_test_parse_modes modes,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_explain_query_forms();
    failures += test_window_grammar_forms();
    failures += test_type_and_literal_forms();
    failures += test_group_concat_expression_forms();

    return failures == 0 ? 0 : 1;
}

static int test_explain_query_forms(void) {
    static const char *const explain_query_forms[] = {
        "EXPLAIN SELECT 1",
        "EXPLAIN FORMAT=TRADITIONAL SELECT 1",
        "EXPLAIN FORMAT=JSON SELECT 1",
        "EXPLAIN FORMAT=TREE SELECT 1",
        "EXPLAIN ANALYZE SELECT 1",
        "EXPLAIN ANALYZE FORMAT=TREE SELECT 1",
        "EXPLAIN SELECT * FROM t1 WHERE MATCH(title) AGAINST ('needle')",
        "EXPLAIN FORMAT=JSON SELECT ROW_NUMBER() RESPECT NULLS OVER () FROM t1",
        "EXPLAIN FORMAT=CSV SELECT * FROM t1 WHERE MATCH(title) AGAINST ('needle')",
        "EXPLAIN ANALYZE FORMAT=JSON SELECT * FROM t1 WHERE MATCH(title) AGAINST ('needle')",
        "EXPLAIN TABLE numbers",
        "EXPLAIN VALUES ROW(1), ROW(2)",
        "EXPLAIN INSERT INTO numbers VALUES (1)",
        "EXPLAIN REPLACE INTO numbers VALUES (1)",
        "EXPLAIN UPDATE numbers SET id = 1 WHERE id = 2",
        "EXPLAIN DELETE FROM numbers WHERE id = 1",
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    for (size_t index = 0U; index < sizeof(explain_query_forms) / sizeof(explain_query_forms[0]);
         ++index) {
        failures += parser_test_parse_sql(explain_query_forms[index], MYLITE_SQL_PARSE_OK, &result);
        statement = parser_test_child_at(result.root, 0U);
        failures += parser_test_expect_node(
            statement,
            MYLITE_SQL_AST_EXPLAIN_STATEMENT,
            explain_query_forms[index]
        );
        mylite_sql_parse_result_deinit(&result);
    }

    failures += parser_test_parse_sql("EXPLAIN numbers", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "EXPLAIN table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_status(
        "EXPLAIN FORMAT=JSON ANALYZE SELECT 1",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "EXPLAIN FORMAT before ANALYZE"
    );
    failures += parse_status(
        "EXPLAIN FORMAT=DEFAULT SELECT 1",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "EXPLAIN FORMAT DEFAULT"
    );
    failures += parse_status(
        "EXPLAIN FOR CONNECTION 1",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "EXPLAIN FOR CONNECTION"
    );

    return failures;
}

static int test_window_grammar_forms(void) {
    static const char *const window_forms[] = {
        "SELECT ROW_NUMBER() OVER () FROM numbers",
        "SELECT ROW_NUMBER() OVER w FROM numbers WINDOW w AS (ORDER BY id)",
        "SELECT ROW_NUMBER() OVER (w ORDER BY id) FROM numbers WINDOW w AS ()",
        "SELECT RANK() OVER (PARTITION BY grp, id ORDER BY id DESC) FROM numbers",
        "SELECT DENSE_RANK() OVER (ORDER BY id + 1 DESC ROWS UNBOUNDED PRECEDING) FROM numbers",
        "SELECT PERCENT_RANK() OVER (ORDER BY id RANGE CURRENT ROW) FROM numbers",
        "SELECT CUME_DIST() OVER (ORDER BY id ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) "
        "FROM numbers",
        "SELECT NTILE(4) OVER (PARTITION BY grp ORDER BY id ROWS BETWEEN 1 PRECEDING AND 1 "
        "FOLLOWING) FROM numbers",
        "SELECT LAG(value, 1, 0) OVER (PARTITION BY grp ORDER BY id DESC) FROM numbers",
        "SELECT LEAD(value, 2) OVER (ORDER BY id) FROM numbers",
        "SELECT FIRST_VALUE(value) OVER (ORDER BY id ROWS CURRENT ROW) FROM numbers",
        "SELECT LAST_VALUE(value) OVER (ORDER BY id ROWS BETWEEN CURRENT ROW AND UNBOUNDED "
        "FOLLOWING) FROM numbers",
        "SELECT NTH_VALUE(value, 2) OVER (ORDER BY id) FROM numbers",
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    for (size_t index = 0U; index < sizeof(window_forms) / sizeof(window_forms[0]); ++index) {
        failures += parser_test_parse_sql(window_forms[index], MYLITE_SQL_PARSE_OK, &result);
        statement = parser_test_child_at(result.root, 0U);
        failures += parser_test_expect_node(
            statement,
            MYLITE_SQL_AST_SELECT_STATEMENT,
            window_forms[index]
        );
        mylite_sql_parse_result_deinit(&result);
    }

    failures += parser_test_parse_sql(
        "SELECT ROW_NUMBER() OVER w FROM numbers WINDOW w AS (PARTITION BY grp ORDER BY id)",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        parser_test_first_child_kind(statement, MYLITE_SQL_AST_WINDOW_DEFINITION_LIST),
        MYLITE_SQL_AST_WINDOW_DEFINITION_LIST,
        "named window definition list"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_type_and_literal_forms(void) {
    static const char *const type_literal_forms[] = {
        "SELECT CAST('2024' AS YEAR)",
        "SELECT CONVERT('2024', YEAR)",
        "SELECT CAST('x' AS CHAR CHARSET utf8mb4)",
        "SELECT CAST('x' AS CHAR CHARSET BINARY)",
        "SELECT _utf8mb4'a'",
        "SELECT _binary X'41'",
        "SELECT _latin1 b'1000001'",
        "SELECT _utf8mb4'a' COLLATE utf8mb4_0900_ai_ci",
        "SELECT DATE '2024-01-02'",
        "SELECT TIME '12:34:56'",
        "SELECT TIMESTAMP '2024-01-02 12:34:56'",
        "SELECT DATE \"2024-01-02\"",
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(type_literal_forms) / sizeof(type_literal_forms[0]);
         ++index) {
        failures += parse_ok(type_literal_forms[index], type_literal_forms[index]);
    }

    return failures;
}

static int test_group_concat_expression_forms(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *function = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT GROUP_CONCAT(name ORDER BY id DESC SEPARATOR '|') FROM numbers",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    function = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    order_clause = parser_test_child_at(function, 1U);
    failures += parser_test_expect_node(
        function,
        MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION,
        "single-key GROUP_CONCAT"
    );
    failures += parser_test_expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "order");
    failures += parser_test_expect_node(
        parser_test_child_at(order_clause, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "single-key GROUP_CONCAT order key"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION,
        "single-key GROUP_CONCAT order direction"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_ok(
        "SELECT GROUP_CONCAT(IFNULL(name, '') ORDER BY id + 1 DESC, name SEPARATOR '|') "
        "FROM numbers",
        "expression GROUP_CONCAT argument and order keys"
    );

    return failures;
}

static int parse_ok(const char *sql, const char *context) {
    return parse_status(sql, MYLITE_SQL_PARSE_OK, context);
}

static int parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
) {
    return parse_status_with_modes(
        sql,
        expected_status,
        (struct parser_test_parse_modes){0},
        context
    );
}

static int parse_status_with_modes(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    struct parser_test_parse_modes modes,
    const char *context
) {
    struct mylite_sql_parse_result result;
    int failures = parser_test_parse_sql_with_modes(sql, expected_status, modes, &result);

    if (failures != 0) {
        (void)parser_test_expect_true(0, context);
    }
    mylite_sql_parse_result_deinit(&result);
    return failures;
}
