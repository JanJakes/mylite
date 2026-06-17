#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_query_expression_clause_placeholders(void);
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
        {.sql = "SELECT * FROM t WHERE u=256 IS NOT NULL",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t WHERE u=256 IS UNKNOWN",
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
        {.sql = "select * from t1 where ROW(1,2,3)=ROW(a,b,c)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT x FROM t GROUP BY x, MATCH(x) AGAINST ('abc') "
                "HAVING MATCH(x) AGAINST ('abc')",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "VALUES ROW(1),ROW(2) ORDER BY '1' DESC",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "select f2 from t1 where '2001-04-10 12:34:56' between f2 and '01-05-01'",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "select f2, f3 from t1 where '01-03-10' between f2 and f3",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "select * from t1,t2 where '01-01-01' in (f1, '01-02-03')",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t1 WHERE '100000000000000000000002' = value",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t1 WHERE '2010-02-01 09:31:02.0' <= a",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t1 WHERE '2010-02-01 09:31:02.0' >= a",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "select * from v1 where '2005.02.02'=f1",
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
    failures += parse_ok("SELECT a FROM t1 ORDER BY ABS(b - 5)");
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
