#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_parenthesized_table_reference_placeholders(void);
static int expect_statement_kind(struct expected_statement expected);
static int parse_ok(const char *sql);
static int parse_syntax_error(const char *sql);

int main(void) {
    int failures = 0;

    failures += test_parenthesized_table_reference_placeholders();

    return failures == 0 ? 0 : 1;
}

static int test_parenthesized_table_reference_placeholders(void) {
    static const struct expected_statement placeholders[] = {
        {.sql = "SELECT * FROM (t1)", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM (t1 JOIN t2 ON t1.id = t2.id)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t1 LEFT JOIN (t2 LEFT JOIN t3 ON t2.id = t3.id) "
                "ON t1.id = t2.id",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t1 LEFT JOIN (t2, t3) ON t1.id = t2.id",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t1 JOIN (t2) ON t1.id = t2.id",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM { OJ t1 LEFT OUTER JOIN t2 ON t1.id = t2.id }",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t1, t2 LEFT JOIN t3 ON t2.id = t3.id",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t2 LEFT JOIN t3 ON t2.id = t3.id, t1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM (( t1 JOIN t2 ON TRUE ))",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT 1 FROM ((t1))", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM (( t1, t2 ))", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM  ( (t1),  (t2) )",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t5 NATURAL JOIN "
                "((t1 NATURAL JOIN t2), (t3 NATURAL JOIN t4))",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM ((( t1, t2 ), t3))",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM ((((t1, t2)), t3))",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM ((t1, ( t2, t3 )))",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM ((t1, ((t2, t3))))",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT * FROM t1 LEFT JOIN t2 LEFT JOIN t3 ON t2.a=t3.a "
                "ON t1.a=t3.a",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT /*+JOIN_ORDER(t2,t3,t1) */ DISTINCT t2.pk FROM t1 "
                "LEFT JOIN t2 RIGHT OUTER JOIN t3 ON t2.f1 = t3.f3 "
                "ON t1.pk = t3.f2 WHERE t3.pk <> t2.pk",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT alias2.col_datetime_key FROM t1 AS alias1 "
                "LEFT JOIN t3 AS alias2 LEFT JOIN t2 AS alias3 "
                "LEFT JOIN t4 AS alias4 ON alias3.pk = alias4.col_int_key "
                "ON alias2.pk = alias3.col_int ON alias1.col_int = alias4.col_int",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
    };
    int failures = 0;

    failures += parse_ok("SELECT * FROM t1 JOIN t2 ON t1.id = t2.id");
    failures += parse_ok("SELECT * FROM (SELECT 1) AS dt");
    failures += parse_syntax_error("SELECT { OJ t1 LEFT OUTER JOIN t2 ON t1.id = t2.id }");
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

static int parse_syntax_error(const char *sql) {
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
