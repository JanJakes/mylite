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
static int test_connection_id_function(void);
static int test_row_count_function(void);
static int test_count_star_aggregate(void);
static int test_unary_and_parenthesized_expression(void);
static int test_literal_categories(void);
static int test_qualified_identifier_keyword_part(void);
static int test_schema_lifecycle_statements(void);
static int test_table_lifecycle_statements(void);
static int test_show_columns_introspection_statements(void);
static int test_show_triggers_empty_introspection_statements(void);
static int test_show_events_empty_introspection_statements(void);
static int test_show_open_tables_empty_introspection_statements(void);
static int test_show_routine_status_empty_introspection_statements(void);
static int test_show_processlist_introspection_statements(void);
static int test_show_warnings_diagnostics_statements(void);
static int test_show_index_empty_introspection_statements(void);
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
    failures += test_connection_id_function();
    failures += test_row_count_function();
    failures += test_count_star_aggregate();
    failures += test_unary_and_parenthesized_expression();
    failures += test_literal_categories();
    failures += test_qualified_identifier_keyword_part();
    failures += test_schema_lifecycle_statements();
    failures += test_table_lifecycle_statements();
    failures += test_show_columns_introspection_statements();
    failures += test_show_triggers_empty_introspection_statements();
    failures += test_show_events_empty_introspection_statements();
    failures += test_show_open_tables_empty_introspection_statements();
    failures += test_show_routine_status_empty_introspection_statements();
    failures += test_show_processlist_introspection_statements();
    failures += test_show_warnings_diagnostics_statements();
    failures += test_show_index_empty_introspection_statements();
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

