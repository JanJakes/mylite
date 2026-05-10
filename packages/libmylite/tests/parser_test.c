#include "sql/mylite_ast.h"
#include "sql/mylite_parser.h"

#include <stdio.h>
#include <string.h>

enum {
    small_integer_column_count = 6,
    signed_integer_column_count = 6,
    alias_integer_column_count = 10,
    display_width_column_count = 16,
    bool_alias_column_count = 3,
    integer_default_column_count = 5,
    alias_int1_unsigned_column_index = 5,
    alias_int2_signed_column_index = 6,
    alias_int3_unsigned_column_index = 7,
    alias_int4_unsigned_column_index = 8,
    alias_int8_unsigned_column_index = 9,
    display_width_int_unsigned_column_index = 8,
    display_width_tinyint_signed_column_index = 9,
    display_width_tinyint_unsigned_column_index = 10,
    display_width_int1_column_index = 11,
    display_width_int8_column_index = 15,
    mediumint_unsigned_column_index = 5,
    signed_integer_column_index = 4,
    signed_bigint_column_index = 5,
};

static int test_empty_script(void);
static int test_use_statements(void);
static int test_select_expression_list(void);
static int test_current_database_functions(void);
static int test_current_user_identity_functions(void);
static int test_current_role_function(void);
static int test_version_function(void);
static int test_connection_id_function(void);
static int test_row_count_function(void);
static int test_last_insert_id_function(void);
static int test_diagnostics_count_system_variables(void);
static int test_count_star_aggregate(void);
static int test_min_max_aggregate(void);
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
static int test_show_errors_diagnostics_statements(void);
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
static int expect_integer_display_width(
    const struct mylite_sql_ast_node *node,
    const char *expected_width,
    const char *context
);
static int expect_integer_bool_alias(const struct mylite_sql_ast_node *node, const char *context);
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
static int expect_column_visibility(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_column_visibility expected,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_empty_script();
    failures += test_use_statements();
    failures += test_select_expression_list();
    failures += test_current_database_functions();
    failures += test_current_user_identity_functions();
    failures += test_current_role_function();
    failures += test_version_function();
    failures += test_connection_id_function();
    failures += test_row_count_function();
    failures += test_last_insert_id_function();
    failures += test_diagnostics_count_system_variables();
    failures += test_count_star_aggregate();
    failures += test_min_max_aggregate();
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
    failures += test_show_errors_diagnostics_statements();
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

static int test_current_role_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT CURRENT_ROLE(), Current_Role(), current_role() FROM DUAL;",
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
        MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION,
        "current role function"
    );
    failures += expect_span_text(first_expression, "CURRENT_ROLE()", "current role span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION,
        "mixed-case current role function"
    );
    failures +=
        expect_span_text(second_expression, "Current_Role()", "mixed-case current role span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION,
        "lower current role function"
    );
    failures += expect_span_text(third_expression, "current_role()", "lower current role span");
    failures +=
        expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "current role from dual");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT CURRENT_ROLE (), (CURRENT_ROLE());", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION,
        "spaced current role function"
    );
    failures += expect_span_text(first_expression, "CURRENT_ROLE ()", "spaced current role span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized current role function"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION,
        "wrapped current role function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT CURRENT_ROLE(1), CURRENT_ROLE(NULL), CURRENT_ROLE('x'), CURRENT_ROLE(1, 2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR,
        "current role integer argument error"
    );
    failures += expect_span_text(first_expression, "CURRENT_ROLE(1)", "current role integer span");
    arguments = child_at(first_expression, 0U);
    failures +=
        expect_node(arguments, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, "current role argument list");
    failures += expect_child_count(arguments, 1U, "current role one argument count");
    failures += expect_literal(
        child_at(arguments, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "current role integer argument"
    );
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR,
        "current role null argument error"
    );
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR,
        "current role string argument error"
    );
    arguments = child_at(child_at(child_at(select_list, 3U), 0U), 0U);
    failures += expect_child_count(arguments, 2U, "current role multiple argument count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE current_role (current_role INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures +=
        expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "current role identifier table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CURRENT_ROLE;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare current role identifier");
    failures += expect_span_text(first_expression, "CURRENT_ROLE", "bare current role span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CURRENT_ROLE() LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

static int test_last_insert_id_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT LAST_INSERT_ID(), Last_Insert_Id(), last_insert_id() FROM DUAL;",
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
        MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION,
        "last insert id function"
    );
    failures +=
        expect_span_text(first_expression, "LAST_INSERT_ID()", "last insert id function span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION,
        "mixed-case last insert id function"
    );
    failures +=
        expect_span_text(second_expression, "Last_Insert_Id()", "mixed-case last insert id span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION,
        "lower last insert id function"
    );
    failures += expect_span_text(third_expression, "last_insert_id()", "lower last insert id span");
    failures +=
        expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "last insert id from dual");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT LAST_INSERT_ID (), (LAST_INSERT_ID());", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION,
        "spaced last insert id function"
    );
    failures +=
        expect_span_text(first_expression, "LAST_INSERT_ID ()", "spaced last insert id span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized last insert id function"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION,
        "wrapped last insert id function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE last_insert_id (last_insert_id INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    failures += expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "last insert id identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT LAST_INSERT_ID;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare last insert id identifier");
    failures += expect_span_text(first_expression, "LAST_INSERT_ID", "bare last insert id span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT LAST_INSERT_ID(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT LAST_INSERT_ID(NULL);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT LAST_INSERT_ID(1, 2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT LAST_INSERT_ID() LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_diagnostics_count_system_variables(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT @@warning_count, @@session.error_count, @@local.Warning_Count FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_SYSTEM_VARIABLE, "warning count variable");
    failures += expect_span_text(first_expression, "@@warning_count", "warning count span");
    failures +=
        expect_node(second_expression, MYLITE_SQL_AST_SYSTEM_VARIABLE, "error count variable");
    failures += expect_span_text(second_expression, "@@session.error_count", "error count span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_SYSTEM_VARIABLE,
        "local warning count variable"
    );
    failures +=
        expect_span_text(third_expression, "@@local.Warning_Count", "local warning count span");
    failures +=
        expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "system variable from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT (@@warning_count), @@session.`warning_count`, @@`error_count`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized warning count variable"
    );
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_SYSTEM_VARIABLE,
        "wrapped warning count variable"
    );
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_SYSTEM_VARIABLE,
        "quoted warning count variable"
    );
    failures += expect_span_text(
        second_expression,
        "@@session.`warning_count`",
        "quoted warning count span"
    );
    failures += expect_node(third_expression, MYLITE_SQL_AST_SYSTEM_VARIABLE, "quoted error count");
    failures += expect_span_text(third_expression, "@@`error_count`", "quoted error count span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT @warning_count;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT ?;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

    failures +=
        parse_sql("SELECT COUNT(id), count(n), Count( nn ) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "count column function"
    );
    failures += expect_span_text(first_expression, "COUNT(id)", "count column span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "lower count column function"
    );
    failures += expect_span_text(second_expression, "count(n)", "lower count column span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "mixed count column function"
    );
    failures += expect_span_text(third_expression, "Count( nn )", "mixed count column span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "count column table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT COUNT(/* inside */n), COUNT(`weird name`) FROM t WHERE id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "commented count column function"
    );
    failures +=
        expect_span_text(first_expression, "COUNT(/* inside */n)", "commented count column span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "quoted count column function"
    );
    failures +=
        expect_span_text(second_expression, "COUNT(`weird name`)", "quoted count column span");
    failures +=
        expect_node(child_at(select, 2U), MYLITE_SQL_AST_WHERE_CLAUSE, "count column where");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT COUNT(1), count(-1), Count( +1 ) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "count integer literal function"
    );
    failures += expect_span_text(first_expression, "COUNT(1)", "count integer literal span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "lower count negative literal function"
    );
    failures += expect_span_text(second_expression, "count(-1)", "lower count negative span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "mixed count positive literal function"
    );
    failures += expect_span_text(third_expression, "Count( +1 )", "mixed count positive span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "count literal table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT COUNT(TRUE), count(false), Count( TRUE ) FROM t;",
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
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "count true literal function"
    );
    failures += expect_span_text(first_expression, "COUNT(TRUE)", "count true literal span");
    failures += expect_literal(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "count true literal argument"
    );
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "lower count false literal function"
    );
    failures += expect_span_text(second_expression, "count(false)", "lower count false span");
    failures += expect_literal(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "count false literal argument"
    );
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "mixed count true literal function"
    );
    failures += expect_span_text(third_expression, "Count( TRUE )", "mixed count true span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "count boolean table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT COUNT(/* inside */NULL) FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "commented count null literal function"
    );
    failures += expect_span_text(
        first_expression,
        "COUNT(/* inside */NULL)",
        "commented count null literal span"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "count literal dual");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT COUNT(/* inside */FALSE) FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "commented count false literal function"
    );
    failures += expect_span_text(
        first_expression,
        "COUNT(/* inside */FALSE)",
        "commented count false literal span"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "count boolean dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT (COUNT(n));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized count column function"
    );
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "wrapped count column function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT COUNT(*), COUNT(id);", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_COUNT_STAR_FUNCTION, "mixed count star");
    failures +=
        expect_node(second_expression, MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION, "mixed count column");
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
    failures += parse_sql("SELECT COUNT(1.0);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT('x');", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT(+TRUE);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT(-FALSE);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT(NOT TRUE);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT(id + 1) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT(TRUE + 1) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT(id, n) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT(t.*) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT(t.id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT COUNT(DISTINCT id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_min_max_aggregate(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    int failures = 0;

    failures += parse_sql("SELECT MIN(id), max(n), Max( n ) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION, "min aggregate");
    failures += expect_span_text(first_expression, "MIN(id)", "min aggregate span");
    failures += expect_node(child_at(first_expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "min arg");
    failures += expect_span_text(child_at(first_expression, 0U), "id", "min arg span");
    failures += expect_node(second_expression, MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION, "lower max");
    failures += expect_span_text(second_expression, "max(n)", "lower max span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION, "mixed max");
    failures += expect_span_text(third_expression, "Max( n )", "mixed max span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "min max from table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT MAX(/* inside */n) FROM t WHERE id = 1;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION,
        "commented max aggregate"
    );
    failures += expect_span_text(first_expression, "MAX(/* inside */n)", "commented max span");
    failures += expect_span_text(child_at(first_expression, 0U), "n", "commented max arg span");
    failures += expect_node(child_at(select, 2U), MYLITE_SQL_AST_WHERE_CLAUSE, "max where");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT (MIN(id));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized min aggregate"
    );
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION,
        "wrapped min aggregate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE min (max INT, min INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures +=
        expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "min max identifier table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT MIN, MAX;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare min identifier");
    failures += expect_span_text(first_expression, "MIN", "bare min span");
    failures += expect_node(second_expression, MYLITE_SQL_AST_IDENTIFIER, "bare max identifier");
    failures += expect_span_text(second_expression, "MAX", "bare max span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT MIN (id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT MAX/**/(id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT MIN();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT MAX(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT MIN(NULL);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT MIN(id, n) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT MIN(*) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT MIN(t.id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT MIN(DISTINCT id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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
    const struct mylite_sql_ast_node *table_names = NULL;
    const struct mylite_sql_ast_node *table_name = NULL;
    const struct mylite_sql_ast_node *rename_pairs = NULL;
    const struct mylite_sql_ast_node *rename_pair = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *table_options = NULL;
    const struct mylite_sql_ast_node *engine_option = NULL;
    const struct mylite_sql_ast_node *charset_option = NULL;
    const struct mylite_sql_ast_node *collation_option = NULL;
    const struct mylite_sql_ast_node *if_not_exists = NULL;
    const struct mylite_sql_ast_node *if_exists = NULL;
    const struct mylite_sql_ast_node *column_type = NULL;
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
    failures += expect_integer_display_width(child_at(column, 1U), NULL, "first column width");
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
        "CREATE TABLE small_integer_types (ti TINYINT, tiu TINYINT UNSIGNED, "
        "si SMALLINT, siu SMALLINT UNSIGNED, mi MEDIUMINT, miu MEDIUMINT UNSIGNED);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures +=
        expect_child_count(columns, small_integer_column_count, "small integer column list");
    column = child_at(columns, 0U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint column type"
    );
    column = child_at(columns, 1U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        1,
        "tinyint unsigned column type"
    );
    column = child_at(columns, 2U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
        0,
        "smallint column type"
    );
    column = child_at(columns, 3U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
        1,
        "smallint unsigned column type"
    );
    column = child_at(columns, 4U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        0,
        "mediumint column type"
    );
    column = child_at(columns, mediumint_unsigned_column_index);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        1,
        "mediumint unsigned column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE signed_integer_types (ti TINYINT SIGNED, si SMALLINT SIGNED, "
        "mi MEDIUMINT SIGNED, i INT SIGNED, ii INTEGER SIGNED, b BIGINT SIGNED);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures +=
        expect_child_count(columns, signed_integer_column_count, "signed integer column list");
    column = child_at(columns, 0U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint signed column type"
    );
    failures += expect_span_text(child_at(column, 1U), "TINYINT SIGNED", "tinyint signed span");
    column = child_at(columns, 1U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
        0,
        "smallint signed column type"
    );
    column = child_at(columns, 2U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        0,
        "mediumint signed column type"
    );
    column = child_at(columns, 3U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "int signed column type"
    );
    column = child_at(columns, signed_integer_column_index);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "integer signed column type"
    );
    failures += expect_span_text(child_at(column, 1U), "INTEGER SIGNED", "integer signed span");
    column = child_at(columns, signed_bigint_column_index);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        0,
        "bigint signed column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE integer_aliases (i1 INT1, i2 INT2, i3 INT3, i4 INT4, i8 INT8, "
        "i1u INT1 UNSIGNED, i2s INT2 SIGNED, i3u INT3 UNSIGNED, "
        "i4u INT4 UNSIGNED, i8u INT8 UNSIGNED);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures +=
        expect_child_count(columns, alias_integer_column_count, "alias integer column list");
    column = child_at(columns, 0U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "int1 alias column type"
    );
    failures += expect_span_text(child_at(column, 1U), "INT1", "int1 alias span");
    column = child_at(columns, 1U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
        0,
        "int2 alias column type"
    );
    column = child_at(columns, 2U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        0,
        "int3 alias column type"
    );
    column = child_at(columns, 3U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "int4 alias column type"
    );
    column = child_at(columns, 4U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        0,
        "int8 alias column type"
    );
    column = child_at(columns, alias_int1_unsigned_column_index);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        1,
        "int1 unsigned alias column type"
    );
    failures += expect_span_text(child_at(column, 1U), "INT1 UNSIGNED", "int1 unsigned span");
    column = child_at(columns, alias_int2_signed_column_index);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
        0,
        "int2 signed alias column type"
    );
    failures += expect_span_text(child_at(column, 1U), "INT2 SIGNED", "int2 signed span");
    column = child_at(columns, alias_int3_unsigned_column_index);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        1,
        "int3 unsigned alias column type"
    );
    column = child_at(columns, alias_int4_unsigned_column_index);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        1,
        "int4 unsigned alias column type"
    );
    column = child_at(columns, alias_int8_unsigned_column_index);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        1,
        "int8 unsigned alias column type"
    );
    failures += expect_span_text(child_at(column, 1U), "INT8 UNSIGNED", "int8 unsigned span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE display_widths (ti0 TINYINT(0), ti1 TINYINT(1), "
        "ti2 TINYINT(2), si SMALLINT(5), mi MEDIUMINT(9), i INT(11), "
        "ii INTEGER(10), bi BIGINT(20), iu INT(10) UNSIGNED, "
        "tis TINYINT(1) SIGNED, tiu TINYINT(1) UNSIGNED, i1 INT1(1), "
        "i2 INT2(5), i3 INT3(7), i4 INT4(9), i8 INT8(20));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures +=
        expect_child_count(columns, display_width_column_count, "display width column list");

    column = child_at(columns, 0U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint zero display width type"
    );
    failures += expect_integer_display_width(column_type, "0", "tinyint zero display width");
    failures += expect_span_text(column_type, "TINYINT(0)", "tinyint zero display width span");

    column = child_at(columns, 1U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint one display width type"
    );
    failures += expect_integer_display_width(column_type, "1", "tinyint one display width");

    column = child_at(columns, display_width_int_unsigned_column_index);
    column_type = child_at(column, 1U);
    failures +=
        expect_integer_type(column_type, MYLITE_SQL_AST_INTEGER_TYPE_INT, 1, "int width unsigned");
    failures += expect_integer_display_width(column_type, "10", "int unsigned display width");
    failures += expect_span_text(column_type, "INT(10) UNSIGNED", "int unsigned width span");

    column = child_at(columns, display_width_tinyint_signed_column_index);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint width signed"
    );
    failures += expect_integer_display_width(column_type, "1", "tinyint signed display width");
    failures += expect_span_text(column_type, "TINYINT(1) SIGNED", "tinyint signed width span");

    column = child_at(columns, display_width_tinyint_unsigned_column_index);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        1,
        "tinyint width unsigned"
    );
    failures += expect_integer_display_width(column_type, "1", "tinyint unsigned display width");
    failures += expect_span_text(column_type, "TINYINT(1) UNSIGNED", "tinyint unsigned width span");

    column = child_at(columns, display_width_int1_column_index);
    column_type = child_at(column, 1U);
    failures +=
        expect_integer_type(column_type, MYLITE_SQL_AST_INTEGER_TYPE_TINYINT, 0, "int1 width");
    failures += expect_integer_display_width(column_type, "1", "int1 display width");

    column = child_at(columns, display_width_int8_column_index);
    column_type = child_at(column, 1U);
    failures +=
        expect_integer_type(column_type, MYLITE_SQL_AST_INTEGER_TYPE_BIGINT, 0, "int8 width");
    failures += expect_integer_display_width(column_type, "20", "int8 display width");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE bool_aliases (b BOOL, c BOOLEAN, nn BOOL NOT NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, bool_alias_column_count, "bool alias column list");
    column = child_at(columns, 0U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "bool alias column type"
    );
    failures += expect_integer_display_width(column_type, NULL, "bool alias display width");
    failures += expect_integer_bool_alias(column_type, "bool alias marker");
    failures += expect_span_text(column_type, "BOOL", "bool alias span");
    column = child_at(columns, 1U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "boolean alias column type"
    );
    failures += expect_integer_bool_alias(column_type, "boolean alias marker");
    failures += expect_span_text(column_type, "BOOLEAN", "boolean alias span");
    column = child_at(columns, 2U);
    column_type = child_at(column, 1U);
    failures += expect_integer_bool_alias(column_type, "bool not null alias marker");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "bool alias not null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE explicit_default_nulls (a INT DEFAULT NULL, "
        "b BIGINT NULL DEFAULT NULL, c BOOL DEFAULT NULL, nn INT NOT NULL DEFAULT NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, 4U, "explicit default null column list");
    column = child_at(columns, 0U);
    failures += expect_true(
        first_child_kind(column, MYLITE_SQL_AST_NULLABILITY) == NULL,
        "default null omitted nullability"
    );
    failures += expect_node(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "default null marker"
    );
    failures += expect_span_text(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        "DEFAULT NULL",
        "default null span"
    );
    column = child_at(columns, 1U);
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "default null explicit nullability"
    );
    failures += expect_node(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "explicit null default marker"
    );
    column = child_at(columns, 2U);
    failures += expect_integer_bool_alias(child_at(column, 1U), "default null bool marker");
    failures += expect_node(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "bool default null marker"
    );
    column = child_at(columns, 3U);
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "not null default null parser marker"
    );
    failures += expect_node(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "not null default null marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE integer_defaults (a INT DEFAULT 5, b INT DEFAULT +9, "
        "c INT DEFAULT -7, d BOOL DEFAULT TRUE, e BOOLEAN DEFAULT FALSE);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures +=
        expect_child_count(columns, integer_default_column_count, "integer default column list");
    column = child_at(columns, 0U);
    failures += expect_node(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
        MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE,
        "integer default marker"
    );
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "integer default literal"
    );
    failures += expect_span_text(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
        "DEFAULT 5",
        "integer default span"
    );
    column = child_at(columns, 1U);
    failures += expect_operator(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "positive default operator"
    );
    column = child_at(columns, 2U);
    failures += expect_operator(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "negative default operator"
    );
    column = child_at(columns, 3U);
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "true default literal"
    );
    column = child_at(columns, 4U);
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "false default literal"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE bool_identifiers (BOOL INT, BOOLEAN TINYINT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    column = child_at(columns, 0U);
    failures += expect_span_text(child_at(column, 0U), "BOOL", "bool identifier column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "bool identifier column type"
    );
    column = child_at(columns, 1U);
    failures += expect_span_text(child_at(column, 0U), "BOOLEAN", "boolean identifier column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "boolean identifier column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE IF NOT EXISTS app.if_missing (id INT) ENGINE=InnoDB;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_name = child_at(statement, 0U);
    columns = child_at(statement, 1U);
    if_not_exists = child_at(statement, 2U);
    table_options = child_at(statement, 3U);
    failures += expect_child_count(statement, 4U, "create if not exists child count");
    failures += expect_node(
        if_not_exists,
        MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE,
        "create if not exists clause"
    );
    failures += expect_span_text(child_at(table_name, 1U), "if_missing", "if not exists table");
    failures += expect_child_count(columns, 1U, "if not exists column list");
    failures +=
        expect_node(table_options, MYLITE_SQL_AST_TABLE_OPTION_LIST, "if not exists options");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP TABLE IF EXISTS app.if_missing;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    table_names = child_at(statement, 0U);
    table_name = child_at(table_names, 0U);
    if_exists = child_at(statement, 1U);
    failures += expect_child_count(statement, 2U, "drop if exists child count");
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_TABLE_STATEMENT, "drop table statement");
    failures += expect_node(table_names, MYLITE_SQL_AST_TABLE_NAME_LIST, "drop table name list");
    failures += expect_child_count(table_names, 1U, "drop if exists table name count");
    failures +=
        expect_node(table_name, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "drop qualified table");
    failures +=
        expect_node(if_exists, MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE, "drop if exists clause");
    failures += expect_span_text(child_at(table_name, 0U), "app", "drop if exists schema");
    failures += expect_span_text(child_at(table_name, 1U), "if_missing", "drop if exists table");
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
    table_names = child_at(statement, 0U);
    table_name = child_at(table_names, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_TABLE_STATEMENT, "drop table statement");
    failures +=
        expect_node(table_names, MYLITE_SQL_AST_TABLE_NAME_LIST, "single drop table name list");
    failures += expect_child_count(table_names, 1U, "single drop table name count");
    failures += expect_span_text(table_name, "app.simple_lifecycle", "drop target");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("DROP TABLE first_table, app.second_table;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    table_names = child_at(statement, 0U);
    table_name = child_at(table_names, 0U);
    failures += expect_child_count(statement, 1U, "multi drop child count");
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_TABLE_STATEMENT, "multi drop statement");
    failures +=
        expect_node(table_names, MYLITE_SQL_AST_TABLE_NAME_LIST, "multi drop table name list");
    failures += expect_child_count(table_names, 2U, "multi drop table name count");
    failures += expect_span_text(table_name, "first_table", "first multi drop target");
    failures +=
        expect_span_text(child_at(table_names, 1U), "app.second_table", "second multi drop target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DROP TABLE IF EXISTS first_table, app.second_table;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_names = child_at(statement, 0U);
    if_exists = child_at(statement, 1U);
    failures += expect_child_count(statement, 2U, "multi drop if exists child count");
    failures += expect_child_count(table_names, 2U, "multi drop if exists table name count");
    failures +=
        expect_node(if_exists, MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE, "multi drop if exists clause");
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
    rename_pairs = child_at(statement, 0U);
    rename_pair = child_at(rename_pairs, 0U);
    failures += expect_child_count(statement, 1U, "rename table statement");
    failures +=
        expect_node(rename_pairs, MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST, "rename pair list");
    failures += expect_child_count(rename_pairs, 1U, "rename pair list child count");
    failures += expect_node(rename_pair, MYLITE_SQL_AST_RENAME_TABLE_PAIR, "rename pair");
    failures +=
        expect_span_text(child_at(rename_pair, 0U), "app.simple_lifecycle", "rename source");
    failures +=
        expect_span_text(child_at(rename_pair, 1U), "archive.renamed_lifecycle", "rename target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "RENAME TABLE first_old TO first_new, app.second_old TO archive.second_new;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    rename_pairs = child_at(statement, 0U);
    rename_pair = child_at(rename_pairs, 0U);
    failures += expect_child_count(statement, 1U, "multi rename statement");
    failures += expect_child_count(rename_pairs, 2U, "multi rename pair count");
    failures += expect_node(rename_pair, MYLITE_SQL_AST_RENAME_TABLE_PAIR, "first rename pair");
    failures += expect_span_text(child_at(rename_pair, 0U), "first_old", "first rename source");
    failures += expect_span_text(child_at(rename_pair, 1U), "first_new", "first rename target");
    rename_pair = child_at(rename_pairs, 1U);
    failures +=
        expect_span_text(child_at(rename_pair, 0U), "app.second_old", "second rename source");
    failures +=
        expect_span_text(child_at(rename_pair, 1U), "archive.second_new", "second rename target");
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
        "ALTER TABLE app.simple_lifecycle ADD COLUMN added INT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT,
        "alter table add column statement"
    );
    failures += expect_child_count(statement, 2U, "alter add column child count");
    failures +=
        expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "alter add target");
    column = child_at(statement, 1U);
    failures += expect_node(column, MYLITE_SQL_AST_COLUMN_DEFINITION, "alter add column");
    failures += expect_span_text(child_at(column, 0U), "added", "alter added column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "alter added column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "alter added column nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle DROP COLUMN old_col;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT,
        "alter table drop column statement"
    );
    failures += expect_child_count(statement, 2U, "alter drop column child count");
    failures +=
        expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "alter drop target");
    failures += expect_span_text(child_at(statement, 1U), "old_col", "alter dropped column name");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE simple_lifecycle DROP old_col;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT,
        "alter table bare drop column statement"
    );
    failures +=
        expect_span_text(child_at(statement, 0U), "simple_lifecycle", "bare alter drop target");
    failures += expect_span_text(child_at(statement, 1U), "old_col", "bare dropped column name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle RENAME COLUMN old_col TO new_col;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_RENAME_COLUMN_STATEMENT,
        "alter table rename column statement"
    );
    failures += expect_child_count(statement, 3U, "alter rename column child count");
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter rename column target"
    );
    failures +=
        expect_span_text(child_at(statement, 1U), "old_col", "alter rename old column name");
    failures +=
        expect_span_text(child_at(statement, 2U), "new_col", "alter rename new column name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle MODIFY COLUMN old_col BIGINT NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT,
        "alter table modify column statement"
    );
    failures += expect_child_count(statement, 2U, "alter modify column child count");
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter modify column target"
    );
    column = child_at(statement, 1U);
    failures += expect_node(column, MYLITE_SQL_AST_COLUMN_DEFINITION, "alter modify column");
    failures += expect_span_text(child_at(column, 0U), "old_col", "alter modified column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        0,
        "alter modified column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "alter modified column nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle CHANGE COLUMN old_col new_col BIGINT NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT,
        "alter table change column statement"
    );
    failures += expect_child_count(statement, 3U, "alter change column child count");
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter change column target"
    );
    failures += expect_span_text(child_at(statement, 1U), "old_col", "alter change old column");
    column = child_at(statement, 2U);
    failures += expect_node(column, MYLITE_SQL_AST_COLUMN_DEFINITION, "alter changed column");
    failures += expect_span_text(child_at(column, 0U), "new_col", "alter change new column");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        0,
        "alter changed column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "alter changed column nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD added BIGINT UNSIGNED NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT,
        "bare alter table add statement"
    );
    failures +=
        expect_span_text(child_at(statement, 0U), "simple_lifecycle", "bare alter add target");
    column = child_at(statement, 1U);
    failures += expect_span_text(child_at(column, 0U), "added", "bare alter add column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        1,
        "bare alter add column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "bare alter add column nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD added_small SMALLINT UNSIGNED NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
        1,
        "smallint unsigned alter add column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD added_signed TINYINT SIGNED NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint signed alter add column type"
    );
    failures += expect_span_text(child_at(column, 1U), "TINYINT SIGNED", "signed alter add span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD added_alias INT1 UNSIGNED NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        1,
        "int1 unsigned alter add column type"
    );
    failures += expect_span_text(child_at(column, 1U), "INT1 UNSIGNED", "alias alter add span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD added_width TINYINT(1) NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "display width alter add column type"
    );
    failures += expect_integer_display_width(column_type, "1", "display width alter add");
    failures += expect_span_text(column_type, "TINYINT(1)", "display width alter add span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD added_bool BOOL NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "bool alias alter add column type"
    );
    failures += expect_integer_bool_alias(column_type, "bool alias alter add marker");
    failures += expect_span_text(column_type, "BOOL", "bool alias alter add span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD added_default INT NULL DEFAULT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_node(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "alter add default null marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col INT UNSIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT,
        "bare alter table modify statement"
    );
    failures +=
        expect_span_text(child_at(statement, 0U), "simple_lifecycle", "bare alter modify target");
    column = child_at(statement, 1U);
    failures += expect_span_text(child_at(column, 0U), "old_col", "bare alter modify column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        1,
        "bare alter modify column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "bare alter modify column nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col TINYINT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint alter modify column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col INT SIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "int signed alter modify column type"
    );
    failures += expect_span_text(child_at(column, 1U), "INT SIGNED", "signed alter modify span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col INT4 SIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "int4 signed alter modify column type"
    );
    failures += expect_span_text(child_at(column, 1U), "INT4 SIGNED", "alias alter modify span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col INT(11) UNSIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        1,
        "display width alter modify column type"
    );
    failures += expect_integer_display_width(column_type, "11", "display width alter modify");
    failures +=
        expect_span_text(column_type, "INT(11) UNSIGNED", "display width alter modify span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col BOOLEAN NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "boolean alias alter modify column type"
    );
    failures += expect_integer_bool_alias(column_type, "boolean alias alter modify marker");
    failures += expect_span_text(column_type, "BOOLEAN", "boolean alias alter modify span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col BIGINT DEFAULT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_node(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "alter modify default null marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_col INT UNSIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT,
        "bare alter table change statement"
    );
    failures +=
        expect_span_text(child_at(statement, 0U), "simple_lifecycle", "bare alter change target");
    failures += expect_span_text(child_at(statement, 1U), "old_col", "bare alter change old name");
    column = child_at(statement, 2U);
    failures += expect_span_text(child_at(column, 0U), "new_col", "bare alter change new name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        1,
        "bare alter change column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "bare alter change column nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_col MEDIUMINT UNSIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 2U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        1,
        "mediumint unsigned alter change column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_col MEDIUMINT SIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 2U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        0,
        "mediumint signed alter change column type"
    );
    failures +=
        expect_span_text(child_at(column, 1U), "MEDIUMINT SIGNED", "signed alter change span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_col INT3 NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 2U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        0,
        "int3 alias alter change column type"
    );
    failures += expect_span_text(child_at(column, 1U), "INT3", "alias alter change span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_width INT1(1) NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 2U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "display width alter change column type"
    );
    failures += expect_integer_display_width(column_type, "1", "display width alter change");
    failures += expect_span_text(column_type, "INT1(1)", "display width alter change span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_bool BOOL NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 2U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "bool alias alter change column type"
    );
    failures += expect_integer_bool_alias(column_type, "bool alias alter change marker");
    failures += expect_span_text(column_type, "BOOL", "bool alias alter change span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_default BIGINT DEFAULT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 2U);
    failures += expect_node(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "alter change default null marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle ALTER COLUMN old_col SET DEFAULT +8;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT,
        "alter table set default statement"
    );
    failures += expect_child_count(statement, 3U, "alter set default child count");
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter set default target"
    );
    failures += expect_span_text(child_at(statement, 1U), "old_col", "alter set default column");
    failures += expect_node(
        child_at(statement, 2U),
        MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE,
        "alter set default value node"
    );
    failures += expect_operator(
        child_at(child_at(statement, 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "alter set default positive value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ALTER old_col SET DEFAULT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT,
        "alter table bare set default statement"
    );
    failures += expect_node(
        child_at(statement, 2U),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "alter set default null node"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle ALTER COLUMN old_col DROP DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT,
        "alter table drop default statement"
    );
    failures += expect_child_count(statement, 2U, "alter drop default child count");
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter drop default target"
    );
    failures += expect_span_text(child_at(statement, 1U), "old_col", "alter drop default column");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ALTER old_col DROP DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT,
        "alter table bare drop default statement"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle ALTER COLUMN old_col SET INVISIBLE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT,
        "alter table column invisible statement"
    );
    failures += expect_child_count(statement, 2U, "alter column invisible child count");
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter column invisible target"
    );
    failures +=
        expect_span_text(child_at(statement, 1U), "old_col", "alter column invisible column");
    failures += expect_column_visibility(
        statement,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE,
        "alter column invisible payload"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ALTER old_col SET VISIBLE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT,
        "alter table column visible statement"
    );
    failures += expect_column_visibility(
        statement,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE,
        "alter column visible payload"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE visibility_names (visible INT, invisible INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
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
        "INSERT INTO simple_lifecycle VALUES (TRUE, false);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_literal(
        child_at(child_at(child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "true insert value"
    );
    failures += expect_literal(
        child_at(child_at(child_at(statement, 2U), 0U), 1U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "false insert value"
    );
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

    failures += parse_sql(
        "INSERT INTO app.simple_lifecycle SET id = TRUE, amount = false;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_literal(
        child_at(child_at(child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "insert set true value"
    );
    failures += expect_literal(
        child_at(child_at(child_at(statement, 1U), 1U), 1U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "insert set false value"
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
    failures += expect_child_count(statement, 1U, "create database child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE SCHEMA `select`;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT, "create schema");
    failures += expect_span_text(child_at(statement, 0U), "`select`", "create schema name");
    failures += expect_child_count(statement, 1U, "create schema child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE DATABASE IF NOT EXISTS app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT, "create database if");
    failures += expect_span_text(child_at(statement, 0U), "app", "create database if name");
    failures += expect_node(
        child_at(statement, 1U),
        MYLITE_SQL_AST_CREATE_SCHEMA_IF_NOT_EXISTS_CLAUSE,
        "create database if marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP DATABASE app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT, "drop database");
    failures += expect_span_text(child_at(statement, 0U), "app", "drop database name");
    failures += expect_child_count(statement, 1U, "drop database child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP SCHEMA app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT, "drop schema");
    failures += expect_span_text(child_at(statement, 0U), "app", "drop schema name");
    failures += expect_child_count(statement, 1U, "drop schema child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP SCHEMA IF EXISTS app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT, "drop schema if");
    failures += expect_span_text(child_at(statement, 0U), "app", "drop schema if name");
    failures += expect_node(
        child_at(statement, 1U),
        MYLITE_SQL_AST_DROP_SCHEMA_IF_EXISTS_CLAUSE,
        "drop schema if marker"
    );
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

static int test_show_errors_diagnostics_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *limit = NULL;
    int failures = 0;

    failures += parse_sql("SHOW ERRORS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT, "show errors");
    failures += expect_child_count(statement, 0U, "show errors child count");
    failures += expect_span_text(statement, "SHOW ERRORS", "show errors span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ERRORS LIMIT 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    limit = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT, "show errors limit");
    failures += expect_child_count(statement, 1U, "show errors limit child count");
    failures += expect_node(limit, MYLITE_SQL_AST_LIMIT_CLAUSE, "show errors limit clause");
    failures += expect_span_text(child_at(limit, 0U), "1", "show errors limit row count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ERRORS LIMIT 2, 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    limit = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT, "show errors comma");
    failures += expect_child_count(limit, 2U, "show errors comma child count");
    failures += expect_span_text(child_at(limit, 0U), "1", "show errors comma row count");
    failures += expect_span_text(child_at(limit, 1U), "2", "show errors comma offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ERRORS LIMIT 1 OFFSET 2;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    limit = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT, "show errors offset");
    failures += expect_child_count(limit, 2U, "show errors offset child count");
    failures += expect_span_text(child_at(limit, 0U), "1", "show errors offset row count");
    failures += expect_span_text(child_at(limit, 1U), "2", "show errors offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COUNT(*) ERRORS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_COUNT_ERRORS_STATEMENT, "show count errors");
    failures += expect_child_count(statement, 0U, "show count errors child count");
    failures += expect_span_text(statement, "SHOW COUNT(*) ERRORS", "show count errors span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE errors (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "errors table");
    failures += expect_span_text(child_at(statement, 0U), "errors", "errors identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COUNT (*) ERRORS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ERRORS LIMIT +1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ERRORS LIMIT -1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ERRORS LIKE 'x';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ERRORS WHERE Code = 1064;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ERRORS ORDER BY Code;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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
            "SELECT id FROM simple_lifecycle WHERE id = TRUE;",
            MYLITE_SQL_AST_OPERATOR_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id <=> false;",
            MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL,
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

    failures +=
        parse_sql("SELECT id FROM simple_lifecycle WHERE id = TRUE;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_literal(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 1U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "true predicate right operand"
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
        "DELETE FROM simple_lifecycle WHERE id = FALSE LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    where_clause = child_at(statement, 1U);
    failures += expect_literal(
        child_at(child_at(where_clause, 0U), 1U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "delete false predicate"
    );
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

    failures += parse_sql(
        "UPDATE simple_lifecycle SET amount = FALSE WHERE id = TRUE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 1U), 0U);
    where_clause = child_at(statement, 2U);
    failures += expect_literal(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "update false assignment value"
    );
    failures += expect_literal(
        child_at(child_at(where_clause, 0U), 1U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "update true predicate value"
    );
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

    failures += parse_sql("CREATE DATABASE IF EXISTS app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE DATABASE app DEFAULT CHARACTER SET utf8mb4;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE DATABASE a.b;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("DROP DATABASE IF NOT EXISTS app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

    failures += parse_sql(
        "CREATE TABLE t (id INT DEFAULT (NULL));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE t (id INT DEFAULT '5');", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE t (id INT DEFAULT 1.5);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE t (id INT DEFAULT 0x1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE t (id INT DEFAULT 1 + 2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE t (id INT DEFAULT NULL NULL);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE t (id INT DEFAULT NULL DEFAULT NULL);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE t (id INT DEFAULT NULL NOT NULL);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE t (id INT(+1));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

    failures +=
        parse_sql("CREATE TABLE IF EXISTS t (id INT);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE IF NOT t (id INT);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP TABLE IF NOT EXISTS t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

    failures +=
        parse_sql("RENAME TABLE old_name TO new_name,;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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
        "ALTER TABLE old_name ADD COLUMN added INT FIRST;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ADD COLUMN added INT AFTER id;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE old_name ADD (added INT);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ADD COLUMN first_added INT, ADD COLUMN second_added INT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ADD COLUMN old_name.added INT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ADD COLUMN added VARCHAR(10);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_signed_unsigned (c INT SIGNED UNSIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_unsigned_signed (c INT UNSIGNED SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_repeated_signed (c INT SIGNED SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_repeated_unsigned (c INT UNSIGNED UNSIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_alias_zerofill (c INT2 ZEROFILL);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_alias_signed_unsigned (c INT1 SIGNED UNSIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_alias_unsigned_signed (c INT1 UNSIGNED SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_alias_repeated_signed (c INT1 SIGNED SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_alias_repeated_unsigned (c INT1 UNSIGNED UNSIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_decimal_signed (c DECIMAL SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_width_minus (c INT(-1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_width_empty (c INT());",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_width_decimal (c INT(1.0));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_width_string (c INT('1'));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_width_hex (c INT(0x1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_width_bit (c INT(b'1'));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_width_parameter (c INT(?));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_width_expression (c INT(1+1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_width_after_unsigned (c INT UNSIGNED(1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_alias_width_signed (c INT1(+1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_zerofill (c MEDIUMINT ZEROFILL);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_int5 (c INT5);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_serial (c SERIAL);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_bool_width (c BOOL(1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_boolean_width (c BOOLEAN(1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_bool_signed (c BOOL SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_bool_unsigned (c BOOL UNSIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_bool_zerofill (c BOOL ZEROFILL);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_bool_signed_unsigned (c BOOL SIGNED UNSIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name DROP COLUMN old_name.added;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name DROP COLUMN added, DROP COLUMN other;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE old_name DROP PRIMARY KEY;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE old_name DROP INDEX idx;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name DROP FOREIGN KEY fk;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name DROP COLUMN IF EXISTS added;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE old_name DROP (added);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name DROP COLUMN added FIRST;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name DROP COLUMN added, ALGORITHM=INSTANT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name DROP COLUMN added, LOCK=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME old_col TO new_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_name.old_col TO new_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_col TO old_name.new_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_col new_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_col TO new_col FIRST;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_col TO new_col, RENAME COLUMN other TO final;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_col TO new_col, ALGORITHM=INSTANT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_col TO new_col, LOCK=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name MODIFY old_name.old_col BIGINT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name MODIFY old_col BIGINT FIRST;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name MODIFY old_col BIGINT AFTER other_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name MODIFY old_col BIGINT, MODIFY other_col BIGINT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name MODIFY old_col VARCHAR(10);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name MODIFY old_col BIGINT, ALGORITHM=INSTANT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name MODIFY old_col BIGINT, LOCK=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CHANGE old_name.old_col new_col BIGINT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CHANGE old_col old_name.new_col BIGINT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col BIGINT FIRST;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col BIGINT AFTER other_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col BIGINT, CHANGE other_col final_col BIGINT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col VARCHAR(10);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col BIGINT, ALGORITHM=INSTANT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col BIGINT, LOCK=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALTER COLUMN old_name.old_col SET DEFAULT 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALTER old_col SET DEFAULT (1 + 1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALTER old_col SET DEFAULT '1';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALTER COLUMN old_name.old_col DROP DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALTER old_col DROP DEFAULT, ALTER other DROP DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALTER COLUMN old_name.old_col SET INVISIBLE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALTER old_col SET INVISIBLE, ALTER other SET VISIBLE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALTER old_col SET HIDDEN;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE t (id INT INVISIBLE);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE t ADD COLUMN v INT INVISIBLE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALTER old_col SET DEFAULT 1, ALTER other SET DEFAULT 2;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES ('text');", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES (1 + 2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES (+TRUE);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

    failures += parse_sql("SELECT id FROM t WHERE TRUE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM t WHERE id IS TRUE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

    failures += parse_sql("SELECT id FROM t LIMIT TRUE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t LIMIT +1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t LIMIT FALSE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

    failures += parse_sql("UPDATE t SET id = -FALSE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET id = TRUE + 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET id = other;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET id = DEFAULT;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET id = 1 LIMIT +1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE t SET id = 1 LIMIT FALSE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

static int expect_integer_display_width(
    const struct mylite_sql_ast_node *node,
    const char *expected_width,
    const char *context
) {
    int has_width = mylite_sql_ast_node_integer_type_has_display_width(node);
    struct mylite_sql_source_span span = mylite_sql_ast_node_integer_type_display_width_span(node);

    if (expected_width == NULL) {
        if (has_width) {
            fprintf(stderr, "%s: expected no display width\n", context);
            return 1;
        }
        return 0;
    }

    if (!has_width) {
        fprintf(
            stderr,
            "%s: expected display width %s, got no display width\n",
            context,
            expected_width
        );
        return 1;
    }
    if (span.text == NULL || span.length != strlen(expected_width) ||
        strncmp(span.text, expected_width, span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected display width %s, got %.*s\n",
            context,
            expected_width,
            (int)span.length,
            span.text == NULL ? "" : span.text
        );
        return 1;
    }

    return 0;
}

static int expect_integer_bool_alias(const struct mylite_sql_ast_node *node, const char *context) {
    if (mylite_sql_ast_node_integer_type_is_bool_alias(node) == 0) {
        fprintf(stderr, "%s: expected bool alias marker\n", context);
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

static int expect_column_visibility(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_column_visibility expected,
    const char *context
) {
    enum mylite_sql_ast_column_visibility actual = mylite_sql_ast_node_column_visibility(node);

    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected column visibility %s, got %s\n",
            context,
            mylite_sql_ast_column_visibility_name(expected),
            mylite_sql_ast_column_visibility_name(actual)
        );
        return 1;
    }

    return 0;
}
