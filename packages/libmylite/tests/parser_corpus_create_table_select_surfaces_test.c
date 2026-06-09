#include "parser_test_support.h"

#include <stddef.h>

struct expected_statement {
    const char *sql;
    enum mylite_sql_ast_node_kind kind;
};

static int test_create_table_select_placeholders(void);
static int test_supported_create_table_select_still_parses(void);
static int test_incomplete_create_table_select_forms(void);
static int expect_statement_kind(struct expected_statement expected);
static int expect_parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_create_table_select_placeholders();
    failures += test_supported_create_table_select_still_parses();
    failures += test_incomplete_create_table_select_forms();

    return failures == 0 ? 0 : 1;
}

static int test_create_table_select_placeholders(void) {
    static const struct expected_statement statements[] = {
        {.sql = "CREATE TABLE t2 (KEY (b)) SELECT * FROM t1",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "CREATE TABLE t1 (a INT) ENGINE=InnoDB SELECT 42 AS a",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "CREATE TEMPORARY TABLE t1 (a INT) SELECT 42 AS a",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "CREATE TABLE t1 AS SELECT 1 AS a UNION SELECT 2",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "CREATE TABLE t1 AS (SELECT 1 AS a) UNION (SELECT 2)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "CREATE TABLE t1 (SELECT 1 AS a) UNION (SELECT 2)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "CREATE TABLE t1 WITH cte AS (SELECT 1 AS a) SELECT a FROM cte",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "CREATE TABLE t1 AS TABLE source",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "CREATE TABLE t1 AS VALUES ROW(1), ROW(2)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "CREATE TABLE t1 WITH cte AS (SELECT 1 AS a) TABLE cte",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "CREATE TABLE t1 WITH cte AS (SELECT 1 AS a) VALUES ROW(1)",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "CREATE TABLE t1 (a INT) PARTITION BY HASH (a) AS SELECT 1 AS a",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
        {.sql = "CREATE TABLE t1 (a INT) PARTITION BY RANGE (a) "
                "(PARTITION p0 VALUES LESS THAN (10)) SELECT 1 AS a",
         .kind = MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT},
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_kind(statements[index]);
    }
    return failures;
}

static int test_supported_create_table_select_still_parses(void) {
    static const struct expected_statement statements[] = {
        {.sql = "CREATE TABLE t1 AS SELECT id FROM source",
         .kind = MYLITE_SQL_AST_CREATE_TABLE_SELECT_STATEMENT},
        {.sql = "CREATE TABLE t1 SELECT id FROM source",
         .kind = MYLITE_SQL_AST_CREATE_TABLE_SELECT_STATEMENT},
        {.sql = "CREATE TEMPORARY TABLE t1 AS SELECT id FROM source",
         .kind = MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_SELECT_STATEMENT},
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_kind(statements[index]);
    }
    return failures;
}

static int test_incomplete_create_table_select_forms(void) {
    int failures = 0;

    failures += expect_parse_status(
        "CREATE TABLE t1 AS",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "missing query source remains syntax error"
    );
    failures += expect_parse_status(
        "CREATE TABLE t1 (a INT) SELECT",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete SELECT remains syntax error"
    );
    failures += expect_parse_status(
        "CREATE TABLE t1 AS VALUES",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "incomplete VALUES remains syntax error"
    );
    failures += expect_parse_status(
        "CREATE TABLE t1 (a INT) PARTITION BY HASH (a) AS",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        "partitioned CTAS without query remains syntax error"
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
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int expect_parse_status(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    const char *context
) {
    struct mylite_sql_parse_result result;
    int failures = parser_test_parse_sql(sql, expected_status, &result);

    mylite_sql_parse_result_deinit(&result);
    (void)context;
    return failures;
}
