#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_parenthesized_table_reference_placeholders(void);
static int expect_statement_kind(struct expected_statement expected);
static int parse_ok(const char *sql);

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
    };
    int failures = 0;

    failures += parse_ok("SELECT * FROM t1 JOIN t2 ON t1.id = t2.id");
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
