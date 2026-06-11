#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_expression_operator_temporal_placeholders(void);
static int test_expression_operator_temporal_malformed_tails(void);
static int expect_statement_kind(struct expected_statement expected);
static int parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_expression_operator_temporal_placeholders();
    failures += test_expression_operator_temporal_malformed_tails();

    return failures == 0 ? 0 : 1;
}

static int test_expression_operator_temporal_placeholders(void) {
    static const struct expected_statement statements[] = {
        {.sql = "SELECT 1 && 1", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT 1 || 0", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT 'ab' NOT LIKE 'ac'", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT id FROM t WHERE c1 LIKE c2 ORDER BY id",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT 'a%' LIKE 'a!%' ESCAPE '!'",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT '1998-01-01' + INTERVAL 1 DAY",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT TIME'10:10:10' + INTERVAL .6 SECOND",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT 1^1 + INTERVAL 1+1 SECOND & 1 + INTERVAL 1+1 SECOND",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT 1%2 - INTERVAL 1^1 SECOND | 1%2 - INTERVAL 1^1 SECOND",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT 'mood' SOUNDS LIKE 'mud'",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SELECT COUNT(*) FROM t WHERE d IN (DATE'2024-01-01', DATE'2024-01-03')",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_kind(statements[index]);
    }
    return failures;
}

static int test_expression_operator_temporal_malformed_tails(void) {
    int failures = 0;

    failures += parse_status(
        "SELECT 1 &&",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete deprecated logical AND"
    );
    failures += parse_status(
        "SELECT && 1",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "missing deprecated logical AND left operand"
    );
    failures +=
        parse_status("SELECT 'ab' NOT LIKE", MYLITE_SQL_PARSE_SYNTAX_ERROR, "incomplete NOT LIKE");
    failures += parse_status(
        "SELECT NOT LIKE 'ac'",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "missing NOT LIKE left operand"
    );
    failures += parse_status(
        "SELECT * FROM t WHERE c LIKE",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete LIKE"
    );
    failures += parse_status(
        "SELECT * FROM t WHERE c LIKE ESCAPE '!'",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "missing LIKE ESCAPE pattern"
    );
    failures += parse_status(
        "SELECT 'a%' LIKE 'a!%' ESCAPE +",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete LIKE ESCAPE"
    );
    failures += parse_status(
        "SELECT INTERVAL 1",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete INTERVAL expression"
    );
    failures += parse_status(
        "SELECT 'mood' SOUNDS LIKE",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete SOUNDS LIKE"
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
