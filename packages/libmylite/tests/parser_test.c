#include "sql/mylite_ast.h"
#include "sql/mylite_parser.h"

#include <stdio.h>
#include <string.h>

static int test_empty_script(void);
static int test_use_statements(void);
static int test_select_expression_list(void);
static int test_current_database_functions(void);
static int test_current_user_identity_functions(void);
static int test_version_function(void);
static int test_row_count_function(void);
static int test_unary_and_parenthesized_expression(void);
static int test_literal_categories(void);
static int test_qualified_identifier_keyword_part(void);
static int test_schema_lifecycle_statements(void);
static int test_table_lifecycle_statements(void);
static int test_select_where_predicates(void);
static int test_select_order_limit_clauses(void);
static int test_delete_statement(void);
static int test_update_statement(void);
static int test_comments_are_skipped(void);
static int test_syntax_errors(void);
static int test_lexer_errors(void);
static int parse_sql(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    struct mylite_sql_parse_result *out_result
);
static const struct mylite_sql_ast_node *child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
);
static const struct mylite_sql_ast_node *first_child_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
);
static int expect_node(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind expected_kind,
    const char *context
);
static int expect_child_count(
    const struct mylite_sql_ast_node *node,
    size_t expected,
    const char *context
);
static int expect_span_text(
    const struct mylite_sql_ast_node *node,
    const char *expected,
    const char *context
);
static int expect_literal(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_literal_kind expected,
    const char *context
);
static int expect_operator(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_operator expected,
    const char *context
);
static int expect_true(int condition, const char *context);
static int expect_integer_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_integer_type expected_type,
    int expected_unsigned,
    const char *context
);
static int expect_nullability(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_nullability expected,
    const char *context
);
static int expect_order_direction(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_order_direction expected,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_empty_script();
    failures += test_use_statements();
    failures += test_select_expression_list();
    failures += test_current_database_functions();
    failures += test_current_user_identity_functions();
    failures += test_version_function();
    failures += test_row_count_function();
    failures += test_unary_and_parenthesized_expression();
    failures += test_literal_categories();
    failures += test_qualified_identifier_keyword_part();
    failures += test_schema_lifecycle_statements();
    failures += test_table_lifecycle_statements();
    failures += test_select_where_predicates();
    failures += test_select_order_limit_clauses();
    failures += test_delete_statement();
    failures += test_update_statement();
    failures += test_comments_are_skipped();
    failures += test_syntax_errors();
    failures += test_lexer_errors();

    return failures == 0 ? 0 : 1;
}

