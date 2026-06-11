#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_executable_admin_set_residuals(void);
static int test_placeholder_admin_set_residuals(void);
static int test_malformed_and_legacy_residuals(void);
static int expect_statement_kind(struct expected_statement expected);
static int parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_executable_admin_set_residuals();
    failures += test_placeholder_admin_set_residuals();
    failures += test_malformed_and_legacy_residuals();

    return failures == 0 ? 0 : 1;
}

static int test_executable_admin_set_residuals(void) {
    static const struct expected_statement statements[] = {
        {.sql = "ANALYZE TABLES t1", .kind = MYLITE_SQL_AST_ANALYZE_TABLE_STATEMENT},
        {.sql = "OPTIMIZE TABLES t1", .kind = MYLITE_SQL_AST_OPTIMIZE_TABLE_STATEMENT},
        {.sql = "DESCRIBE t1 f1", .kind = MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT},
        {.sql = "DESC t1 'f%'", .kind = MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT},
        {.sql = "EXPLAIN t1 f1", .kind = MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT},
        {.sql = "EXPLAIN t1 'f%'", .kind = MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT},
        {.sql = "SHOW EXTENDED COLUMNS FROM t1", .kind = MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT},
        {.sql = "SHOW EXTENDED FULL COLUMNS FROM t1",
         .kind = MYLITE_SQL_AST_SHOW_FULL_COLUMNS_STATEMENT},
        {.sql = "SHOW EXTENDED INDEX FROM t1", .kind = MYLITE_SQL_AST_SHOW_INDEX_STATEMENT},
        {.sql = "SET @@time_zone := 'UTC'", .kind = MYLITE_SQL_AST_SET_STATEMENT},
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_kind(statements[index]);
    }
    return failures;
}

static int test_placeholder_admin_set_residuals(void) {
    static const struct expected_statement statements[] = {
        {.sql = "DESCRIBE SELECT * FROM t1 WHERE t1='ABC'", .kind = MYLITE_SQL_AST_EXPLAIN_STATEMENT
        },
        {.sql = "EXPLAIN ANALYZE DELETE t1 FROM t t1, t t2 WHERE t1.x = t2.x + 1",
         .kind = MYLITE_SQL_AST_EXPLAIN_STATEMENT},
        {.sql = "SHOW ENGINE CSV LOGS", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SHOW ENGINE MyISAM MUTEX", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SHOW TRIGGERS WHERE 0", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SHOW OPEN TABLES WHERE f1()=0",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SET sql_mode = sys.LIST_ADD(@@sql_mode, 'ANSI_QUOTES')",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SET @@sql_mode := @@sql_mode", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT
        },
        {.sql = "SET optimizer_switch=`mrr=on,mrr_cost_based=off`",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SET autocommit=0, PERSIST auto_increment_offset=10",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SET @x = EXISTS (SELECT x FROM t)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "SET @lparam = \"a\" \"b\"", .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_kind(statements[index]);
    }
    return failures;
}

static int test_malformed_and_legacy_residuals(void) {
    int failures = 0;

    failures +=
        parse_status("SHOW MASTER STATUS", MYLITE_SQL_PARSE_SYNTAX_ERROR, "legacy master status");
    failures +=
        parse_status("SHOW SLAVE STATUS", MYLITE_SQL_PARSE_SYNTAX_ERROR, "legacy slave status");
    failures += parse_status("SHOW ENGINE CSV", MYLITE_SQL_PARSE_SYNTAX_ERROR, "incomplete engine");
    failures += parse_status("DESCRIBE", MYLITE_SQL_PARSE_SYNTAX_ERROR, "incomplete describe");
    failures += parse_status("SET app.autocommit = 1", MYLITE_SQL_PARSE_SYNTAX_ERROR, "dotted set");

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
