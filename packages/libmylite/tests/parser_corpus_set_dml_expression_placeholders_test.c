#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_set_dml_expression_placeholders(void);
static int test_set_dml_expression_malformed_tails(void);
static int expect_statement_kind(struct expected_statement expected);
static int parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_set_dml_expression_placeholders();
    failures += test_set_dml_expression_malformed_tails();

    return failures == 0 ? 0 : 1;
}

static int test_set_dml_expression_placeholders(void) {
    static const struct expected_statement placeholders[] = {
        {.sql = "SET @a = 1 / 0", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SET sql_mode = 32 + (65536 * 4)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SET @x = 1 + (SELECT COUNT(*) FROM t)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "INSERT INTO t VALUES (1 / 0, 2 * 3)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "INSERT IGNORE INTO t VALUES (2 / 0)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "INSERT INTO t VALUES (1) ON DUPLICATE KEY UPDATE x = VALUES(x) + 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "UPDATE t SET data = data * 2 WHERE id = 3",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "UPDATE mysql.server_cost SET cost_value = 0.5 * cost_value",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
    };
    int failures = 0;

    failures += expect_statement_kind((struct expected_statement){
        .sql = "UPDATE counters SET n = n + 1;",
        .kind = MYLITE_SQL_AST_UPDATE_STATEMENT,
    });
    for (size_t index = 0U; index < sizeof(placeholders) / sizeof(placeholders[0]); ++index) {
        failures += expect_statement_kind(placeholders[index]);
    }
    return failures;
}

static int test_set_dml_expression_malformed_tails(void) {
    int failures = 0;

    failures +=
        parse_status("SET @a = 1 +", MYLITE_SQL_PARSE_SYNTAX_ERROR, "incomplete SET operator");
    failures += parse_status(
        "INSERT INTO t VALUES (1+)",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete INSERT value operator"
    );
    failures += parse_status(
        "UPDATE t SET a = a * WHERE id = 1",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete UPDATE assignment operator"
    );
    failures += parse_status(
        "INSERT INTO t VALUES (1) ON DUPLICATE KEY UPDATE x = VALUES(x) +",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete duplicate update operator"
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
    if (expected.kind == MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT) {
        failures += parser_test_expect_child_count(statement, 0U, expected.sql);
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
