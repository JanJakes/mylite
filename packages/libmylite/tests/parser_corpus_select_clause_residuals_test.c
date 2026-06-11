#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_select_clause_residuals(void);
static int expect_statement_kind(struct expected_statement expected);
static int parse_ok(const char *sql);

int main(void) {
    int failures = 0;

    failures += test_select_clause_residuals();

    return failures == 0 ? 0 : 1;
}

static int test_select_clause_residuals(void) {
    static const struct expected_statement placeholders[] = {
        {.sql = "SELECT id FROM t1 ORDER BY NULL",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT id FROM t1 ORDER BY 'a' DESC",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT id FROM t1 ORDER BY @rank",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT a,b FROM t1 GROUP BY a,b HAVING b='hello'",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT t1.a AS t1c1, t2.a AS t2c1 "
                "FROM t1 JOIN t2 ON t1.id=t2.id HAVING t1c1 != t2c1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT a FROM t1 GROUP BY a HAVING a IN (10,20)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
    };
    int failures = 0;

    failures += parse_ok("SELECT 0 LIMIT 0");
    failures += parse_ok("SELECT 1 AS a LIMIT 1, 10");
    failures += parse_ok("SELECT 1 AS a LIMIT 1 OFFSET 2");
    failures += parse_ok("SELECT 9 FROM DUAL WHERE 1 LIMIT 1");
    failures += parse_ok("SELECT id FROM t1 FOR SHARE OF t1 FOR UPDATE OF t2");
    failures += parse_ok("SELECT t1.id, t2.id FROM t1 JOIN t2 ON t1.id = t2.id "
                         "FOR SHARE OF t1 NOWAIT FOR UPDATE OF t2 SKIP LOCKED");
    failures += parse_ok("SELECT id FROM t1 LOCK IN SHARE MODE FOR UPDATE");
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
