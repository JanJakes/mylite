#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_query_expression_clause_placeholders(void);
static int expect_statement_kind(struct expected_statement expected);
static int parse_ok(const char *sql);

int main(void) {
    int failures = 0;

    failures += test_query_expression_clause_placeholders();

    return failures == 0 ? 0 : 1;
}

static int test_query_expression_clause_placeholders(void) {
    static const struct expected_statement placeholders[] = {
        {.sql = "SELECT COUNT(*) FROM t1 WHERE a + 1 > 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT a FROM t1 ORDER BY ABS(b - 5)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT a, COUNT(*) FROM t1 GROUP BY a + 0 "
                "HAVING COUNT(*) >= 1 AND a > 0 ORDER BY a + 0",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT a FROM t1 WHERE (a,b) = (1,2)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT a FROM t1 WHERE(a,b) = (1,2)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT a FROM t1 WHERE a IN (SELECT a FROM t2 WHERE b + 1 > 20) "
                "ORDER BY a",
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
    failures += parse_ok("SELECT t1.a, t2.a FROM t1 JOIN t2 ON t1.a = t2.a");
    failures += parse_ok("SELECT * FROM (SELECT 1) AS dt");
    for (size_t index = 0U; index < sizeof(placeholders) / sizeof(placeholders[0]); ++index) {
        failures += expect_statement_kind(placeholders[index]);
    }
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
