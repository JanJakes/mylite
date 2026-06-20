#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

struct expected_literal_left_between_ast {
    const char *sql;
    const char *lower;
    const char *upper;
};

struct expected_literal_left_in_ast {
    const char *sql;
    const char *first_list_value;
};

struct expected_row_constructor_predicate_ast {
    const char *sql;
    enum mylite_sql_ast_operator expected_operator;
};

static int test_query_expression_clause_placeholders(void);
static int expect_literal_left_between_ast(struct expected_literal_left_between_ast expected);
static int expect_literal_left_in_ast(struct expected_literal_left_in_ast expected);
static int expect_row_constructor_predicate_ast(
    struct expected_row_constructor_predicate_ast expected
);
static int expect_statement_kind(struct expected_statement expected);
static int parse_ok(const char *sql);
static int parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_query_expression_clause_placeholders();

    return failures == 0 ? 0 : 1;
}

static int test_query_expression_clause_placeholders(void) {
    static const struct expected_statement placeholders[] = {
        {.sql = "SELECT a, COUNT(*) FROM t1 GROUP BY a + 0 "
                "HAVING COUNT(*) >= 1 AND a > 0 ORDER BY a + 0",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT a, COUNT(*) FROM t1 GROUP BY a HAVING a = b - 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT t1.a FROM t1 JOIN t2 ON t1.a = t2.b - 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT a FROM t1 WHERE (a,b) = (1,2)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT a FROM t1 WHERE(a,b) = (1,2)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t1 WHERE f1->\"$.id\"= 5",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t1 WHERE f1->>\"$.name\" = \"James\"",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT {fn CONCAT(a1,a2)} FROM t1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "UPDATE t3 SET a4={d '1789-07-14'} WHERE a1=0",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t1 LEFT JOIN t2 ON t1.a = t2.a "
                "WHERE t1.a BETWEEN t2.b AND t1.b",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t1 LEFT JOIN t2 ON t1.a = t2.a "
                "WHERE t1.a IN(t2.a, t2.b)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t1 JOIN t2 ON ROW(1,2)=ROW(t1.a,t2.b)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT COUNT(*) FROM t1 HAVING ROW(1,2)=ROW(a,b)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT x FROM t GROUP BY x, MATCH(x) AGAINST ('abc') "
                "HAVING MATCH(x) AGAINST ('abc')",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT 1 FROM t1 GROUP BY @b := @a, @b",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "select `foo` ()", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT DISTINCT t2.col_int_key FROM t1 LEFT JOIN t2 "
                "ON t1.col_varchar_10 = t2.col_varchar_10_key "
                "WHERE t2.pk ORDER BY t2.col_int_key",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "WITH qn AS (SELECT 1) SELECT * FROM qn",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "WITH RECURSIVE qn(n) AS (SELECT 1 UNION ALL SELECT n + 1 FROM qn "
                "WHERE n < 3) SELECT * FROM qn",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "WITH qn AS (SELECT 1), qn AS (SELECT 2) SELECT 3",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "WITH cte AS (SELECT 1) (SELECT * FROM cte) LIMIT 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "WITH ids AS (SELECT id FROM t1) UPDATE t1 SET b = 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "WITH doomed AS (SELECT id FROM t1) DELETE FROM t1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "UPDATE t1, t2 SET t1.b = t1.b + 1 WHERE t1.a = t2.a",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "DELETE FROM t1 WHERE a = a + sleep(0) ORDER BY a LIMIT 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
    };
    int failures = 0;

    failures += parse_ok("SELECT a FROM t1 WHERE a = 1");
    failures += parse_ok("SELECT COUNT(*) FROM t1 WHERE a + 1 > 1");
    failures += parse_ok("SELECT COUNT(*) FROM t1 WHERE (a + 1) > 1");
    failures += parse_ok("SELECT COUNT(*) FROM t1 WHERE ((a + 1) * 2) > 4");
    failures += parse_ok("SELECT COUNT(*) FROM t1 WHERE ((a + 1) * 2) > 4 AND ((b + 1) * 2) > 8");
    failures += parse_ok("SELECT COUNT(*) FROM t1 WHERE a = b - 1");
    failures += parse_ok("SELECT COUNT(*) FROM t1 WHERE a = ABS(b - 1)");
    failures += parse_ok("SELECT COUNT(*) FROM t1 WHERE a + b = b + a");
    failures += parse_ok("SELECT COUNT(*) FROM t1 WHERE a < (b + c) * 2");
    failures += parse_ok("SELECT COUNT(*) FROM t1 WHERE a BETWEEN b - 7 AND b - 5");
    failures += parse_ok("SELECT COUNT(*) FROM t1 WHERE a IN (b - 5, 0)");
    failures += parse_ok("SELECT a FROM t1 WHERE a IN (SELECT a FROM t2 WHERE b + 1 > 20) "
                         "ORDER BY a");
    failures += parse_ok("SELECT a FROM t1 WHERE a IN (SELECT a FROM t2 WHERE (b + 1) > 20) "
                         "ORDER BY a");
    failures += parse_ok("SELECT a FROM t1 WHERE a IN (SELECT a FROM t2 WHERE ((b + 1) * 2) > 40) "
                         "ORDER BY a");
    failures += parse_ok("SELECT t1.a, t2.a FROM t1 JOIN t2 ON t1.a = t2.a");
    failures += parse_ok("SELECT * FROM (SELECT 1) AS dt");
    failures += parse_ok("SELECT * FROM t WHERE u=256 IS NULL");
    failures += parse_ok("SELECT * FROM t WHERE u=256 IS NOT NULL");
    failures += parse_ok("SELECT * FROM t WHERE u=256 IS UNKNOWN");
    failures += parse_ok("SELECT * FROM t WHERE u=256 IS NOT UNKNOWN");
    failures += parse_ok("SELECT * FROM t WHERE u > 256 IS UNKNOWN");
    failures += parse_ok("SELECT * FROM t WHERE u <=> NULL IS NOT UNKNOWN");
    failures += parse_ok("SELECT * FROM t1 WHERE '100000000000000000000002' = value");
    failures += parse_ok("SELECT * FROM t1 WHERE '2010-02-01 09:31:02.0' <= a");
    failures += parse_ok("SELECT * FROM t1 WHERE '2010-02-01 09:31:02.0' < a");
    failures += parse_ok("SELECT * FROM t1 WHERE '2010-02-01 09:31:02.0' >= a");
    failures += parse_ok("SELECT * FROM t1 WHERE '2010-02-02 00:00:00.0' > a");
    failures += parse_ok("SELECT * FROM t1 WHERE '5' <> value");
    failures += parse_ok("select * from v1 where '2005.02.02'=f1");
    failures += parse_ok("select * from v1 where '2005.02.02'<=>f1");
    failures += parse_ok("select f2 from t1 where '2001-04-10 12:34:56' between f2 and '01-05-01'");
    failures +=
        parse_ok("select f2 from t1 where '2001-04-10 12:34:56' not between f2 and '01-05-01'");
    failures += parse_ok("select f2, f3 from t1 where '01-03-10' between f2 and f3");
    failures += parse_ok("select * from t1,t2 where '01-01-01' in (f1, '01-02-03')");
    failures += parse_ok("select * from t1,t2 where '01-01-01' not in (f1, '01-02-03')");
    failures +=
        expect_row_constructor_predicate_ast((struct expected_row_constructor_predicate_ast){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ROW(1,2,'x')=ROW(a,b,c)",
            .expected_operator = MYLITE_SQL_AST_OPERATOR_EQUAL,
        });
    failures +=
        expect_row_constructor_predicate_ast((struct expected_row_constructor_predicate_ast){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ROW(1,2,'x')<=>ROW(a,b,c)",
            .expected_operator = MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL,
        });
    failures +=
        expect_row_constructor_predicate_ast((struct expected_row_constructor_predicate_ast){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ROW(1,2,'z')<>ROW(a,b,c)",
            .expected_operator = MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
        });
    failures +=
        expect_row_constructor_predicate_ast((struct expected_row_constructor_predicate_ast){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ROW(1,2,'z')!=ROW(a,b,c)",
            .expected_operator = MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
        });
    failures +=
        expect_row_constructor_predicate_ast((struct expected_row_constructor_predicate_ast){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ROW(1,2,NULL)<>ROW(a,b,c)",
            .expected_operator = MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
        });
    failures +=
        expect_row_constructor_predicate_ast((struct expected_row_constructor_predicate_ast){
            .sql = "SELECT COUNT(*) FROM t1 WHERE ROW(1,2,NULL)<=>ROW(a,b,c)",
            .expected_operator = MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL,
        });
    failures += expect_literal_left_between_ast((struct expected_literal_left_between_ast){
        .sql = "select f2 from t1 where '2001-04-10 12:34:56' between f2 and '01-05-01'",
        .lower = "f2",
        .upper = "'01-05-01'",
    });
    failures += expect_literal_left_between_ast((struct expected_literal_left_between_ast){
        .sql = "select f2, f3 from t1 where '01-03-10' between f2 and f3",
        .lower = "f2",
        .upper = "f3",
    });
    failures += expect_literal_left_in_ast((struct expected_literal_left_in_ast){
        .sql = "select * from t1,t2 where '01-01-01' in (f1, '01-02-03')",
        .first_list_value = "f1",
    });
    failures += parse_ok("SELECT a FROM t1 ORDER BY ABS(b - 5)");
    failures += parse_ok("VALUES ROW(1),ROW(2) ORDER BY NULL DESC");
    failures += parse_ok("VALUES ROW(1),ROW(2) ORDER BY '1' DESC");
    for (size_t index = 0U; index < sizeof(placeholders) / sizeof(placeholders[0]); ++index) {
        failures += expect_statement_kind(placeholders[index]);
    }
    failures += parse_status(
        "SELECT * FROM t1 WHERE 'x' BETWEEN a",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete literal-left BETWEEN"
    );
    failures += parse_status(
        "SELECT * FROM t1 WHERE 'x' IN (a,)",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "malformed literal-left IN list"
    );
    failures += parse_status(
        "SELECT 1 FROM t1 GROUP BY @b :=",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete GROUP BY user-variable assignment"
    );
    failures += parse_status(
        "select `foo` (",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete quoted function call"
    );
    return failures;
}

static int expect_literal_left_between_ast(struct expected_literal_left_between_ast expected) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    int failures = parser_test_parse_sql(expected.sql, MYLITE_SQL_PARSE_OK, &result);

    select = parser_test_child_at(result.root, 0U);
    where_clause = parser_test_child_at(select, 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures += parser_test_expect_node(
        predicate,
        MYLITE_SQL_AST_BETWEEN_PREDICATE,
        "literal-left BETWEEN predicate"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(predicate, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "literal-left BETWEEN subject"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(predicate, 1U),
        expected.lower,
        "BETWEEN lower"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(predicate, 2U),
        expected.upper,
        "BETWEEN upper"
    );
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int expect_literal_left_in_ast(struct expected_literal_left_in_ast expected) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    const struct mylite_sql_ast_node *value_list = NULL;
    int failures = parser_test_parse_sql(expected.sql, MYLITE_SQL_PARSE_OK, &result);

    select = parser_test_child_at(result.root, 0U);
    where_clause = parser_test_child_at(select, 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    value_list = parser_test_child_at(predicate, 1U);
    failures += parser_test_expect_node(predicate, MYLITE_SQL_AST_IN_PREDICATE, "literal-left IN");
    failures += parser_test_expect_literal(
        parser_test_child_at(predicate, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "literal-left IN subject"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(value_list, 0U),
        expected.first_list_value,
        "literal-left IN first value"
    );
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int expect_row_constructor_predicate_ast(
    struct expected_row_constructor_predicate_ast expected
) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    const struct mylite_sql_ast_node *left_row = NULL;
    const struct mylite_sql_ast_node *right_row = NULL;
    int failures = parser_test_parse_sql(expected.sql, MYLITE_SQL_PARSE_OK, &result);

    select = parser_test_child_at(result.root, 0U);
    where_clause = parser_test_child_at(select, 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    left_row = parser_test_child_at(predicate, 0U);
    right_row = parser_test_child_at(predicate, 1U);

    failures += parser_test_expect_node(
        predicate,
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "row-constructor WHERE predicate"
    );
    failures += parser_test_expect_operator(
        predicate,
        expected.expected_operator,
        "row-constructor comparison operator"
    );
    failures += parser_test_expect_node(
        left_row,
        MYLITE_SQL_AST_ROW_CONSTRUCTOR,
        "row-constructor left operand"
    );
    failures += parser_test_expect_node(
        right_row,
        MYLITE_SQL_AST_ROW_CONSTRUCTOR,
        "row-constructor right operand"
    );
    failures += parser_test_expect_child_count(left_row, 3U, "left ROW operand child count");
    failures += parser_test_expect_child_count(right_row, 3U, "right ROW operand child count");
    mylite_sql_parse_result_deinit(&result);
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
