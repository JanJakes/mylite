#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_query_function_subquery_placeholders(void);
static int expect_statement_kind(struct expected_statement expected);
static int expect_syntax_error(const char *sql);
static int parse_ok(const char *sql);

int main(void) {
    int failures = 0;

    failures += test_query_function_subquery_placeholders();

    return failures == 0 ? 0 : 1;
}

static int test_query_function_subquery_placeholders(void) {
    static const struct expected_statement placeholders[] = {
        {.sql = "SELECT hex(a), hex(lower(a)), hex(upper(a)) FROM t1 ORDER BY binary(a)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT t1.a FROM t1 ORDER BY (SELECT SUM(t2.a) FROM t2)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT SUM(i) OVER w FROM t1 WINDOW w AS (PARTITION BY j ORDER BY i) "
                "ORDER BY SUM(i) OVER w",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "VALUES ROW((SELECT 1), 10)", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "INSERT INTO t1 (a, b) VALUES (888, (SELECT a))",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
    };
    int failures = 0;

    failures += parse_ok("SELECT ABS(-1)");
    failures += parse_ok("SELECT (SELECT 1)");
    failures += parse_ok("SELECT a FROM t1 WHERE a = 1");
    failures += expect_syntax_error("SELECT f(1,,2)");
    failures += expect_syntax_error("SELECT ABS(1) +");
    failures += expect_syntax_error("REPLACE INTO t VALUES ((SELECT 1))");
    failures += expect_syntax_error("REPLACE INTO t SET id = (SELECT 1)");
    failures += expect_syntax_error("WITH c AS (SELECT id FROM t) UPDATE t SET id = 1");
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

static int expect_syntax_error(const char *sql) {
    struct mylite_sql_parse_result result;
    int failures = parser_test_parse_sql(sql, MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);

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
