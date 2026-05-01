#include "sql/mylite_ast.h"
#include "sql/mylite_parser.h"

#include <stdio.h>
#include <string.h>

static int test_empty_script(void);
static int test_use_statements(void);
static int test_schema_lifecycle_statements(void);
static int test_connection_charset_statements(void);
static int test_select_expression_list(void);
static int test_information_schema_select(void);
static int test_unary_and_parenthesized_expression(void);
static int test_literal_categories(void);
static int test_qualified_identifier_keyword_part(void);
static int test_comments_are_skipped(void);
static int test_syntax_errors(void);
static int test_lexer_errors(void);
static int parse_sql(const char *sql, enum mylite_sql_parse_status expected_status,
                     struct mylite_sql_parse_result *out_result);
static const struct mylite_sql_ast_node *child_at(const struct mylite_sql_ast_node *node,
                                                  size_t index);
static int expect_node(const struct mylite_sql_ast_node *node,
                       enum mylite_sql_ast_node_kind expected_kind, const char *context);
static int expect_child_count(const struct mylite_sql_ast_node *node, size_t expected,
                              const char *context);
static int expect_span_text(const struct mylite_sql_ast_node *node, const char *expected,
                            const char *context);
static int expect_literal(const struct mylite_sql_ast_node *node,
                          enum mylite_sql_ast_literal_kind expected, const char *context);
static int expect_operator(const struct mylite_sql_ast_node *node,
                           enum mylite_sql_ast_operator expected, const char *context);
static int expect_schema_option(const struct mylite_sql_ast_node *node,
                                enum mylite_sql_ast_schema_option expected, const char *context);

int main(void)
{
    int failures = 0;

    failures += test_empty_script();
    failures += test_use_statements();
    failures += test_schema_lifecycle_statements();
    failures += test_connection_charset_statements();
    failures += test_select_expression_list();
    failures += test_information_schema_select();
    failures += test_unary_and_parenthesized_expression();
    failures += test_literal_categories();
    failures += test_qualified_identifier_keyword_part();
    failures += test_comments_are_skipped();
    failures += test_syntax_errors();
    failures += test_lexer_errors();

    return failures == 0 ? 0 : 1;
}

