#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_nonreserved_identifier_statements(void);
static int expect_statement_kind(struct expected_statement expected);

int main(void) {
    return test_nonreserved_identifier_statements() == 0 ? 0 : 1;
}

static int test_nonreserved_identifier_statements(void) {
    static const struct expected_statement statements[] = {
        {
            .sql = "CREATE TABLE t1 (current INT, diagnostics INT, "
                   "number INT, returned_sqlstate INT)",
            .kind = MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        },
        {
            .sql = "INSERT INTO t1 (current, diagnostics, number, returned_sqlstate) "
                   "VALUES (1,2,3,4)",
            .kind = MYLITE_SQL_AST_INSERT_STATEMENT,
        },
        {
            .sql = "SELECT current, diagnostics, number, returned_sqlstate "
                   "FROM t1 WHERE number = 3",
            .kind = MYLITE_SQL_AST_SELECT_STATEMENT,
        },
        {
            .sql = "CREATE TABLE t0 (skip INT, locked INT, nowait INT)",
            .kind = MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        },
        {
            .sql = "CREATE TABLE diag_non_reserved ("
                   "diagnostics INT, current INT, stacked INT, exception INT)",
            .kind = MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        },
        {
            .sql = "CREATE TABLE SESSION_USER(a INT)",
            .kind = MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        },
        {
            .sql = "CREATE TABLE SYSTEM_USER(a INT)",
            .kind = MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        },
        {
            .sql = "OPTIMIZE TABLES columns_priv, db, user",
            .kind = MYLITE_SQL_AST_OPTIMIZE_TABLE_STATEMENT,
        },
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_kind(statements[index]);
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
    mylite_sql_parse_result_deinit(&result);
    return failures;
}
