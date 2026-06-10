#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
    int requests_chain;
};

static int test_transaction_completion_grammar(void);
static int test_rename_tables_alias(void);
static int test_admin_placeholders(void);
static int test_malformed_tails(void);
static int expect_statement(struct expected_statement expected);
static int parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_transaction_completion_grammar();
    failures += test_rename_tables_alias();
    failures += test_admin_placeholders();
    failures += test_malformed_tails();

    return failures == 0 ? 0 : 1;
}

static int test_transaction_completion_grammar(void) {
    static const struct expected_statement statements[] = {
        {.sql = "COMMIT", .kind = MYLITE_SQL_AST_COMMIT_STATEMENT, .requests_chain = 0},
        {.sql = "COMMIT WORK", .kind = MYLITE_SQL_AST_COMMIT_STATEMENT, .requests_chain = 0},
        {.sql = "COMMIT AND CHAIN", .kind = MYLITE_SQL_AST_COMMIT_STATEMENT, .requests_chain = 1},
        {.sql = "COMMIT WORK AND NO CHAIN",
         .kind = MYLITE_SQL_AST_COMMIT_STATEMENT,
         .requests_chain = 0},
        {.sql = "COMMIT RELEASE", .kind = MYLITE_SQL_AST_COMMIT_STATEMENT, .requests_chain = 0},
        {.sql = "COMMIT AND NO CHAIN NO RELEASE",
         .kind = MYLITE_SQL_AST_COMMIT_STATEMENT,
         .requests_chain = 0},
        {.sql = "ROLLBACK AND CHAIN", .kind = MYLITE_SQL_AST_ROLLBACK_STATEMENT, .requests_chain = 1
        },
        {.sql = "ROLLBACK WORK AND NO CHAIN RELEASE",
         .kind = MYLITE_SQL_AST_ROLLBACK_STATEMENT,
         .requests_chain = 0},
        {.sql = "ROLLBACK NO RELEASE",
         .kind = MYLITE_SQL_AST_ROLLBACK_STATEMENT,
         .requests_chain = 0},
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement(statements[index]);
    }
    return failures;
}

static int test_rename_tables_alias(void) {
    int failures = 0;

    failures += expect_statement((struct expected_statement){
        .sql = "RENAME TABLES t1 TO t2",
        .kind = MYLITE_SQL_AST_RENAME_TABLE_STATEMENT,
    });
    failures += expect_statement((struct expected_statement){
        .sql = "RENAME TABLES a TO b, c TO d",
        .kind = MYLITE_SQL_AST_RENAME_TABLE_STATEMENT,
    });

    return failures;
}

static int test_admin_placeholders(void) {
    static const struct expected_statement statements[] = {
        {.sql = "CREATE SERVER srv FOREIGN DATA WRAPPER mysql OPTIONS (DATABASE 'test')",
         .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "ALTER SERVER srv OPTIONS (USER 'sally')",
         .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "DROP SERVER IF EXISTS srv", .kind = MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT},
        {.sql = "ALTER SCHEMA app ENCRYPTION = 'N'",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "ALTER DATABASE app READ ONLY DEFAULT",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "ALTER DATABASE app READ ONLY = 1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement(statements[index]);
    }
    return failures;
}

static int test_malformed_tails(void) {
    int failures = 0;

    failures += parse_status("COMMIT AND", MYLITE_SQL_PARSE_SYNTAX_ERROR, "incomplete commit");
    failures +=
        parse_status("COMMIT AND NO", MYLITE_SQL_PARSE_SYNTAX_ERROR, "incomplete commit no-chain");
    failures += parse_status(
        "ROLLBACK RELEASE AND CHAIN",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "out-of-order rollback completion"
    );
    failures +=
        parse_status("RENAME TABLES old TO", MYLITE_SQL_PARSE_SYNTAX_ERROR, "incomplete rename");
    failures +=
        parse_status("CREATE SERVER", MYLITE_SQL_PARSE_SYNTAX_ERROR, "incomplete create server");
    failures +=
        parse_status("ALTER SERVER srv", MYLITE_SQL_PARSE_SYNTAX_ERROR, "incomplete alter server");
    failures +=
        parse_status("DROP SERVER", MYLITE_SQL_PARSE_SYNTAX_ERROR, "incomplete drop server");
    failures +=
        parse_status("DROP SERVER IF", MYLITE_SQL_PARSE_SYNTAX_ERROR, "incomplete drop server if");
    failures += parse_status(
        "DROP SERVER IF EXISTS",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete drop server if exists"
    );
    failures += parse_status(
        "DROP SERVER EXISTS",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "malformed drop server exists"
    );
    failures += parse_status(
        "ALTER SCHEMA app READ ONLY",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete schema read only option"
    );
    failures += parse_status(
        "ALTER SCHEMA app ENCRYPTION",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete schema encryption option"
    );

    return failures;
}

static int expect_statement(struct expected_statement expected) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *child = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(expected.sql, MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(statement, expected.kind, expected.sql);
    if (expected.requests_chain) {
        failures += parser_test_expect_child_count(statement, 1U, expected.sql);
        child = parser_test_child_at(statement, 0U);
        failures += parser_test_expect_node(
            child,
            MYLITE_SQL_AST_TRANSACTION_CHAIN_COMPLETION,
            expected.sql
        );
    } else if (expected.kind == MYLITE_SQL_AST_COMMIT_STATEMENT ||
               expected.kind == MYLITE_SQL_AST_ROLLBACK_STATEMENT ||
               expected.kind == MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT ||
               expected.kind == MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT) {
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
