#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_expression_residual_placeholders(void);
static int test_expression_residual_malformed_tails(void);
static int expect_statement_kind(struct expected_statement expected);
static int parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_expression_residual_placeholders();
    failures += test_expression_residual_malformed_tails();

    return failures == 0 ? 0 : 1;
}

static int test_expression_residual_placeholders(void) {
    static const struct expected_statement statements[] = {
        {.sql = "SELECT * FROM t0 WHERE 0.9 > t0.c0",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT a, b, c FROM t1 WHERE (a > b) <> c",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "DO COUNT(DISTINCT ROUND(CAST(SLEEP(0) AS DECIMAL), NULL))",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT \"1900-01-01 00:00:00\" + INTERVAL 1<<20 HOUR",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t WHERE a = CURRENT_TIME",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t WHERE a = CURRENT_DATE",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT _latin1'B' BETWEEN _latin1'a' AND _latin1'c'",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t1 ORDER BY ADDTIME(a, '00:00:00') DESC",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT 1 FROM t1 GROUP BY INSERT(a,'1','11','1')",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT 1 FROM r GROUP BY MAKE_SET(1,c) WITH ROLLUP",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t1 WHERE NOT(NOT(a))",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT f1, f2, f3 FROM t1 WHERE f1 BETWEEN f2 AND f3",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t2,t3 WHERE f2 IN (f3,'2003-04-05')",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT argument FROM log_rows WHERE argument LIKE ('SET%')",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT a - INTERVAL(b) MICROSECOND FROM t",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_kind(statements[index]);
    }
    return failures;
}

static int test_expression_residual_malformed_tails(void) {
    int failures = 0;

    failures += parse_status("SELECT f(1,,2)", MYLITE_SQL_PARSE_SYNTAX_ERROR, "missing argument");
    failures += parse_status("DO f(1,,2)", MYLITE_SQL_PARSE_SYNTAX_ERROR, "DO missing argument");
    failures += parse_status("SELECT !", MYLITE_SQL_PARSE_SYNTAX_ERROR, "incomplete not");
    failures += parse_status("SELECT 1 <<", MYLITE_SQL_PARSE_SYNTAX_ERROR, "incomplete shift");
    failures += parse_status("SELECT a =", MYLITE_SQL_PARSE_SYNTAX_ERROR, "incomplete equals");
    failures += parse_status(
        "SELECT a - INTERVAL(,) MICROSECOND FROM t",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "malformed interval"
    );
    failures += parse_status("DO", MYLITE_SQL_PARSE_SYNTAX_ERROR, "incomplete DO");

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