static int test_empty_script(void) {
    struct mylite_sql_parse_result result;
    int failures = 0;

    failures += parse_sql("", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(result.root, MYLITE_SQL_AST_SCRIPT, "empty root");
    failures += expect_child_count(result.root, 0U, "empty root");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 1U, "trailing semicolon root");
    failures += expect_node(
        child_at(result.root, 0U),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "trailing semicolon statement"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_use_statements(void) {
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

static int test_select_expression_list(void) {
    enum { expected_select_item_count = 5 };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_item = NULL;
    const struct mylite_sql_ast_node *add = NULL;
    const struct mylite_sql_ast_node *multiply = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT 1 + 2 * 3, 'text', TRUE, FALSE, NULL FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );

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

    failures += expect_literal(
        child_at(child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "string literal"
    );
    failures += expect_literal(
        child_at(child_at(select_list, 2U), 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "true literal"
    );
    failures += expect_literal(
        child_at(child_at(select_list, 3U), 0U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "false literal"
    );
    failures += expect_literal(
        child_at(child_at(select_list, 4U), 0U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "null literal"
    );

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_current_database_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    int failures = 0;

    failures += parse_sql("SELECT DATABASE(), SCHEMA();", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_DATABASE_FUNCTION, "database function");
    failures += expect_span_text(first_expression, "DATABASE()", "database function span");
    failures += expect_node(second_expression, MYLITE_SQL_AST_SCHEMA_FUNCTION, "schema function");
    failures += expect_span_text(second_expression, "SCHEMA()", "schema function span");
    failures += expect_child_count(first_expression, 0U, "database function child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT database(), schema() FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_DATABASE_FUNCTION, "lower database function");
    failures += expect_span_text(first_expression, "database()", "lower database span");
    failures +=
        expect_node(second_expression, MYLITE_SQL_AST_SCHEMA_FUNCTION, "lower schema function");
    failures += expect_span_text(second_expression, "schema()", "lower schema span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "function from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DATABASE (), (SCHEMA());", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_DATABASE_FUNCTION, "spaced database function");
    failures += expect_span_text(first_expression, "DATABASE ()", "spaced database span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized schema function"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_SCHEMA_FUNCTION,
        "wrapped schema function"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_current_user_identity_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *fourth_expression = NULL;
    int failures = 0;

    failures +=
        parse_sql("SELECT USER(), CURRENT_USER(), CURRENT_USER;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_USER_FUNCTION, "user function");
    failures += expect_span_text(first_expression, "USER()", "user function span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_CURRENT_USER_FUNCTION,
        "current user function"
    );
    failures += expect_span_text(second_expression, "CURRENT_USER()", "current user function span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_CURRENT_USER_FUNCTION,
        "bare current user function"
    );
    failures += expect_span_text(third_expression, "CURRENT_USER", "bare current user span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT user(), current_user FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_USER_FUNCTION, "lower user function");
    failures += expect_span_text(first_expression, "user()", "lower user span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_CURRENT_USER_FUNCTION,
        "lower current user keyword"
    );
    failures += expect_span_text(second_expression, "current_user", "lower current user span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "identity from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT SESSION_USER(), SYSTEM_USER(), session_user(), system_user() FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    fourth_expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_SESSION_USER_FUNCTION,
        "session user function"
    );
    failures += expect_span_text(first_expression, "SESSION_USER()", "session user span");
    failures +=
        expect_node(second_expression, MYLITE_SQL_AST_SYSTEM_USER_FUNCTION, "system user function");
    failures += expect_span_text(second_expression, "SYSTEM_USER()", "system user span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_SESSION_USER_FUNCTION,
        "lower session user function"
    );
    failures += expect_span_text(third_expression, "session_user()", "lower session user span");
    failures += expect_node(
        fourth_expression,
        MYLITE_SQL_AST_SYSTEM_USER_FUNCTION,
        "lower system user function"
    );
    failures += expect_span_text(fourth_expression, "system_user()", "lower system user span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "alias from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT SESSION_USER(/* inside */), SYSTEM_USER(/* inside */);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_SESSION_USER_FUNCTION,
        "commented session user function"
    );
    failures += expect_span_text(
        first_expression,
        "SESSION_USER(/* inside */)",
        "commented session user span"
    );
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_SYSTEM_USER_FUNCTION,
        "commented system user function"
    );
    failures += expect_span_text(
        second_expression,
        "SYSTEM_USER(/* inside */)",
        "commented system user span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT USER (), (CURRENT_USER);", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_USER_FUNCTION, "spaced user function");
    failures += expect_span_text(first_expression, "USER ()", "spaced user span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized current user keyword"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_CURRENT_USER_FUNCTION,
        "wrapped current user keyword"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT (SESSION_USER()), (System_User());", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized session user"
    );
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_SESSION_USER_FUNCTION,
        "wrapped session user"
    );
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized system user"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_SYSTEM_USER_FUNCTION,
        "wrapped system user"
    );
    failures += expect_span_text(second_expression, "(System_User())", "wrapped system user span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE user (user INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "user identifier table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE session_user (system_user INT, session_user INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    failures += expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "identity alias identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT USER;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare user identifier");
    failures += expect_span_text(first_expression, "USER", "bare user span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SESSION_USER, SYSTEM_USER;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare session user identifier");
    failures +=
        expect_node(second_expression, MYLITE_SQL_AST_IDENTIFIER, "bare system user identifier");
    failures += expect_span_text(first_expression, "SESSION_USER", "bare session user span");
    failures += expect_span_text(second_expression, "SYSTEM_USER", "bare system user span");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_version_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT VERSION(), Version(), version() FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_VERSION_FUNCTION, "version function");
    failures += expect_span_text(first_expression, "VERSION()", "version function span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_VERSION_FUNCTION,
        "mixed-case version function"
    );
    failures += expect_span_text(second_expression, "Version()", "mixed-case version span");
    failures +=
        expect_node(third_expression, MYLITE_SQL_AST_VERSION_FUNCTION, "lower version function");
    failures += expect_span_text(third_expression, "version()", "lower version span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "version from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT VERSION (), (VERSION());", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_VERSION_FUNCTION, "spaced version function");
    failures += expect_span_text(first_expression, "VERSION ()", "spaced version span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized version function"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_VERSION_FUNCTION,
        "wrapped version function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT VERSION(1), VERSION(NULL), VERSION(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR,
        "version integer argument error"
    );
    failures += expect_span_text(first_expression, "VERSION(1)", "version integer argument span");
    arguments = child_at(first_expression, 0U);
    failures +=
        expect_node(arguments, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, "version argument list");
    failures += expect_child_count(arguments, 1U, "version one argument count");
    failures += expect_literal(
        child_at(arguments, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "version integer argument"
    );
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR,
        "version null argument error"
    );
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR,
        "version multiple argument error"
    );
    arguments = child_at(third_expression, 0U);
    failures += expect_child_count(arguments, 2U, "version multiple argument count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE version (version INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures +=
        expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "version identifier table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT VERSION;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare version identifier");
    failures += expect_span_text(first_expression, "VERSION", "bare version span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT VERSION() LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_row_count_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT ROW_COUNT(), Row_Count(), row_count() FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_ROW_COUNT_FUNCTION, "row count function");
    failures += expect_span_text(first_expression, "ROW_COUNT()", "row count function span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_ROW_COUNT_FUNCTION,
        "mixed-case row count function"
    );
    failures += expect_span_text(second_expression, "Row_Count()", "mixed-case row count span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_ROW_COUNT_FUNCTION,
        "lower row count function"
    );
    failures += expect_span_text(third_expression, "row_count()", "lower row count span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "row count from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ROW_COUNT (), (ROW_COUNT());", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_ROW_COUNT_FUNCTION,
        "spaced row count function"
    );
    failures += expect_span_text(first_expression, "ROW_COUNT ()", "spaced row count span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized row count function"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_ROW_COUNT_FUNCTION,
        "wrapped row count function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE row_count (row_count INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures +=
        expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "row count identifier table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ROW_COUNT;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare row count identifier");
    failures += expect_span_text(first_expression, "ROW_COUNT", "bare row count span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ROW_COUNT(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT ROW_COUNT(NULL);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT ROW_COUNT(1, 2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT ROW_COUNT() LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_unary_and_parenthesized_expression(void) {
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
    failures += expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized expression"
    );
    failures += expect_span_text(parenthesized, "(1 + 2)", "parenthesized expression");
    failures += expect_node(add, MYLITE_SQL_AST_BINARY_EXPRESSION, "parenthesized add");
    failures += expect_operator(add, MYLITE_SQL_AST_OPERATOR_ADD, "parenthesized add");
    failures += expect_span_text(child_at(add, 0U), "1", "parenthesized add left");
    failures += expect_span_text(child_at(add, 1U), "2", "parenthesized add right");

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_literal_categories(void) {
    enum { expected_literal_item_count = 5 };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    int failures = 0;

    failures += parse_sql("SELECT 0xabc, b'10', .25, 1e+3, N'a';", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, expected_literal_item_count, "literal select list");
    failures += expect_literal(
        child_at(child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_LITERAL_HEX,
        "hex literal"
    );
    failures += expect_literal(
        child_at(child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_LITERAL_BIT,
        "bit literal"
    );
    failures += expect_literal(
        child_at(child_at(select_list, 2U), 0U),
        MYLITE_SQL_AST_LITERAL_DECIMAL,
        "decimal literal"
    );
    failures += expect_literal(
        child_at(child_at(select_list, 3U), 0U),
        MYLITE_SQL_AST_LITERAL_FLOAT,
        "float literal"
    );
    failures += expect_literal(
        child_at(child_at(select_list, 4U), 0U),
        MYLITE_SQL_AST_LITERAL_NATIONAL_STRING,
        "national literal"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, 1U, "wildcard select list");
    failures += expect_node(
        child_at(child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "wildcard item"
    );

    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT *;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(child_at(result.root, 0U), 1U, "bare wildcard select");
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, 1U, "bare wildcard select list");
    failures += expect_node(
        child_at(child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "bare wildcard item"
    );

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_qualified_identifier_keyword_part(void) {
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

    failures += expect_node(
        qualified,
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "commented qualified identifier"
    );
    failures += expect_span_text(child_at(qualified, 0U), "mydb", "commented qualified left");
    failures += expect_span_text(child_at(qualified, 1U), "select", "commented qualified right");

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_table_lifecycle_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *table_name = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE app.simple_lifecycle (id INT, amount BIGINT NOT NULL, "
        "flags INTEGER UNSIGNED NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_name = child_at(statement, 0U);
    columns = child_at(statement, 1U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "create table statement");
    failures +=
        expect_node(table_name, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "create qualified table");
    failures += expect_span_text(child_at(table_name, 0U), "app", "create schema name");
    failures += expect_span_text(child_at(table_name, 1U), "simple_lifecycle", "create table name");
    failures += expect_node(columns, MYLITE_SQL_AST_COLUMN_DEFINITION_LIST, "create column list");
    failures += expect_child_count(columns, 3U, "create column list");

    column = child_at(columns, 0U);
    failures += expect_span_text(child_at(column, 0U), "id", "first column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "first column type"
    );
    failures += expect_true(child_at(column, 2U) == NULL, "first column default nullability");

    column = child_at(columns, 1U);
    failures += expect_span_text(child_at(column, 0U), "amount", "second column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        0,
        "second column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "second column nullability"
    );

    column = child_at(columns, 2U);
    failures += expect_span_text(child_at(column, 0U), "flags", "third column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        1,
        "third column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "third column nullability"
    );

    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP TABLE app.simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_TABLE_STATEMENT, "drop table statement");
    failures += expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "drop target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE TABLE app.simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT, "truncate table statement");
    failures +=
        expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "truncate target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT, "bare truncate statement");
    failures +=
        expect_span_text(child_at(statement, 0U), "simple_lifecycle", "bare truncate target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLES FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_TABLES_STATEMENT, "show tables statement");
    failures += expect_span_text(child_at(statement, 0U), "app", "show tables schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLES IN app;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(result.root, 0U),
        MYLITE_SQL_AST_SHOW_TABLES_STATEMENT,
        "show tables in statement"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLES;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_TABLES_STATEMENT, "bare show tables");
    failures += expect_true(child_at(statement, 0U) == NULL, "bare show has no schema child");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "RENAME TABLE app.simple_lifecycle TO archive.renamed_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_RENAME_TABLE_STATEMENT, "rename table statement");
    failures += expect_child_count(statement, 2U, "rename table statement");
    failures += expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "rename source");
    failures +=
        expect_span_text(child_at(statement, 1U), "archive.renamed_lifecycle", "rename target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT INTO app.simple_lifecycle (amount, id) VALUES (+1, -2), (NULL, 3);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "insert statement");
    failures += expect_child_count(statement, 3U, "insert statement");
    failures += expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "insert target");
    failures +=
        expect_node(child_at(statement, 1U), MYLITE_SQL_AST_IDENTIFIER_LIST, "insert columns");
    failures += expect_child_count(child_at(statement, 1U), 2U, "insert columns");
    failures += expect_span_text(child_at(child_at(statement, 1U), 0U), "amount", "insert col 1");
    failures += expect_span_text(child_at(child_at(statement, 1U), 1U), "id", "insert col 2");
    failures += expect_node(child_at(statement, 2U), MYLITE_SQL_AST_INSERT_ROW_LIST, "insert rows");
    failures += expect_child_count(child_at(statement, 2U), 2U, "insert rows");
    failures += expect_node(
        child_at(child_at(statement, 2U), 0U),
        MYLITE_SQL_AST_INSERT_ROW,
        "first insert row"
    );
    failures += expect_operator(
        child_at(child_at(child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "positive insert value"
    );
    failures += expect_operator(
        child_at(child_at(child_at(statement, 2U), 0U), 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "negative insert value"
    );
    failures += expect_literal(
        child_at(child_at(child_at(statement, 2U), 1U), 0U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "null insert value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO simple_lifecycle VALUES (1);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "insert without columns");
    failures += expect_node(
        child_at(statement, 1U),
        MYLITE_SQL_AST_IDENTIFIER_LIST,
        "empty insert columns"
    );
    failures += expect_child_count(child_at(statement, 1U), 0U, "insert has no column list");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM app.simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SELECT_STATEMENT, "table wildcard select");
    failures +=
        expect_node(child_at(statement, 1U), MYLITE_SQL_AST_FROM_TABLE, "table select from");
    failures += expect_span_text(
        child_at(child_at(statement, 1U), 0U),
        "app.simple_lifecycle",
        "table select target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id, amount FROM simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SELECT_STATEMENT, "table projection select");
    failures += expect_node(child_at(statement, 1U), MYLITE_SQL_AST_FROM_TABLE, "projection from");
    failures += expect_span_text(
        child_at(child_at(child_at(statement, 0U), 0U), 0U),
        "id",
        "first projection"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(statement, 0U), 1U), 0U),
        "amount",
        "second projection"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_schema_lifecycle_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("CREATE DATABASE app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT, "create database");
    failures += expect_span_text(child_at(statement, 0U), "app", "create database name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE SCHEMA `select`;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT, "create schema");
    failures += expect_span_text(child_at(statement, 0U), "`select`", "create schema name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP DATABASE app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT, "drop database");
    failures += expect_span_text(child_at(statement, 0U), "app", "drop database name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP SCHEMA app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT, "drop schema");
    failures += expect_span_text(child_at(statement, 0U), "app", "drop schema name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW DATABASES;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT, "show databases");
    failures += expect_child_count(statement, 0U, "show databases child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW SCHEMAS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT, "show schemas");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_where_predicates(void) {
    static const struct {
        const char *sql;
        enum mylite_sql_ast_operator expected_operator;
        enum mylite_sql_ast_node_kind expected_kind;
    } cases[] = {
        {
            "SELECT id FROM simple_lifecycle WHERE id = 1;",
            MYLITE_SQL_AST_OPERATOR_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id <=> 1;",
            MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id <> 1;",
            MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id != 1;",
            MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id < -1;",
            MYLITE_SQL_AST_OPERATOR_LESS,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id <= +1;",
            MYLITE_SQL_AST_OPERATOR_LESS_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id > 1;",
            MYLITE_SQL_AST_OPERATOR_GREATER,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id >= 1;",
            MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id IS NULL;",
            MYLITE_SQL_AST_OPERATOR_IS_NULL,
            MYLITE_SQL_AST_IS_NULL_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id IS NOT NULL;",
            MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL,
            MYLITE_SQL_AST_IS_NULL_PREDICATE,
        },
    };
    struct mylite_sql_parse_result result;
    int failures = 0;

    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        const struct mylite_sql_ast_node *statement = NULL;
        const struct mylite_sql_ast_node *where_clause = NULL;
        const struct mylite_sql_ast_node *predicate = NULL;

        failures += parse_sql(cases[index].sql, MYLITE_SQL_PARSE_OK, &result);
        statement = child_at(result.root, 0U);
        where_clause = child_at(statement, 2U);
        predicate = child_at(where_clause, 0U);
        failures += expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "where clause");
        failures += expect_node(predicate, cases[index].expected_kind, "where predicate");
        failures += expect_operator(predicate, cases[index].expected_operator, "where operator");
        failures += expect_span_text(child_at(predicate, 0U), "id", "where predicate column");
        mylite_sql_parse_result_deinit(&result);
    }

    failures +=
        parse_sql("SELECT * FROM simple_lifecycle WHERE (id = +1);", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE simple_lifecycle.id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified predicate column"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_order_limit_clauses(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures +=
        parse_sql("SELECT * FROM simple_lifecycle ORDER BY id;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    order_clause = first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    failures += expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "order clause");
    failures += expect_span_text(child_at(order_clause, 0U), "id", "default order key");
    failures += expect_true(child_at(order_clause, 1U) == NULL, "default order has no direction");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id = 1 ORDER BY nn ASC LIMIT 2 OFFSET 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    order_clause = first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "asc order clause");
    failures += expect_span_text(child_at(order_clause, 0U), "nn", "asc order key");
    failures += expect_order_direction(
        child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_ASC,
        "asc order direction"
    );
    failures += expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "offset limit clause");
    failures += expect_span_text(child_at(limit_clause, 0U), "2", "offset limit row count");
    failures += expect_span_text(child_at(limit_clause, 1U), "1", "offset limit offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle ORDER BY simple_lifecycle.id DESC LIMIT 1, 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    order_clause = first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_node(
        child_at(order_clause, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified order key"
    );
    failures += expect_order_direction(
        child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "desc order direction"
    );
    failures += expect_span_text(child_at(limit_clause, 0U), "2", "comma limit row count");
    failures += expect_span_text(child_at(limit_clause, 1U), "1", "comma limit offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM simple_lifecycle LIMIT 0;", MYLITE_SQL_PARSE_OK, &result);
    limit_clause = first_child_kind(child_at(result.root, 0U), MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "simple limit clause");
    failures += expect_span_text(child_at(limit_clause, 0U), "0", "simple limit row count");
    failures += expect_true(child_at(limit_clause, 1U) == NULL, "simple limit has no offset");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_delete_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures += parse_sql("DELETE FROM app.simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DELETE_STATEMENT, "delete statement");
    failures += expect_child_count(statement, 1U, "delete statement target only");
    failures += expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "delete target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DELETE FROM simple_lifecycle WHERE id = +1 ORDER BY nn DESC LIMIT 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    where_clause = child_at(statement, 1U);
    order_clause = child_at(statement, 2U);
    limit_clause = child_at(statement, 3U);
    failures += expect_node(statement, MYLITE_SQL_AST_DELETE_STATEMENT, "qualified delete");
    failures += expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "delete where");
    failures += expect_operator(
        child_at(where_clause, 0U),
        MYLITE_SQL_AST_OPERATOR_EQUAL,
        "delete where operator"
    );
    failures += expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "delete order");
    failures += expect_span_text(child_at(order_clause, 0U), "nn", "delete order key");
    failures += expect_order_direction(
        child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "delete order direction"
    );
    failures += expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "delete limit");
    failures += expect_span_text(child_at(limit_clause, 0U), "2", "delete limit row count");
    failures += expect_true(child_at(limit_clause, 1U) == NULL, "delete limit has no offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DELETE FROM simple_lifecycle ORDER BY simple_lifecycle.id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    order_clause = first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    failures += expect_node(
        child_at(order_clause, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "delete qualified order key"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM simple_lifecycle LIMIT 0;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    limit_clause = first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "delete simple limit");
    failures += expect_span_text(child_at(limit_clause, 0U), "0", "delete simple limit row count");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_update_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *assignment_list = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures +=
        parse_sql("UPDATE app.simple_lifecycle SET amount = +1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment_list = child_at(statement, 1U);
    assignment = child_at(assignment_list, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_UPDATE_STATEMENT, "update statement");
    failures += expect_child_count(statement, 2U, "update statement required children");
    failures += expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "update target");
    failures += expect_node(
        assignment_list,
        MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST,
        "update assignment list"
    );
    failures += expect_child_count(assignment_list, 1U, "update single assignment");
    failures += expect_node(assignment, MYLITE_SQL_AST_UPDATE_ASSIGNMENT, "update assignment node");
    failures += expect_span_text(child_at(assignment, 0U), "amount", "update assignment target");
    failures += expect_operator(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "update positive assignment value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE simple_lifecycle SET amount = NULL WHERE id = +1 ORDER BY nn DESC LIMIT 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    assignment_list = child_at(statement, 1U);
    assignment = child_at(assignment_list, 0U);
    where_clause = child_at(statement, 2U);
    order_clause = child_at(statement, 3U);
    limit_clause = child_at(statement, 4U);
    failures += expect_node(statement, MYLITE_SQL_AST_UPDATE_STATEMENT, "qualified update");
    failures += expect_literal(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "update NULL assignment value"
    );
    failures += expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "update where");
    failures += expect_operator(
        child_at(where_clause, 0U),
        MYLITE_SQL_AST_OPERATOR_EQUAL,
        "update where operator"
    );
    failures += expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "update order");
    failures += expect_span_text(child_at(order_clause, 0U), "nn", "update order key");
    failures += expect_order_direction(
        child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "update order direction"
    );
    failures += expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "update limit");
    failures += expect_span_text(child_at(limit_clause, 0U), "2", "update limit row count");
    failures += expect_true(child_at(limit_clause, 1U) == NULL, "update limit has no offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE simple_lifecycle SET simple_lifecycle.amount = -1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    assignment_list = child_at(statement, 1U);
    assignment = child_at(assignment_list, 0U);
    failures += expect_node(
        child_at(assignment, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "update qualified assignment target"
    );
    failures += expect_operator(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "update negative assignment value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE simple_lifecycle SET amount = 1, nn = 2;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment_list = child_at(statement, 1U);
    failures += expect_child_count(assignment_list, 2U, "update multiple assignment list");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE simple_lifecycle SET amount = 1 LIMIT 0;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    limit_clause = first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "update simple limit");
    failures += expect_span_text(child_at(limit_clause, 0U), "0", "update simple limit row count");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_comments_are_skipped(void) {
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

static int test_syntax_errors(void) {
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

    failures += parse_sql("SELECT INTEGER;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1 WHERE TRUE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1, * FROM DUAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DATABASE(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SCHEMA(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT USER(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CURRENT_USER(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SESSION_USER(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SYSTEM_USER(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SESSION_USER ();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SYSTEM_USER ();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SESSION_USER/**/();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SYSTEM_USER/**/();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DATABASE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SCHEMA;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DATABASE() LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT USER() LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CURRENT_USER LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE DATABASE IF NOT EXISTS app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE DATABASE app DEFAULT CHARACTER SET utf8mb4;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE DATABASE a.b;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP DATABASE IF EXISTS app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW DATABASES LIKE 'app%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW DATABASES WHERE Database = 'app';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL DATABASES;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE t ();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE a.b.c (id INT);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE t (id VARCHAR(10));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE t (id INT(11));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE t (id INT) ENGINE=InnoDB;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP TABLE IF EXISTS t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP TABLE a, b;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE TABLE IF EXISTS t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE TABLE a, b;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE TABLE t WHERE id = 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE TABLE t LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE TEMPORARY TABLE t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("RENAME TABLE old_name new_name;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "RENAME TABLE old_name TO new_name, other TO target;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE old_name RENAME new_name;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES ('text');", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES (1 + 2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t SET id = 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT IGNORE INTO t VALUES (1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM t WHERE id = NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM t WHERE 1 = id;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM t WHERE id + 1 = 2;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM t WHERE id = '1';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM t WHERE id = 1 AND nn = 2;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM t WHERE id = 1 OR nn = 2;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM t WHERE ABS(id) = 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM t WHERE id = b'1';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM t JOIN other WHERE id = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM t WHERE id = 1 ORDER BY nn, id;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM t WHERE id = 1 LIMIT +1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t LIMIT +1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("DELETE FROM t LIMIT 1 OFFSET 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t LIMIT 1, 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t ORDER BY id, nn;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE LOW_PRIORITY FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET id = '1';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET id = 1 + 2;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET id = other;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET id = DEFAULT;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET id = 1 LIMIT +1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE t SET id = 1 LIMIT 1 OFFSET 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE t SET id = 1 LIMIT 1, 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE t SET id = 1 ORDER BY id, nn LIMIT 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE LOW_PRIORITY t SET id = 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t AS x SET id = 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE t PARTITION (p0) SET id = 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "WITH c AS (SELECT id FROM t) UPDATE t SET id = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_lexer_errors(void) {
    struct mylite_sql_parse_result result;
    int failures = 0;

    failures += parse_sql("SELECT 'unterminated", MYLITE_SQL_PARSE_LEXER_ERROR, &result);
    if (result.error_token.error != MYLITE_SQL_LEXER_ERROR_UNTERMINATED_STRING) {
        fprintf(
            stderr,
            "expected unterminated string lexer error, got %s\n",
            mylite_sql_lexer_error_name(result.error_token.error)
        );
        failures = 1;
    }

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int parse_sql(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    struct mylite_sql_parse_result *out_result
) {
    enum mylite_sql_parse_status actual = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = strlen(sql),
            .modes = 0U,
        },
        out_result
    );

    if (actual != expected_status) {
        fprintf(
            stderr,
            "parse '%s': expected %s, got %s\n",
            sql,
            mylite_sql_parse_status_name(expected_status),
            mylite_sql_parse_status_name(actual)
        );
        return 1;
    }

    return 0;
}

static const struct mylite_sql_ast_node *child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
) {
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

static const struct mylite_sql_ast_node *first_child_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }

    child = node->first_child;
    while (child != NULL) {
        if (child->kind == kind) {
            return child;
        }
        child = child->next_sibling;
    }

    return NULL;
}

static int expect_node(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind expected_kind,
    const char *context
) {
    if (node == NULL) {
        fprintf(
            stderr,
            "%s: expected %s, got null\n",
            context,
            mylite_sql_ast_node_kind_name(expected_kind)
        );
        return 1;
    }

    if (node->kind != expected_kind) {
        fprintf(
            stderr,
            "%s: expected %s, got %s\n",
            context,
            mylite_sql_ast_node_kind_name(expected_kind),
            mylite_sql_ast_node_kind_name(node->kind)
        );
        return 1;
    }

    return 0;
}

static int expect_child_count(
    const struct mylite_sql_ast_node *node,
    size_t expected,
    const char *context
) {
    size_t actual = mylite_sql_ast_node_child_count(node);

    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu children, got %zu\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_span_text(
    const struct mylite_sql_ast_node *node,
    const char *expected,
    const char *context
) {
    size_t expected_length = strlen(expected);

    if (node == NULL) {
        fprintf(stderr, "%s: expected span '%s', got null node\n", context, expected);
        return 1;
    }

    if (node->span.length != expected_length ||
        (expected_length > 0U && memcmp(node->span.text, expected, expected_length) != 0)) {
        fprintf(
            stderr,
            "%s: expected span '%s', got '%.*s'\n",
            context,
            expected,
            (int)node->span.length,
            node->span.text == NULL ? "" : node->span.text
        );
        return 1;
    }

    return 0;
}

static int expect_literal(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_literal_kind expected,
    const char *context
) {
    int failures = expect_node(node, MYLITE_SQL_AST_LITERAL, context);

    if (node != NULL && mylite_sql_ast_node_literal_kind(node) != expected) {
        fprintf(
            stderr,
            "%s: expected literal %s, got %s\n",
            context,
            mylite_sql_ast_literal_kind_name(expected),
            mylite_sql_ast_literal_kind_name(mylite_sql_ast_node_literal_kind(node))
        );
        failures = 1;
    }

    return failures;
}

static int expect_operator(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_operator expected,
    const char *context
) {
    if (node == NULL) {
        fprintf(
            stderr,
            "%s: expected operator %s, got null node\n",
            context,
            mylite_sql_ast_operator_name(expected)
        );
        return 1;
    }

    if (mylite_sql_ast_node_operator(node) != expected) {
        fprintf(
            stderr,
            "%s: expected operator %s, got %s\n",
            context,
            mylite_sql_ast_operator_name(expected),
            mylite_sql_ast_operator_name(mylite_sql_ast_node_operator(node))
        );
        return 1;
    }

    return 0;
}

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_integer_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_integer_type expected_type,
    int expected_unsigned,
    const char *context
) {
    enum mylite_sql_ast_integer_type actual_type = mylite_sql_ast_node_integer_type(node);
    int actual_unsigned = mylite_sql_ast_node_integer_type_is_unsigned(node);

    if (actual_type != expected_type || actual_unsigned != expected_unsigned) {
        fprintf(
            stderr,
            "%s: expected %s unsigned=%d, got %s unsigned=%d\n",
            context,
            mylite_sql_ast_integer_type_name(expected_type),
            expected_unsigned,
            mylite_sql_ast_integer_type_name(actual_type),
            actual_unsigned
        );
        return 1;
    }

    return 0;
}

static int expect_nullability(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_nullability expected,
    const char *context
) {
    enum mylite_sql_ast_nullability actual = mylite_sql_ast_node_nullability(node);

    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %s, got %s\n",
            context,
            mylite_sql_ast_nullability_name(expected),
            mylite_sql_ast_nullability_name(actual)
        );
        return 1;
    }

    return 0;
}

static int expect_order_direction(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_order_direction expected,
    const char *context
) {
    enum mylite_sql_ast_order_direction actual = mylite_sql_ast_node_order_direction(node);

    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected order direction %s, got %s\n",
            context,
            mylite_sql_ast_order_direction_name(expected),
            mylite_sql_ast_order_direction_name(actual)
        );
        return 1;
    }

    return 0;
}