static int test_connection_id_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT CONNECTION_ID(), Connection_Id(), connection_id() FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CONNECTION_ID_FUNCTION,
        "connection id function"
    );
    failures += expect_span_text(first_expression, "CONNECTION_ID()", "connection id span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_CONNECTION_ID_FUNCTION,
        "mixed-case connection id function"
    );
    failures +=
        expect_span_text(second_expression, "Connection_Id()", "mixed-case connection id span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_CONNECTION_ID_FUNCTION,
        "lower connection id function"
    );
    failures += expect_span_text(third_expression, "connection_id()", "lower connection id span");
    failures +=
        expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "connection id from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT CONNECTION_ID (), CONNECTION_ID/**/(), CONNECTION_ID(/* inside */), "
        "(CONNECTION_ID());",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CONNECTION_ID_FUNCTION,
        "spaced connection id function"
    );
    failures += expect_span_text(first_expression, "CONNECTION_ID ()", "spaced connection id span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_CONNECTION_ID_FUNCTION,
        "comment-before-paren connection id function"
    );
    failures += expect_span_text(
        second_expression,
        "CONNECTION_ID/**/()",
        "comment-before-paren connection id span"
    );
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_CONNECTION_ID_FUNCTION,
        "commented connection id function"
    );
    failures += expect_span_text(
        third_expression,
        "CONNECTION_ID(/* inside */)",
        "commented connection id span"
    );
    failures += expect_node(
        child_at(child_at(select_list, 3U), 0U),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized connection id function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT CONNECTION_ID(1), CONNECTION_ID(NULL), CONNECTION_ID(1, 2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR,
        "connection id integer argument error"
    );
    arguments = child_at(first_expression, 0U);
    failures += expect_child_count(arguments, 1U, "connection id one argument count");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR,
        "connection id null argument error"
    );
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR,
        "connection id multiple argument error"
    );
    arguments = child_at(third_expression, 0U);
    failures += expect_child_count(arguments, 2U, "connection id multiple argument count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE connection_id (connection_id INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "connection id identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CONNECTION_ID;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare connection id identifier");
    failures += expect_span_text(first_expression, "CONNECTION_ID", "bare connection id span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT CONNECTION_ID() LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

static int test_count_star_aggregate(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    int failures = 0;

    failures +=
        parse_sql("SELECT COUNT(*), count(*), Count( * ) FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_COUNT_STAR_FUNCTION, "count star function");
    failures += expect_span_text(first_expression, "COUNT(*)", "count star function span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "lower count star function"
    );
    failures += expect_span_text(second_expression, "count(*)", "lower count star span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "mixed count star function"
    );
    failures += expect_span_text(third_expression, "Count( * )", "mixed count star span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "count from dual");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT COUNT(/* inside */*) FROM t WHERE id = 1;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "commented count star function"
    );
    failures +=
        expect_span_text(first_expression, "COUNT(/* inside */*)", "commented count star span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "count from table");
    failures += expect_node(child_at(select, 2U), MYLITE_SQL_AST_WHERE_CLAUSE, "count where");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT (COUNT(*));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized count star function"
    );
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "wrapped count star function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE count (count INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures +=
        expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "count identifier table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT COUNT;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare count identifier");
    failures += expect_span_text(first_expression, "COUNT", "bare count span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT COUNT (*) FROM DUAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT/**/(*) FROM DUAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT(t.*) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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
    const struct mylite_sql_ast_node *table_options = NULL;
    const struct mylite_sql_ast_node *engine_option = NULL;
    const struct mylite_sql_ast_node *charset_option = NULL;
    const struct mylite_sql_ast_node *collation_option = NULL;
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

    failures += parse_sql(
        "CREATE TABLE engine_forms (id INT) ENGINE=InnoDB;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_options = child_at(statement, 2U);
    engine_option = child_at(table_options, 0U);
    failures += expect_child_count(statement, 3U, "engine create child count");
    failures +=
        expect_node(table_options, MYLITE_SQL_AST_TABLE_OPTION_LIST, "create table options");
    failures += expect_child_count(table_options, 1U, "engine option list child count");
    failures += expect_node(
        engine_option,
        MYLITE_SQL_AST_TABLE_ENGINE_OPTION,
        "create table engine option"
    );
    failures += expect_span_text(child_at(engine_option, 0U), "InnoDB", "engine option name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE engine_space (id INT) ENGINE InnoDB;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE engine_string (id INT) ENGINE='InnoDB';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_options = child_at(statement, 2U);
    engine_option = child_at(table_options, 0U);
    failures +=
        expect_literal(child_at(engine_option, 0U), MYLITE_SQL_AST_LITERAL_STRING, "string engine");
    failures += expect_span_text(child_at(engine_option, 0U), "'InnoDB'", "string engine name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE engine_quoted (id INT) ENGINE=`InnoDB`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE charset_options (id INT) DEFAULT CHARSET=utf8mb4 "
        "COLLATE='utf8mb4_0900_ai_ci';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_options = child_at(statement, 2U);
    charset_option = child_at(table_options, 0U);
    collation_option = child_at(table_options, 1U);
    failures += expect_child_count(table_options, 2U, "charset option list child count");
    failures += expect_node(
        charset_option,
        MYLITE_SQL_AST_TABLE_CHARSET_OPTION,
        "create table charset option"
    );
    failures += expect_span_text(child_at(charset_option, 0U), "utf8mb4", "charset option name");
    failures += expect_node(
        collation_option,
        MYLITE_SQL_AST_TABLE_COLLATION_OPTION,
        "create table collation option"
    );
    failures += expect_literal(
        child_at(collation_option, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "string collation"
    );
    failures += expect_span_text(
        child_at(collation_option, 0U),
        "'utf8mb4_0900_ai_ci'",
        "collation option name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE character_set_space (id INT) DEFAULT CHARACTER SET utf8mb4;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE charset_space (id INT) CHARSET utf8mb4;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE default_collate (id INT) DEFAULT COLLATE utf8mb4_0900_ai_ci;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE combined_options (id INT) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 "
        "COLLATE=utf8mb4_0900_ai_ci;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_options = child_at(statement, 2U);
    failures += expect_child_count(table_options, 3U, "combined option list child count");
    failures += expect_node(
        child_at(table_options, 0U),
        MYLITE_SQL_AST_TABLE_ENGINE_OPTION,
        "combined engine option"
    );
    failures += expect_node(
        child_at(table_options, 1U),
        MYLITE_SQL_AST_TABLE_CHARSET_OPTION,
        "combined charset option"
    );
    failures += expect_node(
        child_at(table_options, 2U),
        MYLITE_SQL_AST_TABLE_COLLATION_OPTION,
        "combined collation option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE reverse_options (id INT) COLLATE=utf8mb4_0900_ai_ci "
        "DEFAULT CHARSET=`utf8mb4` ENGINE=InnoDB;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE status (status INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE collation (collation INT);", MYLITE_SQL_PARSE_OK, &result);
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

    failures += parse_sql("SHOW TABLES LIKE 'a%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_TABLES_STATEMENT, "show tables like");
    failures +=
        expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING, "tables like");
    failures += expect_span_text(child_at(statement, 0U), "'a%'", "show tables like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLES FROM app LIKE 'a\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_TABLES_STATEMENT, "show tables schema like");
    failures += expect_span_text(child_at(statement, 0U), "app", "show tables like schema");
    failures +=
        expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING, "schema like");
    failures += expect_span_text(child_at(statement, 1U), "'a\\_%'", "schema like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLE STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT,
        "bare show table status"
    );
    failures +=
        expect_true(child_at(statement, 0U) == NULL, "bare table status has no schema child");
    failures += expect_true(child_at(statement, 1U) == NULL, "bare table status has no like child");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLE STATUS FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT,
        "show table status from"
    );
    failures += expect_span_text(child_at(statement, 0U), "app", "show table status schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLE STATUS IN app LIKE 'a\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT,
        "show table status in like"
    );
    failures += expect_span_text(child_at(statement, 0U), "app", "show table status like schema");
    failures +=
        expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING, "status like");
    failures += expect_span_text(child_at(statement, 1U), "'a\\_%'", "status like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLE STATUS LIKE 'a%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT,
        "show table status like"
    );
    failures +=
        expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING, "status bare like");
    failures += expect_span_text(child_at(statement, 0U), "'a%'", "status bare like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CHARACTER SET;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT, "show character set");
    failures += expect_child_count(statement, 0U, "show character set child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CHARSET LIKE 'utf8%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT, "show charset like");
    failures +=
        expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING, "charset like");
    failures += expect_span_text(child_at(statement, 0U), "'utf8%'", "charset like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLLATION;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT, "show collation");
    failures += expect_child_count(statement, 0U, "show collation child count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW COLLATION LIKE 'utf8mb4\\_0900\\_ai\\_ci';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT, "show collation like");
    failures +=
        expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING, "collation like");
    failures += expect_span_text(
        child_at(statement, 0U),
        "'utf8mb4\\_0900\\_ai\\_ci'",
        "collation like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ENGINES;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT, "show engines");
    failures += expect_child_count(statement, 0U, "show engines child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW STORAGE ENGINES;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT, "show storage engines");
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
        "ALTER TABLE app.simple_lifecycle RENAME TO archive.renamed_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT,
        "alter table rename to statement"
    );
    failures += expect_child_count(statement, 2U, "alter table rename statement");
    failures +=
        expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "alter rename source");
    failures += expect_span_text(
        child_at(statement, 1U),
        "archive.renamed_lifecycle",
        "alter rename target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle RENAME renamed_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT,
        "alter table bare rename statement"
    );
    failures +=
        expect_span_text(child_at(statement, 1U), "renamed_lifecycle", "bare rename target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle RENAME AS renamed_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT,
        "alter table rename as statement"
    );
    failures += expect_span_text(child_at(statement, 1U), "renamed_lifecycle", "rename as target");
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

    failures += parse_sql(
        "INSERT INTO app.simple_lifecycle SET id = +1, amount = NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_SET_STATEMENT, "insert set statement");
    failures += expect_child_count(statement, 2U, "insert set statement");
    failures +=
        expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "insert set target");
    failures += expect_node(
        child_at(statement, 1U),
        MYLITE_SQL_AST_INSERT_ASSIGNMENT_LIST,
        "insert set assignments"
    );
    failures += expect_child_count(child_at(statement, 1U), 2U, "insert set assignment count");
    failures += expect_node(
        child_at(child_at(statement, 1U), 0U),
        MYLITE_SQL_AST_INSERT_ASSIGNMENT,
        "insert set first assignment"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(statement, 1U), 0U), 0U),
        "id",
        "insert set first target"
    );
    failures += expect_operator(
        child_at(child_at(child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "insert set first value"
    );
    failures += expect_literal(
        child_at(child_at(child_at(statement, 1U), 1U), 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "insert set second value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT simple_lifecycle SET id = -1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_SET_STATEMENT, "insert set no into");
    failures += expect_span_text(child_at(statement, 0U), "simple_lifecycle", "no into target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT INTO simple_lifecycle SET simple_lifecycle.id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        child_at(child_at(child_at(statement, 1U), 0U), 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "insert set qualified target"
    );
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

    failures += parse_sql("SHOW DATABASES LIKE 'app%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT, "show databases like");
    failures +=
        expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING, "databases like");
    failures += expect_span_text(child_at(statement, 0U), "'app%'", "databases like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW SCHEMAS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT, "show schemas");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE DATABASE app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_CREATE_DATABASE_STATEMENT,
        "show create database"
    );
    failures += expect_child_count(statement, 1U, "show create database child count");
    failures += expect_span_text(child_at(statement, 0U), "app", "show create database name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE SCHEMA `select`;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_CREATE_DATABASE_STATEMENT, "show create schema");
    failures += expect_span_text(child_at(statement, 0U), "`select`", "show create schema name");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_columns_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW COLUMNS FROM numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "show columns statement");
    failures += expect_child_count(statement, 1U, "show columns child count");
    failures += expect_span_text(child_at(statement, 0U), "numbers", "show columns table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLUMNS IN app.numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "show columns in statement");
    failures += expect_child_count(statement, 1U, "show columns in child count");
    failures +=
        expect_span_text(child_at(statement, 0U), "app.numbers", "show columns qualified table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FIELDS FROM numbers IN app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "show fields statement");
    failures += expect_child_count(statement, 2U, "show fields explicit schema child count");
    failures += expect_span_text(child_at(statement, 0U), "numbers", "show fields table");
    failures += expect_span_text(child_at(statement, 1U), "app", "show fields schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FIELDS IN numbers FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT,
        "show fields in from statement"
    );
    failures += expect_child_count(statement, 2U, "show fields in from child count");
    failures += expect_span_text(child_at(statement, 0U), "numbers", "show fields in table");
    failures += expect_span_text(child_at(statement, 1U), "app", "show fields from schema");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW COLUMNS FROM app.numbers FROM other;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT,
        "show columns qualified explicit schema"
    );
    failures += expect_child_count(statement, 2U, "show columns qualified explicit child count");
    failures +=
        expect_span_text(child_at(statement, 0U), "app.numbers", "show columns qualified table");
    failures += expect_span_text(child_at(statement, 1U), "other", "show columns trailing schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLUMNS FROM numbers LIKE 'i%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "show columns like");
    failures += expect_child_count(statement, 2U, "show columns like child count");
    failures += expect_span_text(child_at(statement, 0U), "numbers", "show columns like table");
    failures +=
        expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING, "columns like");
    failures += expect_span_text(child_at(statement, 1U), "'i%'", "show columns like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW FIELDS FROM app.numbers FROM other LIKE 'i\\_1';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "show fields qualified like");
    failures += expect_child_count(statement, 3U, "show fields qualified like child count");
    failures += expect_span_text(child_at(statement, 0U), "app.numbers", "fields like table");
    failures += expect_span_text(child_at(statement, 1U), "other", "fields like schema");
    failures +=
        expect_literal(child_at(statement, 2U), MYLITE_SQL_AST_LITERAL_STRING, "fields like");
    failures += expect_span_text(child_at(statement, 2U), "'i\\_1'", "fields like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DESCRIBE app.numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "describe table");
    failures += expect_span_text(child_at(statement, 0U), "app.numbers", "describe target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DESC numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "desc table");
    failures += expect_span_text(child_at(statement, 0U), "numbers", "desc target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN app.numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "explain table");
    failures += expect_span_text(child_at(statement, 0U), "app.numbers", "explain target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE TABLE app.numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT, "show create table");
    failures += expect_child_count(statement, 1U, "show create child count");
    failures += expect_span_text(child_at(statement, 0U), "app.numbers", "show create target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE columns (fields INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "columns table name");
    failures += expect_span_text(child_at(statement, 0U), "columns", "columns identifier");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_triggers_empty_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW TRIGGERS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT, "show triggers");
    failures += expect_child_count(statement, 0U, "show triggers child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL TRIGGERS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT, "show full triggers");
    failures += expect_child_count(statement, 0U, "show full triggers child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TRIGGERS FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT, "show triggers from");
    failures += expect_child_count(statement, 1U, "show triggers from child count");
    failures += expect_span_text(child_at(statement, 0U), "app", "show triggers schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TRIGGERS IN app LIKE 'account\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT, "show triggers in like");
    failures += expect_child_count(statement, 2U, "show triggers in like child count");
    failures += expect_span_text(child_at(statement, 0U), "app", "show triggers like schema");
    failures +=
        expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING, "triggers like");
    failures += expect_span_text(child_at(statement, 1U), "'account\\_%'", "triggers like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL TRIGGERS LIKE 'account';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT, "show full triggers like");
    failures += expect_child_count(statement, 1U, "show full triggers like child count");
    failures += expect_literal(
        child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "full triggers like"
    );
    failures +=
        expect_span_text(child_at(statement, 0U), "'account'", "full triggers like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE triggers (full INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "triggers table");
    failures += expect_span_text(child_at(statement, 0U), "triggers", "triggers identifier");
    failures += expect_span_text(
        child_at(child_at(child_at(statement, 1U), 0U), 0U),
        "full",
        "full identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TRIGGER FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW EXTENDED TRIGGERS FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW TRIGGERS FROM app WHERE `Table` = 'account';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW TRIGGERS FROM app LIKE 'account' WHERE `Table` = 'account';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_events_empty_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW EVENTS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT, "show events");
    failures += expect_child_count(statement, 0U, "show events child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EVENTS FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT, "show events from");
    failures += expect_child_count(statement, 1U, "show events from child count");
    failures += expect_span_text(child_at(statement, 0U), "app", "show events schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EVENTS IN app LIKE 'daily\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT, "show events in like");
    failures += expect_child_count(statement, 2U, "show events in like child count");
    failures += expect_span_text(child_at(statement, 0U), "app", "show events like schema");
    failures +=
        expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING, "events like");
    failures += expect_span_text(child_at(statement, 1U), "'daily\\_%'", "events like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EVENTS LIKE 'daily%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT, "show events like");
    failures += expect_child_count(statement, 1U, "show events like child count");
    failures +=
        expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING, "events bare like");
    failures += expect_span_text(child_at(statement, 0U), "'daily%'", "events bare like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE events (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "events table");
    failures += expect_span_text(child_at(statement, 0U), "events", "events identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL EVENTS FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EXTENDED EVENTS FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EVENT FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW EVENTS FROM app WHERE Name = 'daily_event';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW EVENTS FROM app LIKE 'daily%' WHERE Name = 'daily_event';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_open_tables_empty_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW OPEN TABLES;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT, "show open tables");
    failures += expect_child_count(statement, 0U, "show open tables child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW OPEN TABLES FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT, "show open tables from");
    failures += expect_child_count(statement, 1U, "show open tables from child count");
    failures += expect_span_text(child_at(statement, 0U), "app", "show open tables schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW OPEN TABLES IN app LIKE 'open\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT,
        "show open tables in like"
    );
    failures += expect_child_count(statement, 2U, "show open tables in like child count");
    failures += expect_span_text(child_at(statement, 0U), "app", "show open tables like schema");
    failures +=
        expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING, "open tables like");
    failures += expect_span_text(child_at(statement, 1U), "'open\\_%'", "open tables like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW OPEN TABLES LIKE 'open%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT, "show open tables like");
    failures += expect_child_count(statement, 1U, "show open tables like child count");
    failures += expect_literal(
        child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "open tables bare like"
    );
    failures +=
        expect_span_text(child_at(statement, 0U), "'open%'", "open tables bare like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE open (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "open table");
    failures += expect_span_text(child_at(statement, 0U), "open", "open identifier");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW FULL OPEN TABLES FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW EXTENDED OPEN TABLES FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW OPEN TABLES FROM app WHERE `Table` = 'open_table';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW OPEN TABLES FROM app LIKE 'open%' WHERE `Table` = 'open_table';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW OPEN TABLES FROM app ORDER BY `Table`;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW OPEN TABLES FROM app LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW OPEN TABLES FROM app.extra;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW OPEN TABLES FROM app LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW OPEN TABLES FROM app LIKE NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_routine_status_empty_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW PROCEDURE STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_PROCEDURE_STATUS_STATEMENT,
        "show procedure status"
    );
    failures += expect_child_count(statement, 0U, "show procedure status child count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW PROCEDURE STATUS LIKE 'routine\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_PROCEDURE_STATUS_STATEMENT,
        "show procedure status like"
    );
    failures += expect_child_count(statement, 1U, "show procedure status like child count");
    failures += expect_literal(
        child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "procedure status like"
    );
    failures +=
        expect_span_text(child_at(statement, 0U), "'routine\\_%'", "procedure like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FUNCTION STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_FUNCTION_STATUS_STATEMENT,
        "show function status"
    );
    failures += expect_child_count(statement, 0U, "show function status child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FUNCTION STATUS LIKE 'routine%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_FUNCTION_STATUS_STATEMENT,
        "show function status like"
    );
    failures += expect_child_count(statement, 1U, "show function status like child count");
    failures += expect_literal(
        child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "function status like"
    );
    failures += expect_span_text(child_at(statement, 0U), "'routine%'", "function like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE status (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "status table");
    failures += expect_span_text(child_at(statement, 0U), "status", "status identifier");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW PROCEDURE STATUS FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FUNCTION STATUS IN app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL PROCEDURE STATUS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EXTENDED FUNCTION STATUS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW PROCEDURE STATUS WHERE Name = 'routine_proc';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW PROCEDURE STATUS LIKE 'routine%' WHERE Name = 'routine_proc';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW PROCEDURE STATUS ORDER BY Name;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FUNCTION STATUS LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW PROCEDURE STATUS LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW FUNCTION STATUS LIKE NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW PROCEDURE STATUS LIKE N'routine';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW FUNCTION STATUS LIKE _utf8mb4'routine';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_processlist_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW PROCESSLIST;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_PROCESSLIST_STATEMENT, "show processlist");
    failures += expect_child_count(statement, 0U, "show processlist child count");
    failures += expect_span_text(statement, "SHOW PROCESSLIST", "show processlist span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL PROCESSLIST;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_FULL_PROCESSLIST_STATEMENT,
        "show full processlist"
    );
    failures += expect_child_count(statement, 0U, "show full processlist child count");
    failures += expect_span_text(statement, "SHOW FULL PROCESSLIST", "show full processlist span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL /* keep */ PROCESSLIST;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_FULL_PROCESSLIST_STATEMENT,
        "show full processlist with comment"
    );
    failures += expect_span_text(
        statement,
        "SHOW FULL /* keep */ PROCESSLIST",
        "show full processlist commented span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE processlist (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "processlist table");
    failures += expect_span_text(child_at(statement, 0U), "processlist", "processlist identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW PROCESSLIST LIKE 'root%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW PROCESSLIST WHERE Id > 0;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW PROCESSLIST ORDER BY Id;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW PROCESSLIST LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL PROCESSLIST LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW PROCESSLIST FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW PROCESSLIST IN app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EXTENDED PROCESSLIST;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_warnings_diagnostics_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *limit = NULL;
    int failures = 0;

    failures += parse_sql("SHOW WARNINGS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT, "show warnings");
    failures += expect_child_count(statement, 0U, "show warnings child count");
    failures += expect_span_text(statement, "SHOW WARNINGS", "show warnings span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW WARNINGS LIMIT 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    limit = child_at(statement, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT, "show warnings limit");
    failures += expect_child_count(statement, 1U, "show warnings limit child count");
    failures += expect_node(limit, MYLITE_SQL_AST_LIMIT_CLAUSE, "show warnings limit clause");
    failures += expect_span_text(child_at(limit, 0U), "1", "show warnings limit row count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW WARNINGS LIMIT 2, 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    limit = child_at(statement, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT, "show warnings comma");
    failures += expect_child_count(limit, 2U, "show warnings comma child count");
    failures += expect_span_text(child_at(limit, 0U), "1", "show warnings comma row count");
    failures += expect_span_text(child_at(limit, 1U), "2", "show warnings comma offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW WARNINGS LIMIT 1 OFFSET 2;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    limit = child_at(statement, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT, "show warnings offset");
    failures += expect_child_count(limit, 2U, "show warnings offset child count");
    failures += expect_span_text(child_at(limit, 0U), "1", "show warnings offset row count");
    failures += expect_span_text(child_at(limit, 1U), "2", "show warnings offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COUNT(*) WARNINGS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_COUNT_WARNINGS_STATEMENT, "show count warnings");
    failures += expect_child_count(statement, 0U, "show count warnings child count");
    failures += expect_span_text(statement, "SHOW COUNT(*) WARNINGS", "show count warnings span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE warnings (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "warnings table");
    failures += expect_span_text(child_at(statement, 0U), "warnings", "warnings identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COUNT (*) WARNINGS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW WARNINGS LIMIT +1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW WARNINGS LIMIT -1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW WARNINGS LIKE 'x';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW WARNINGS WHERE Code = 1287;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW WARNINGS ORDER BY Code;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_index_empty_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW INDEX FROM numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show index");
    failures += expect_child_count(statement, 1U, "show index child count");
    failures += expect_span_text(child_at(statement, 0U), "numbers", "show index table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW INDEX IN app.numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show index in");
    failures += expect_child_count(statement, 1U, "show index in child count");
    failures += expect_span_text(child_at(statement, 0U), "app.numbers", "show index in table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW INDEXES FROM numbers FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show indexes");
    failures += expect_child_count(statement, 2U, "show indexes child count");
    failures += expect_span_text(child_at(statement, 0U), "numbers", "show indexes table");
    failures += expect_span_text(child_at(statement, 1U), "app", "show indexes schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW KEYS IN app.numbers IN other;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show keys");
    failures += expect_child_count(statement, 2U, "show keys child count");
    failures += expect_span_text(child_at(statement, 0U), "app.numbers", "show keys table");
    failures += expect_span_text(child_at(statement, 1U), "other", "show keys schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE indexes (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "indexes table");
    failures += expect_span_text(child_at(statement, 0U), "indexes", "indexes identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW INDEX;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW EXTENDED INDEX FROM numbers;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW INDEX FROM numbers WHERE Key_name = 'idx';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
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

    failures += parse_sql("SHOW DATABASES LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW DATABASES LIKE NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW DATABASES LIKE N'app%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW DATABASES WHERE Database = 'app';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL DATABASES;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL TABLE STATUS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EXTENDED TABLE STATUS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLE STATUS FULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW TABLE STATUS WHERE Name = 't';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLE STATUS LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLE STATUS LIKE NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLE STATUS LIKE N't%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW TABLE STATUS LIKE 't%' FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CHARACTER SETS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CHARSETS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLLATIONS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW CHARACTER SET WHERE Charset = 'utf8mb4';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CHARACTER SET LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CHARACTER SET LIKE NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW CHARACTER SET LIKE N'utf8mb4';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW CHARACTER SET LIKE _utf8mb4'utf8mb4';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW COLLATION WHERE Collation = 'utf8mb4_0900_ai_ci';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLLATION LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLLATION LIKE NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW COLLATION LIKE N'utf8mb4_0900_ai_ci';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW COLLATION LIKE _utf8mb4'utf8mb4_0900_ai_ci';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE DATABASE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW CREATE DATABASE IF NOT EXISTS app;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW CREATE DATABASE IF EXISTS app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW CREATE DATABASE app LIKE 'a%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW CREATE DATABASE app DEFAULT CHARACTER SET utf8mb4;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE DATABASE app.db;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ENGINES LIKE 'InnoDB';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW ENGINES WHERE Engine = 'InnoDB';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL ENGINES;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ENGINE InnoDB STATUS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL COLUMNS FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE TABLE t LIKE 't';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW CREATE TABLE t WHERE Table = 't';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE TABLE t FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE TEMPORARY TABLE t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL CREATE TABLE t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EXTENDED COLUMNS FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLUMNS FROM t LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW COLUMNS FROM t WHERE Field = 'id';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DESCRIBE t id;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DESCRIBE SELECT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN t id;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN t 'i%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN t FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN SELECT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN FORMAT=JSON SELECT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN ANALYZE SELECT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN FOR CONNECTION 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN EXTENDED SELECT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN PARTITIONS SELECT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

    failures += parse_sql(
        "CREATE TABLE t (id INT) ENGINE=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE t (id INT) DEFAULT CHARSET=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE t (id INT) COLLATE=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE t (id INT) DEFAULT CHARACTER SET;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE t (id INT) ENGINE=InnoDB, DEFAULT CHARSET=utf8mb4;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE t (id INT) COMMENT='x';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

    failures += parse_sql(
        "ALTER TABLE old_name RENAME TABLE new_name;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME new_name, ADD COLUMN added INT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME new_name, RENAME final_name;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_column TO new_column;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ADD COLUMN added INT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES ('text');", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES (1 + 2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t SET id = 1 + 2;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT IGNORE INTO t VALUES (1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT LOW_PRIORITY INTO t SET id = 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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