static int test_empty_script(void)
{
    struct mylite_sql_parse_result result;
    int failures = 0;

    failures += parse_sql("", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(result.root, MYLITE_SQL_AST_SCRIPT, "empty root");
    failures += expect_child_count(result.root, 0U, "empty root");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 1U, "trailing semicolon root");
    failures += expect_node(child_at(result.root, 0U), MYLITE_SQL_AST_SELECT_STATEMENT,
                            "trailing semicolon statement");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 123", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(result.root, MYLITE_SQL_AST_SCRIPT, "select 123 root");
    failures += expect_node(child_at(result.root, 0U), MYLITE_SQL_AST_SELECT_STATEMENT,
                            "select 123 statement");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_use_statements(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *first_use = NULL;
    const struct mylite_sql_ast_node *second_use = NULL;
    int failures = 0;

    failures += parse_sql("USE mylite_seed; USE `select`;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 2U, "use root");

    first_use = child_at(result.root, 0U);
    second_use = child_at(result.root, 1U);
    failures += expect_node(first_use, MYLITE_SQL_AST_USE_STATEMENT, "first use");
    failures += expect_span_text(child_at(first_use, 0U), "mylite_seed", "first schema");
    failures += expect_node(second_use, MYLITE_SQL_AST_USE_STATEMENT, "second use");
    failures += expect_span_text(child_at(second_use, 0U), "`select`", "second schema");

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_schema_lifecycle_statements(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *options = NULL;
    int failures = 0;

    failures += parse_sql("CREATE DATABASE IF NOT EXISTS mylite_app DEFAULT CHARACTER SET utf8mb4 "
                          "COLLATE utf8mb4_bin ENCRYPTION='N';",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    options = child_at(statement, 2U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT, "create schema");
    failures +=
        expect_node(child_at(statement, 0U), MYLITE_SQL_AST_IF_NOT_EXISTS, "create if not exists");
    failures += expect_span_text(child_at(statement, 1U), "mylite_app", "create schema name");
    failures += expect_child_count(options, 3U, "create options");
    failures += expect_schema_option(
        child_at(options, 0U), MYLITE_SQL_AST_SCHEMA_OPTION_CHARACTER_SET, "create charset option");
    failures += expect_schema_option(child_at(options, 1U), MYLITE_SQL_AST_SCHEMA_OPTION_COLLATE,
                                     "create collate option");
    failures += expect_schema_option(child_at(options, 2U), MYLITE_SQL_AST_SCHEMA_OPTION_ENCRYPTION,
                                     "create encryption option");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE DATABASE encryption DEFAULT CHARSET 'utf8mb4' "
                          "COLLATE 'utf8mb4_bin';",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    options = child_at(statement, 1U);
    failures += expect_span_text(child_at(statement, 0U), "encryption", "nonreserved schema name");
    failures += expect_child_count(options, 2U, "create charset alias options");
    failures += expect_literal(child_at(child_at(options, 0U), 0U), MYLITE_SQL_AST_LITERAL_STRING,
                               "quoted charset value");
    failures += expect_literal(child_at(child_at(options, 1U), 0U), MYLITE_SQL_AST_LITERAL_STRING,
                               "quoted collate value");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER SCHEMA DEFAULT COLLATE utf8mb4_0900_ai_ci READ ONLY = 1;",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    options = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT, "alter schema");
    failures += expect_child_count(statement, 1U, "alter omitted schema child count");
    failures += expect_child_count(options, 2U, "alter options");
    failures += expect_schema_option(child_at(options, 0U), MYLITE_SQL_AST_SCHEMA_OPTION_COLLATE,
                                     "alter collate option");
    failures += expect_schema_option(child_at(options, 1U), MYLITE_SQL_AST_SCHEMA_OPTION_READ_ONLY,
                                     "alter read only option");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP SCHEMA IF EXISTS `select`;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT, "drop schema");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_IF_EXISTS, "drop if exists");
    failures += expect_span_text(child_at(statement, 1U), "`select`", "drop schema name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW DATABASES; SHOW SCHEMAS;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(child_at(result.root, 0U), MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT,
                            "show databases");
    failures += expect_node(child_at(result.root, 1U), MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT,
                            "show schemas");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW DATABASES LIKE 'my%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_connection_charset_statements(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SET NAMES utf8mb4;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SET_NAMES_STATEMENT, "set names");
    failures += expect_span_text(child_at(statement, 0U), "utf8mb4", "set names charset");
    failures += expect_child_count(statement, 1U, "set names child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET NAMES 'latin1' COLLATE 'latin1_bin';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SET_NAMES_STATEMENT, "set names collate");
    failures += expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING,
                               "set names quoted charset");
    failures += expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING,
                               "set names quoted collation");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET NAMES binary; SET NAMES DEFAULT;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 2U, "set names script");
    failures +=
        expect_span_text(child_at(child_at(result.root, 0U), 0U), "binary", "set names binary");
    failures += expect_node(child_at(child_at(result.root, 1U), 0U), MYLITE_SQL_AST_DEFAULT,
                            "set names default");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET CHARACTER SET utf8mb3;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT, "set character set");
    failures += expect_span_text(child_at(statement, 0U), "utf8mb3", "set character set charset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET CHARACTER SET 'latin1';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT, "set character set");
    failures += expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING,
                               "set character set quoted charset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET CHARSET binary;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT, "set charset");
    failures += expect_span_text(child_at(statement, 0U), "binary", "set charset charset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET CHARSET DEFAULT;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT, "set charset default");
    failures +=
        expect_node(child_at(statement, 0U), MYLITE_SQL_AST_DEFAULT, "set charset default value");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SET NAMES DEFAULT COLLATE utf8mb4_bin;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SET SESSION sql_mode = 'ANSI_QUOTES';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_expression_list(void)
{
    enum { expected_select_item_count = 5 };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_item = NULL;
    const struct mylite_sql_ast_node *add = NULL;
    const struct mylite_sql_ast_node *multiply = NULL;
    int failures = 0;

    failures += parse_sql("SELECT 1 + 2 * 3, 'text', TRUE, FALSE, NULL FROM DUAL;",
                          MYLITE_SQL_PARSE_OK, &result);

    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_item = child_at(select_list, 0U);
    add = child_at(first_item, 0U);
    multiply = child_at(add, 1U);

    failures += expect_node(select, MYLITE_SQL_AST_SELECT_STATEMENT, "select statement");
    failures += expect_node(select_list, MYLITE_SQL_AST_SELECT_LIST, "select list");
    failures += expect_child_count(select_list, expected_select_item_count, "select list");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "from dual");

    failures += expect_node(add, MYLITE_SQL_AST_BINARY_EXPRESSION, "add expression");
    failures += expect_operator(add, MYLITE_SQL_AST_OPERATOR_ADD, "add expression");
    failures += expect_span_text(child_at(add, 0U), "1", "add left");
    failures += expect_node(multiply, MYLITE_SQL_AST_BINARY_EXPRESSION, "multiply expression");
    failures += expect_operator(multiply, MYLITE_SQL_AST_OPERATOR_MULTIPLY, "multiply expression");
    failures += expect_span_text(child_at(multiply, 0U), "2", "multiply left");
    failures += expect_span_text(child_at(multiply, 1U), "3", "multiply right");

    failures += expect_literal(child_at(child_at(select_list, 1U), 0U),
                               MYLITE_SQL_AST_LITERAL_STRING, "string literal");
    failures += expect_literal(child_at(child_at(select_list, 2U), 0U), MYLITE_SQL_AST_LITERAL_TRUE,
                               "true literal");
    failures += expect_literal(child_at(child_at(select_list, 3U), 0U),
                               MYLITE_SQL_AST_LITERAL_FALSE, "false literal");
    failures += expect_literal(child_at(child_at(select_list, 4U), 0U), MYLITE_SQL_AST_LITERAL_NULL,
                               "null literal");

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_information_schema_select(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *from_table = NULL;
    const struct mylite_sql_ast_node *qualified = NULL;
    int failures = 0;

    failures +=
        parse_sql("SELECT * FROM INFORMATION_SCHEMA.SCHEMATA;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_SELECT_STATEMENT, "information schema select");
    failures += expect_node(from_table, MYLITE_SQL_AST_FROM_TABLE, "information schema from");
    failures += expect_node(qualified, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "information schema table name");
    failures += expect_span_text(child_at(qualified, 0U), "INFORMATION_SCHEMA",
                                 "information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "SCHEMATA", "information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM `information_schema`.`STATISTICS`;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "`information_schema`",
                                 "quoted information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "`STATISTICS`",
                                 "quoted information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA;",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM INFORMATION_SCHEMA.SCHEMATA WHERE TRUE;",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_unary_and_parenthesized_expression(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *unary = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    const struct mylite_sql_ast_node *add = NULL;
    int failures = 0;

    failures += parse_sql("SELECT -(1 + 2);", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    unary = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(unary, 0U);
    add = child_at(parenthesized, 0U);

    failures += expect_node(unary, MYLITE_SQL_AST_UNARY_EXPRESSION, "negative expression");
    failures += expect_operator(unary, MYLITE_SQL_AST_OPERATOR_NEGATIVE, "negative expression");
    failures += expect_span_text(unary, "-(1 + 2)", "negative expression");
    failures += expect_node(parenthesized, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
                            "parenthesized expression");
    failures += expect_span_text(parenthesized, "(1 + 2)", "parenthesized expression");
    failures += expect_node(add, MYLITE_SQL_AST_BINARY_EXPRESSION, "parenthesized add");
    failures += expect_operator(add, MYLITE_SQL_AST_OPERATOR_ADD, "parenthesized add");
    failures += expect_span_text(child_at(add, 0U), "1", "parenthesized add left");
    failures += expect_span_text(child_at(add, 1U), "2", "parenthesized add right");

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_literal_categories(void)
{
    enum { expected_literal_item_count = 5 };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    int failures = 0;

    failures += parse_sql("SELECT 0xabc, b'10', .25, 1e+3, N'a';", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, expected_literal_item_count, "literal select list");
    failures += expect_literal(child_at(child_at(select_list, 0U), 0U), MYLITE_SQL_AST_LITERAL_HEX,
                               "hex literal");
    failures += expect_literal(child_at(child_at(select_list, 1U), 0U), MYLITE_SQL_AST_LITERAL_BIT,
                               "bit literal");
    failures += expect_literal(child_at(child_at(select_list, 2U), 0U),
                               MYLITE_SQL_AST_LITERAL_DECIMAL, "decimal literal");
    failures += expect_literal(child_at(child_at(select_list, 3U), 0U),
                               MYLITE_SQL_AST_LITERAL_FLOAT, "float literal");
    failures += expect_literal(child_at(child_at(select_list, 4U), 0U),
                               MYLITE_SQL_AST_LITERAL_NATIONAL_STRING, "national literal");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, 1U, "wildcard select list");
    failures += expect_node(child_at(child_at(select_list, 0U), 0U), MYLITE_SQL_AST_WILDCARD,
                            "wildcard item");

    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT *;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(child_at(result.root, 0U), 1U, "bare wildcard select");
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, 1U, "bare wildcard select list");
    failures += expect_node(child_at(child_at(select_list, 0U), 0U), MYLITE_SQL_AST_WILDCARD,
                            "bare wildcard item");

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_qualified_identifier_keyword_part(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *qualified = NULL;
    int failures = 0;

    failures += parse_sql("SELECT mydb.select;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    qualified = child_at(child_at(select_list, 0U), 0U);

    failures += expect_node(qualified, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "qualified identifier");
    failures += expect_span_text(child_at(qualified, 0U), "mydb", "qualified left");
    failures += expect_span_text(child_at(qualified, 1U), "select", "qualified right");

    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT mydb. /* comment */ select;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    qualified = child_at(child_at(select_list, 0U), 0U);

    failures += expect_node(qualified, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "commented qualified identifier");
    failures += expect_span_text(child_at(qualified, 0U), "mydb", "commented qualified left");
    failures += expect_span_text(child_at(qualified, 1U), "select", "commented qualified right");

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_comments_are_skipped(void)
{
    struct mylite_sql_parse_result result;
    int failures = 0;

    failures +=
        parse_sql("/* regular */ SELECT -- line\n1 /*+ hint */;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 1U, "comment root");
    failures +=
        expect_node(child_at(result.root, 0U), MYLITE_SQL_AST_SELECT_STATEMENT, "comment select");

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_syntax_errors(void)
{
    struct mylite_sql_parse_result result;
    int failures = 0;

    failures += parse_sql("SELECT FROM DUAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    if (result.error_token.text == NULL || result.error_token.length != strlen("FROM") ||
        memcmp(result.error_token.text, "FROM", strlen("FROM")) != 0) {
        fprintf(stderr, "expected syntax error token FROM\n");
        failures = 1;
    }
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1 +;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT INTERVAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1 WHERE TRUE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1, * FROM DUAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_lexer_errors(void)
{
    struct mylite_sql_parse_result result;
    int failures = 0;

    failures += parse_sql("SELECT 'unterminated", MYLITE_SQL_PARSE_LEXER_ERROR, &result);
    if (result.error_token.error != MYLITE_SQL_LEXER_ERROR_UNTERMINATED_STRING) {
        fprintf(stderr, "expected unterminated string lexer error, got %s\n",
                mylite_sql_lexer_error_name(result.error_token.error));
        failures = 1;
    }

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int parse_sql(const char *sql, enum mylite_sql_parse_status expected_status,
                     struct mylite_sql_parse_result *out_result)
{
    enum mylite_sql_parse_status actual = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = strlen(sql),
            .modes = 0U,
        },
        out_result);

    if (actual != expected_status) {
        fprintf(stderr, "parse '%s': expected %s, got %s\n", sql,
                mylite_sql_parse_status_name(expected_status),
                mylite_sql_parse_status_name(actual));
        return 1;
    }

    return 0;
}

static const struct mylite_sql_ast_node *child_at(const struct mylite_sql_ast_node *node,
                                                  size_t index)
{
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }

    child = node->first_child;
    for (size_t current = 0U; current < index && child != NULL; ++current) {
        child = child->next_sibling;
    }
    return child;
}

static int expect_node(const struct mylite_sql_ast_node *node,
                       enum mylite_sql_ast_node_kind expected_kind, const char *context)
{
    if (node == NULL) {
        fprintf(stderr, "%s: expected %s, got null\n", context,
                mylite_sql_ast_node_kind_name(expected_kind));
        return 1;
    }

    if (node->kind != expected_kind) {
        fprintf(stderr, "%s: expected %s, got %s\n", context,
                mylite_sql_ast_node_kind_name(expected_kind),
                mylite_sql_ast_node_kind_name(node->kind));
        return 1;
    }

    return 0;
}

static int expect_child_count(const struct mylite_sql_ast_node *node, size_t expected,
                              const char *context)
{
    size_t actual = mylite_sql_ast_node_child_count(node);

    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu children, got %zu\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_span_text(const struct mylite_sql_ast_node *node, const char *expected,
                            const char *context)
{
    size_t expected_length = strlen(expected);

    if (node == NULL) {
        fprintf(stderr, "%s: expected span '%s', got null node\n", context, expected);
        return 1;
    }

    if (node->span.length != expected_length ||
        (expected_length > 0U && memcmp(node->span.text, expected, expected_length) != 0)) {
        fprintf(stderr, "%s: expected span '%s', got '%.*s'\n", context, expected,
                (int)node->span.length, node->span.text == NULL ? "" : node->span.text);
        return 1;
    }

    return 0;
}

static int expect_literal(const struct mylite_sql_ast_node *node,
                          enum mylite_sql_ast_literal_kind expected, const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_LITERAL, context);

    if (node != NULL && node->literal_kind != expected) {
        fprintf(stderr, "%s: expected literal %s, got %s\n", context,
                mylite_sql_ast_literal_kind_name(expected),
                mylite_sql_ast_literal_kind_name(node->literal_kind));
        failures = 1;
    }

    return failures;
}

static int expect_operator(const struct mylite_sql_ast_node *node,
                           enum mylite_sql_ast_operator expected, const char *context)
{
    if (node == NULL) {
        fprintf(stderr, "%s: expected operator %s, got null node\n", context,
                mylite_sql_ast_operator_name(expected));
        return 1;
    }

    if (node->operator_kind != expected) {
        fprintf(stderr, "%s: expected operator %s, got %s\n", context,
                mylite_sql_ast_operator_name(expected),
                mylite_sql_ast_operator_name(node->operator_kind));
        return 1;
    }

    return 0;
}

static int expect_schema_option(const struct mylite_sql_ast_node *node,
                                enum mylite_sql_ast_schema_option expected, const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_SCHEMA_OPTION, context);

    if (node != NULL && node->schema_option != expected) {
        fprintf(stderr, "%s: expected schema option %s, got %s\n", context,
                mylite_sql_ast_schema_option_name(expected),
                mylite_sql_ast_schema_option_name(node->schema_option));
        failures = 1;
    }

    return failures;
}
