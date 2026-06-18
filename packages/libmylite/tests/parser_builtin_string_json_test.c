#include "parser_test_support.h"

#include <stdio.h>

static int test_current_database_functions(void);
static int test_current_user_identity_functions(void);
static int test_current_role_function(void);
static int test_if_function(void);
static int test_ifnull_function(void);
static int test_coalesce_function(void);
static int test_concat_function(void);
static int test_pipes_as_concat_operator(void);
static int test_concat_ws_function(void);
static int test_replace_function(void);
static int test_reverse_function(void);
static int test_quote_function(void);
static int test_elt_function(void);
static int test_string_bitmask_functions(void);
static int test_field_function(void);
static int test_interval_function(void);
static int test_json_valid_function(void);
static int test_json_extract_functions(void);
static int test_json_construction_functions(void);
static int test_json_set_function(void);
static int test_json_introspection_functions(void);
static int test_scalar_subquery_expression(void);
static int test_nullif_function(void);
static int test_isnull_function(void);
static int test_uuid_function(void);
static int test_char_function(void);
static int test_charset_collation_functions(void);
static int test_string_length_functions(void);
static int test_string_codepoint_contexts(void);
static int test_string_case_functions(void);
static int test_string_trim_functions(void);
static int test_string_slice_functions(void);
static int test_string_padding_functions(void);
static int test_string_search_functions(void);
static int test_soundex_function(void);
static int test_find_in_set_function(void);
static int test_strcmp_function(void);
static int test_regexp_like_function(void);
static int test_regexp_string_functions(void);

int main(void) {
    int failures = 0;

    failures += test_current_database_functions();
    failures += test_current_user_identity_functions();
    failures += test_current_role_function();
    failures += test_if_function();
    failures += test_ifnull_function();
    failures += test_coalesce_function();
    failures += test_concat_function();
    failures += test_pipes_as_concat_operator();
    failures += test_concat_ws_function();
    failures += test_replace_function();
    failures += test_reverse_function();
    failures += test_quote_function();
    failures += test_elt_function();
    failures += test_string_bitmask_functions();
    failures += test_field_function();
    failures += test_interval_function();
    failures += test_json_valid_function();
    failures += test_json_extract_functions();
    failures += test_json_construction_functions();
    failures += test_json_set_function();
    failures += test_json_introspection_functions();
    failures += test_scalar_subquery_expression();
    failures += test_nullif_function();
    failures += test_isnull_function();
    failures += test_uuid_function();
    failures += test_char_function();
    failures += test_charset_collation_functions();
    failures += test_string_length_functions();
    failures += test_string_codepoint_contexts();
    failures += test_string_case_functions();
    failures += test_string_trim_functions();
    failures += test_string_slice_functions();
    failures += test_string_padding_functions();
    failures += test_string_search_functions();
    failures += test_soundex_function();
    failures += test_find_in_set_function();
    failures += test_strcmp_function();
    failures += test_regexp_like_function();
    failures += test_regexp_string_functions();

    return failures == 0 ? 0 : 1;
}

static int test_current_database_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SELECT DATABASE(), SCHEMA();", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_DATABASE_FUNCTION,
        "database function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "DATABASE()", "database function span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_SCHEMA_FUNCTION,
        "schema function"
    );
    failures += parser_test_expect_span_text(second_expression, "SCHEMA()", "schema function span");
    failures +=
        parser_test_expect_child_count(first_expression, 0U, "database function child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT database(), schema() FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_DATABASE_FUNCTION,
        "lower database function"
    );
    failures += parser_test_expect_span_text(first_expression, "database()", "lower database span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_SCHEMA_FUNCTION,
        "lower schema function"
    );
    failures += parser_test_expect_span_text(second_expression, "schema()", "lower schema span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "function from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT DATABASE (), (SCHEMA());", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_DATABASE_FUNCTION,
        "spaced database function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "DATABASE ()", "spaced database span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized schema function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
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

    failures += parser_test_parse_sql(
        "SELECT USER(), CURRENT_USER(), CURRENT_USER;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_USER_FUNCTION, "user function");
    failures += parser_test_expect_span_text(first_expression, "USER()", "user function span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_CURRENT_USER_FUNCTION,
        "current user function"
    );
    failures += parser_test_expect_span_text(
        second_expression,
        "CURRENT_USER()",
        "current user function span"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_CURRENT_USER_FUNCTION,
        "bare current user function"
    );
    failures +=
        parser_test_expect_span_text(third_expression, "CURRENT_USER", "bare current user span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT user(), current_user FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_USER_FUNCTION,
        "lower user function"
    );
    failures += parser_test_expect_span_text(first_expression, "user()", "lower user span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_CURRENT_USER_FUNCTION,
        "lower current user keyword"
    );
    failures +=
        parser_test_expect_span_text(second_expression, "current_user", "lower current user span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "identity from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT SESSION_USER(), SYSTEM_USER(), session_user(), system_user() FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    fourth_expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_SESSION_USER_FUNCTION,
        "session user function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "SESSION_USER()", "session user span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_SYSTEM_USER_FUNCTION,
        "system user function"
    );
    failures +=
        parser_test_expect_span_text(second_expression, "SYSTEM_USER()", "system user span");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_SESSION_USER_FUNCTION,
        "lower session user function"
    );
    failures +=
        parser_test_expect_span_text(third_expression, "session_user()", "lower session user span");
    failures += parser_test_expect_node(
        fourth_expression,
        MYLITE_SQL_AST_SYSTEM_USER_FUNCTION,
        "lower system user function"
    );
    failures +=
        parser_test_expect_span_text(fourth_expression, "system_user()", "lower system user span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "alias from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT SESSION_USER(/* inside */), SYSTEM_USER(/* inside */);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_SESSION_USER_FUNCTION,
        "commented session user function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "SESSION_USER(/* inside */)",
        "commented session user span"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_SYSTEM_USER_FUNCTION,
        "commented system user function"
    );
    failures += parser_test_expect_span_text(
        second_expression,
        "SYSTEM_USER(/* inside */)",
        "commented system user span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT USER (), (CURRENT_USER);", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_USER_FUNCTION,
        "spaced user function"
    );
    failures += parser_test_expect_span_text(first_expression, "USER ()", "spaced user span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized current user keyword"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_CURRENT_USER_FUNCTION,
        "wrapped current user keyword"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT (SESSION_USER()), (System_User());",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized session user"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_SESSION_USER_FUNCTION,
        "wrapped session user"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized system user"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_SYSTEM_USER_FUNCTION,
        "wrapped system user"
    );
    failures += parser_test_expect_span_text(
        second_expression,
        "(System_User())",
        "wrapped system user span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE user (user INT);", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "user identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE session_user (system_user INT, session_user INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "identity alias identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE session (local INT, global INT, off INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "nonreserved SET keyword identifier table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(select, 0U),
        "session",
        "session table identifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(select, 1U), 0U), 0U),
        "local",
        "local column identifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(select, 1U), 1U), 0U),
        "global",
        "global column identifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(select, 1U), 2U), 0U),
        "off",
        "off column identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT USER;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IDENTIFIER,
        "bare user identifier"
    );
    failures += parser_test_expect_span_text(first_expression, "USER", "bare user span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT SESSION_USER, SYSTEM_USER;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IDENTIFIER,
        "bare session user identifier"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_IDENTIFIER,
        "bare system user identifier"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "SESSION_USER", "bare session user span");
    failures +=
        parser_test_expect_span_text(second_expression, "SYSTEM_USER", "bare system user span");
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

    failures += parser_test_parse_sql(
        "SELECT CURRENT_ROLE(), Current_Role(), current_role() FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION,
        "current role function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "CURRENT_ROLE()", "current role span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION,
        "mixed-case current role function"
    );
    failures += parser_test_expect_span_text(
        second_expression,
        "Current_Role()",
        "mixed-case current role span"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION,
        "lower current role function"
    );
    failures +=
        parser_test_expect_span_text(third_expression, "current_role()", "lower current role span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "current role from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT CURRENT_ROLE (), (CURRENT_ROLE());",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION,
        "spaced current role function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "CURRENT_ROLE ()",
        "spaced current role span"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized current role function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION,
        "wrapped current role function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT CURRENT_ROLE(1), CURRENT_ROLE(NULL), CURRENT_ROLE('x'), CURRENT_ROLE(1, 2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR,
        "current role integer argument error"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "CURRENT_ROLE(1)",
        "current role integer span"
    );
    arguments = parser_test_child_at(first_expression, 0U);
    failures += parser_test_expect_node(
        arguments,
        MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST,
        "current role argument list"
    );
    failures += parser_test_expect_child_count(arguments, 1U, "current role one argument count");
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "current role integer argument"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR,
        "current role null argument error"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR,
        "current role string argument error"
    );
    arguments =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(select_list, 3U), 0U), 0U);
    failures +=
        parser_test_expect_child_count(arguments, 2U, "current role multiple argument count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE current_role (current_role INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "current role identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT CURRENT_ROLE;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IDENTIFIER,
        "bare current role identifier"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "CURRENT_ROLE", "bare current role span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT CURRENT_ROLE() LIMIT 1;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_if_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    const struct mylite_sql_ast_node *nested = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT IF(1,2,3), If(TRUE,NULL,FALSE), if(+0,-1,+1) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_IF_FUNCTION, "if function");
    failures += parser_test_expect_span_text(first_expression, "IF(1,2,3)", "if function span");
    failures += parser_test_expect_child_count(first_expression, 3U, "if function argument count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "if cond"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "if true"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 2U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "if false"
    );
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_IF_FUNCTION, "mixed-case if");
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "if TRUE"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "if NULL"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 2U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "if FALSE"
    );
    failures += parser_test_expect_node(third_expression, MYLITE_SQL_AST_IF_FUNCTION, "lower if");
    failures += parser_test_expect_operator(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "if positive condition"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(third_expression, 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "if negative branch"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(third_expression, 2U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "if positive branch"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "if from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT IF (1,2,3), (IF(1,2,3)), IF(IF(1,1,0),2,3);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    nested = parser_test_child_at(third_expression, 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_IF_FUNCTION, "spaced if function");
    failures += parser_test_expect_span_text(first_expression, "IF (1,2,3)", "spaced if span");
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized if"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parenthesized, 0U),
        MYLITE_SQL_AST_IF_FUNCTION,
        "wrapped if function"
    );
    failures += parser_test_expect_span_text(parenthesized, "(IF(1,2,3))", "parenthesized if span");
    failures +=
        parser_test_expect_node(third_expression, MYLITE_SQL_AST_IF_FUNCTION, "outer nested if");
    failures += parser_test_expect_node(nested, MYLITE_SQL_AST_IF_FUNCTION, "inner nested if");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT t1.a,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,\n"
        "IF((ROUND(t1.a,2)=1), 2,0)))))))))))))))))))))))))))))) + 1\n"
        "FROM t1",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT IF(1, 2 + 3, 'x');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_IF_FUNCTION, "deferred if args");
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "deferred if arithmetic argument"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "deferred if string argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT IF();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT IF(1);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT IF(1,2);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT IF(1,2,3,4);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_ifnull_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    const struct mylite_sql_ast_node *nested = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT IFNULL(1,2), IfNull(TRUE,NULL), ifnull(+0,-1) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IFNULL_FUNCTION,
        "ifnull function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "IFNULL(1,2)", "ifnull function span");
    failures +=
        parser_test_expect_child_count(first_expression, 2U, "ifnull function argument count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "ifnull value"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "ifnull fallback"
    );
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_IFNULL_FUNCTION, "mixed ifnull");
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "ifnull TRUE"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "ifnull NULL"
    );
    failures +=
        parser_test_expect_node(third_expression, MYLITE_SQL_AST_IFNULL_FUNCTION, "lower ifnull");
    failures += parser_test_expect_operator(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "ifnull positive value"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(third_expression, 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "ifnull negative fallback"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "ifnull from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT IFNULL (NULL,10), (IFNULL(NULL,10)), IFNULL(IFNULL(NULL,1),2), "
        "IFNULL(IF(0,NULL,4),5);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    nested = parser_test_child_at(third_expression, 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_IFNULL_FUNCTION, "spaced ifnull");
    failures +=
        parser_test_expect_span_text(first_expression, "IFNULL (NULL,10)", "spaced ifnull span");
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized ifnull"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parenthesized, 0U),
        MYLITE_SQL_AST_IFNULL_FUNCTION,
        "wrapped ifnull"
    );
    failures += parser_test_expect_span_text(
        parenthesized,
        "(IFNULL(NULL,10))",
        "parenthesized ifnull span"
    );
    failures +=
        parser_test_expect_node(third_expression, MYLITE_SQL_AST_IFNULL_FUNCTION, "outer ifnull");
    failures += parser_test_expect_node(nested, MYLITE_SQL_AST_IFNULL_FUNCTION, "inner ifnull");
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(select_list, 3U), 0U), 0U),
        MYLITE_SQL_AST_IF_FUNCTION,
        "nested if in ifnull"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT IFNULL(1, 2 + 3);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IFNULL_FUNCTION,
        "deferred ifnull"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "deferred ifnull arithmetic fallback"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT IFNULL();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR,
        "empty ifnull argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT IFNULL(1);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR,
        "one ifnull argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT IFNULL(1,2,3);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR,
        "three ifnull argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_coalesce_function(void) {
    enum {
        coalesce_nested_ifnull_item_index = 4,
        coalesce_nested_if_item_index = 5,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    const struct mylite_sql_ast_node *nested = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT COALESCE(1), Coalesce(TRUE,NULL), coalesce(+0,-1,NULL) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_COALESCE_FUNCTION,
        "coalesce function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "COALESCE(1)", "coalesce function span");
    failures +=
        parser_test_expect_child_count(first_expression, 1U, "coalesce function child count");
    arguments = parser_test_child_at(first_expression, 0U);
    failures +=
        parser_test_expect_node(arguments, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, "coalesce args");
    failures += parser_test_expect_child_count(arguments, 1U, "coalesce one argument");
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "coalesce value"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_COALESCE_FUNCTION,
        "mixed coalesce"
    );
    arguments = parser_test_child_at(second_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 2U, "coalesce two arguments");
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "coalesce TRUE"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "coalesce NULL"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_COALESCE_FUNCTION,
        "lower coalesce"
    );
    arguments = parser_test_child_at(third_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 3U, "coalesce three arguments");
    failures += parser_test_expect_operator(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "coalesce positive value"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(arguments, 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "coalesce negative fallback"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 2U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "coalesce null fallback"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "coalesce from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT COALESCE (NULL,10), (COALESCE(NULL,10)), "
        "COALESCE(NULL, COALESCE(NULL,1), 2), COALESCE(IF(0,NULL,4),5), "
        "COALESCE(NULL, IFNULL(COALESCE(NULL,NULL),9)), "
        "COALESCE(NULL, IF(COALESCE(NULL,0),1,2));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    arguments = parser_test_child_at(third_expression, 0U);
    nested = parser_test_child_at(arguments, 1U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_COALESCE_FUNCTION,
        "spaced coalesce"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "COALESCE (NULL,10)",
        "spaced coalesce span"
    );
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized coalesce"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parenthesized, 0U),
        MYLITE_SQL_AST_COALESCE_FUNCTION,
        "wrapped coalesce"
    );
    failures += parser_test_expect_span_text(
        parenthesized,
        "(COALESCE(NULL,10))",
        "parenthesized coalesce span"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_COALESCE_FUNCTION,
        "outer coalesce"
    );
    failures += parser_test_expect_node(nested, MYLITE_SQL_AST_COALESCE_FUNCTION, "inner coalesce");
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(select_list, 3U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_IF_FUNCTION,
        "nested if in coalesce"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(
                    parser_test_child_at(select_list, coalesce_nested_ifnull_item_index),
                    0U
                ),
                0U
            ),
            1U
        ),
        MYLITE_SQL_AST_IFNULL_FUNCTION,
        "nested ifnull in coalesce"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(
                    parser_test_child_at(select_list, coalesce_nested_if_item_index),
                    0U
                ),
                0U
            ),
            1U
        ),
        MYLITE_SQL_AST_IF_FUNCTION,
        "nested if in coalesce"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT COALESCE(1, 2 + 3, 'x');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_COALESCE_FUNCTION,
        "deferred coalesce"
    );
    arguments = parser_test_child_at(first_expression, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(arguments, 1U),
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "deferred coalesce arithmetic argument"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "deferred coalesce string argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT COALESCE();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT COALESCE(NULL, COALESCE());", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT COALESCE(1,,2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_concat_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT CONCAT('a', 'b'), concat(v, '-', id) AS label FROM t ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CONCAT_FUNCTION,
        "concat function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "CONCAT('a', 'b')", "concat function span");
    arguments = parser_test_child_at(first_expression, 0U);
    failures +=
        parser_test_expect_node(arguments, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, "concat args");
    failures += parser_test_expect_child_count(arguments, 2U, "concat two arguments");
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "concat first"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "concat second"
    );
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_CONCAT_FUNCTION, "lower concat");
    arguments = parser_test_child_at(second_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 3U, "concat row arguments");
    failures += parser_test_expect_node(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "concat column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "concat alias"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "concat from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT CONCAT();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CONCAT_ARGUMENT_COUNT_ERROR,
        "concat zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT concat FROM t;", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "concat identifier");
    failures += parser_test_expect_span_text(first_expression, "concat", "concat identifier span");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_pipes_as_concat_operator(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *concat = NULL;
    const struct mylite_sql_ast_node *nested_concat = NULL;
    const struct mylite_sql_ast_node *add = NULL;
    const struct mylite_sql_ast_node *not_expression = NULL;
    const struct mylite_sql_ast_node *negative_concat = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SELECT 1||0;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT,
        "default pipes logical placeholder"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql_with_modes(
        "SELECT 'a'||'b'||'c', 1+2||3, NOT 1||2, -1||2;",
        MYLITE_SQL_PARSE_OK,
        (struct parser_test_parse_modes){.value = MYLITE_SQL_MODE_PIPES_AS_CONCAT},
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    concat = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    nested_concat = parser_test_child_at(concat, 0U);
    add = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    not_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    negative_concat = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);

    failures += parser_test_expect_node(concat, MYLITE_SQL_AST_BINARY_EXPRESSION, "pipes concat");
    failures += parser_test_expect_operator(
        concat,
        MYLITE_SQL_AST_OPERATOR_CONCAT,
        "pipes concat operator"
    );
    failures += parser_test_expect_node(
        nested_concat,
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "left associative pipes"
    );
    failures += parser_test_expect_operator(
        nested_concat,
        MYLITE_SQL_AST_OPERATOR_CONCAT,
        "left associative pipes operator"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(concat, 1U),
        "'c'",
        "rightmost pipes operand"
    );

    failures += parser_test_expect_node(add, MYLITE_SQL_AST_BINARY_EXPRESSION, "pipes plus root");
    failures +=
        parser_test_expect_operator(add, MYLITE_SQL_AST_OPERATOR_ADD, "pipes plus root operator");
    failures += parser_test_expect_node(
        parser_test_child_at(add, 1U),
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "pipes before plus"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(add, 1U),
        MYLITE_SQL_AST_OPERATOR_CONCAT,
        "pipes before plus operator"
    );

    failures += parser_test_expect_node(
        not_expression,
        MYLITE_SQL_AST_UNARY_EXPRESSION,
        "not after pipes precedence"
    );
    failures += parser_test_expect_operator(
        not_expression,
        MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT,
        "not after pipes operator"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(not_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_CONCAT,
        "pipes before not operand"
    );

    failures += parser_test_expect_node(
        negative_concat,
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "unary before pipes"
    );
    failures += parser_test_expect_operator(
        negative_concat,
        MYLITE_SQL_AST_OPERATOR_CONCAT,
        "unary before pipes op"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(negative_concat, 0U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "unary"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_concat_ws_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT CONCAT_WS('-', 'a', 'b'), concat_ws('-', v, id) AS label "
        "FROM t ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CONCAT_WS_FUNCTION,
        "concat_ws function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "CONCAT_WS('-', 'a', 'b')",
        "concat_ws span"
    );
    arguments = parser_test_child_at(first_expression, 0U);
    failures +=
        parser_test_expect_node(arguments, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, "concat_ws args");
    failures += parser_test_expect_child_count(arguments, 3U, "concat_ws three arguments");
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "concat_ws separator"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "concat_ws first"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "concat_ws second"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_CONCAT_WS_FUNCTION,
        "lower concat_ws"
    );
    arguments = parser_test_child_at(second_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 3U, "concat_ws row arguments");
    failures += parser_test_expect_node(
        parser_test_child_at(arguments, 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "concat_ws column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "concat_ws alias"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "concat_ws from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT CONCAT_WS();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CONCAT_WS_ARGUMENT_COUNT_ERROR,
        "concat_ws zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT CONCAT_WS(',');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CONCAT_WS_FUNCTION,
        "concat_ws one argument function"
    );
    arguments = parser_test_child_at(first_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 1U, "concat_ws one argument");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT concat_ws FROM t;", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IDENTIFIER,
        "concat_ws identifier"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "concat_ws", "concat_ws identifier span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DO CONCAT_WS('-', 'a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(expression_list, 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CONCAT_WS_FUNCTION,
        "do concat_ws"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT CONCAT_WS (',', 'a', 'b') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CONCAT_WS_FUNCTION,
        "concat_ws whitespace"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_replace_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT REPLACE('abcabc', 'a', 'x'), replace(v, 'a', 'x') AS label "
        "FROM t ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_REPLACE_FUNCTION,
        "replace function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "REPLACE('abcabc', 'a', 'x')",
        "replace span"
    );
    failures += parser_test_expect_child_count(first_expression, 3U, "replace three children");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "replace str"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "replace search"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "replace replacement"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_REPLACE_FUNCTION,
        "lower replace"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "replace column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "replace alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DO REPLACE('abc', 'b', 'B');", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(expression_list, 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_REPLACE_FUNCTION, "do replace");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT REPLACE ('abc', 'a', 'A') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_REPLACE_FUNCTION,
        "replace whitespace"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT REPLACE();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    failures += parser_test_parse_sql("SELECT REPLACE(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    failures +=
        parser_test_parse_sql("SELECT REPLACE(1, 2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    failures += parser_test_parse_sql(
        "SELECT REPLACE(1, 2, 3, 4);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    failures += parser_test_parse_sql("SELECT REPLACE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);

    return failures;
}

static int test_reverse_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT REVERSE('abc'), reverse(v) AS label FROM t ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_REVERSE_FUNCTION,
        "reverse function"
    );
    failures += parser_test_expect_span_text(first_expression, "REVERSE('abc')", "reverse span");
    failures += parser_test_expect_child_count(first_expression, 1U, "reverse one child");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "reverse str"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_REVERSE_FUNCTION,
        "lower reverse"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "reverse column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "reverse alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DO REVERSE('abc');", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(expression_list, 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_REVERSE_FUNCTION, "do reverse");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT REVERSE ('abc'), (REVERSE('abc')) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_REVERSE_FUNCTION,
        "reverse whitespace"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized reverse"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_REVERSE_FUNCTION,
        "wrapped reverse"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT REVERSE();", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_parse_sql("SELECT REVERSE('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    failures +=
        parser_test_parse_sql("CREATE TABLE reverse(id INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_quote_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT QUOTE('abc'), quote(v) AS label FROM t ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_QUOTE_FUNCTION, "quote function");
    failures += parser_test_expect_span_text(expression, "QUOTE('abc')", "quote span");
    failures += parser_test_expect_child_count(expression, 1U, "quote one child");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "quote str"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_QUOTE_FUNCTION, "lower quote");
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "quote column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "quote alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("DO QUOTE('abc'), QUOTE(NULL);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "quote do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_QUOTE_FUNCTION,
        "do quote"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_QUOTE_FUNCTION,
        "do null quote"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT QUOTE ('abc'), (QUOTE(1.50)) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_QUOTE_FUNCTION, "spaced quote");
    failures += parser_test_expect_span_text(expression, "QUOTE ('abc')", "spaced quote span");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "wrapped quote"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_QUOTE_FUNCTION,
        "inner quote"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT QUOTE();", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_QUOTE_ARGUMENT_COUNT_ERROR,
        "quote zero argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT QUOTE('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_QUOTE_ARGUMENT_COUNT_ERROR,
        "quote two argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE quote_words (quote INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "quote identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_elt_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT ELT(2, 'a', 'b'), elt(TRUE, 10) AS e FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_ELT_FUNCTION, "elt function");
    failures += parser_test_expect_span_text(first_expression, "ELT(2, 'a', 'b')", "elt span");
    arguments = parser_test_child_at(first_expression, 0U);
    failures +=
        parser_test_expect_node(arguments, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, "elt args");
    failures += parser_test_expect_child_count(arguments, 3U, "elt three arguments");
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "elt index"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "elt first"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "elt second"
    );
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_ELT_FUNCTION, "lower elt");
    arguments = parser_test_child_at(second_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 2U, "elt lower arguments");
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "elt true index"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "elt integer"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "elt alias"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "elt from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ELT();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ELT_ARGUMENT_COUNT_ERROR,
        "elt zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ELT(1);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ELT_FUNCTION,
        "elt one argument runtime marker"
    );
    arguments = parser_test_child_at(first_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 1U, "elt one argument count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("DO ELT(2, 'a', 'b'), ELT(NULL, 'x');", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "elt do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_ELT_FUNCTION,
        "do elt"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_ELT_FUNCTION,
        "do null elt"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE elt (elt INT); SELECT elt FROM elt;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_string_bitmask_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT EXPORT_SET(bits, 'Y', 'N', ':', 4), make_set(bits, 'a', 'b') AS made "
        "FROM t ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_EXPORT_SET_FUNCTION,
        "export_set function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "EXPORT_SET(bits, 'Y', 'N', ':', 4)",
        "export_set span"
    );
    arguments = parser_test_child_at(first_expression, 0U);
    failures += parser_test_expect_node(
        arguments,
        MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST,
        "export_set args"
    );
    failures += parser_test_expect_child_count(
        arguments,
        export_set_parser_argument_count,
        "export_set five arguments"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "export_set bits"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "export_set on"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "export_set off"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 3U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "export_set separator"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 4U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "export_set count"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_MAKE_SET_FUNCTION,
        "lower make_set"
    );
    arguments = parser_test_child_at(second_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 3U, "make_set argument count");
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "make_set alias"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "bitmask table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT EXPORT_SET (5,'Y','N',',',4), MAKE_SET (3,'a','b') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_EXPORT_SET_FUNCTION,
        "spaced export_set"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "EXPORT_SET (5,'Y','N',',',4)",
        "spaced export_set span"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_MAKE_SET_FUNCTION,
        "spaced make_set"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT EXPORT_SET();", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_EXPORT_SET_ARGUMENT_COUNT_ERROR,
        "export_set zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT MAKE_SET();", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_MAKE_SET_ARGUMENT_COUNT_ERROR,
        "make_set zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO EXPORT_SET(5,'Y','N',',',4), MAKE_SET(3,'a','b');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "bitmask do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_EXPORT_SET_FUNCTION,
        "do export_set"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_MAKE_SET_FUNCTION,
        "do make_set"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE export_set (make_set INT); SELECT make_set FROM export_set;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_field_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT FIELD('b', 'a', 'b'), field(v, 'x', 'y') AS pos "
        "FROM t ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_FIELD_FUNCTION, "field function");
    failures +=
        parser_test_expect_span_text(first_expression, "FIELD('b', 'a', 'b')", "field span");
    arguments = parser_test_child_at(first_expression, 0U);
    failures +=
        parser_test_expect_node(arguments, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, "field args");
    failures += parser_test_expect_child_count(arguments, 3U, "field three arguments");
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "field search"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "field first"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "field second"
    );
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_FIELD_FUNCTION, "lower field");
    arguments = parser_test_child_at(second_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 3U, "field row arguments");
    failures += parser_test_expect_node(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "field column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "field alias"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "field from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT FIELD();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR,
        "field zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT FIELD('x');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_FIELD_FUNCTION,
        "field one argument runtime marker"
    );
    arguments = parser_test_child_at(first_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 1U, "field one argument count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO FIELD('x', 'a'), FIELD(NULL, NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "field do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_FIELD_FUNCTION,
        "do field"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_FIELD_FUNCTION,
        "do field null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE field (field INT); SELECT field FROM field;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT FIELD('b', 'a', 'b') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_interval_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *thresholds = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT INTERVAL(23, 1, 15, 17, 30, 44, 200), "
        "interval(v, 1, 2) AS bucket FROM t ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_INTERVAL_FUNCTION,
        "interval function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "INTERVAL(23, 1, 15, 17, 30, 44, 200)",
        "interval span"
    );
    failures += parser_test_expect_child_count(first_expression, 2U, "interval child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "interval search"
    );
    thresholds = parser_test_child_at(first_expression, 1U);
    failures += parser_test_expect_node(
        thresholds,
        MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST,
        "interval thresholds"
    );
    failures += parser_test_expect_child_count(
        thresholds,
        interval_parser_threshold_count,
        "interval threshold count"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(thresholds, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "first threshold"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_INTERVAL_FUNCTION,
        "lower interval"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "interval column"
    );
    thresholds = parser_test_child_at(second_expression, 1U);
    failures += parser_test_expect_child_count(thresholds, 2U, "row interval threshold count");
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "interval alias"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "interval from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO INTERVAL(NULL, 1, 2), INTERVAL(TRUE, FALSE, TRUE);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "interval do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_INTERVAL_FUNCTION,
        "do interval null"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_INTERVAL_FUNCTION,
        "do interval boolean"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT INTERVAL();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT INTERVAL(1);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT INTERVAL(1, 2) FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_json_valid_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT JSON_VALID('{\"a\":1}'), json_valid(payload) AS ok "
        "FROM t WHERE JSON_VALID(payload) ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_VALID_FUNCTION,
        "json_valid function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "JSON_VALID('{\"a\":1}')",
        "json_valid span"
    );
    failures += parser_test_expect_child_count(first_expression, 1U, "json_valid argument count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "json_valid string argument"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_JSON_VALID_FUNCTION,
        "lower json_valid"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "json_valid column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "json_valid alias"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "json_valid table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT JSON_VALID();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR,
        "json_valid zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT JSON_VALID('{}', '{}');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR,
        "json_valid two argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO JSON_VALID('{}'), JSON_VALID(NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "json_valid do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_JSON_VALID_FUNCTION,
        "do json_valid"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_JSON_VALID_FUNCTION,
        "do json_valid null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE json_valid (json_valid INT); SELECT json_valid FROM json_valid;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT JSON_VALID('{\"a\":1}') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_json_extract_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_item = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_item = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *operator_expression = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT JSON_EXTRACT('{\"a\":1}', '$.a'), JSON_UNQUOTE('\"x\"') AS value, "
        "JSON_QUOTE('abc') AS quoted FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_item = parser_test_child_at(select_list, 1U);
    second_expression = parser_test_child_at(second_item, 0U);
    third_item = parser_test_child_at(select_list, 2U);
    third_expression = parser_test_child_at(third_item, 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION,
        "json_extract function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "JSON_EXTRACT('{\"a\":1}', '$.a')",
        "json_extract span"
    );
    failures += parser_test_expect_child_count(first_expression, 2U, "json_extract argument count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "json_extract document"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "json_extract path"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION,
        "json_unquote function"
    );
    failures += parser_test_expect_span_text(
        second_expression,
        "JSON_UNQUOTE('\"x\"')",
        "json_unquote span"
    );
    failures +=
        parser_test_expect_child_count(second_expression, 1U, "json_unquote argument count");
    failures += parser_test_expect_node(
        parser_test_child_at(second_item, 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "json_unquote alias"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_JSON_QUOTE_FUNCTION,
        "json_quote function"
    );
    failures +=
        parser_test_expect_span_text(third_expression, "JSON_QUOTE('abc')", "json_quote span");
    failures += parser_test_expect_child_count(third_expression, 1U, "json_quote argument count");
    failures += parser_test_expect_literal(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "json_quote string argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(third_item, 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "json_quote alias"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "json from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT j->'$.a', t.s->>'$.b' FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    operator_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        operator_expression,
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "json arrow"
    );
    failures += parser_test_expect_operator(
        operator_expression,
        MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT,
        "json arrow operator"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(operator_expression, 0U),
        "j",
        "json arrow column"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(operator_expression, 1U),
        "'$.a'",
        "json arrow path"
    );

    operator_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        operator_expression,
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "json unquote arrow"
    );
    failures += parser_test_expect_operator(
        operator_expression,
        MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT,
        "json unquote arrow operator"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(operator_expression, 0U),
        "t.s",
        "json unquote arrow column"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(operator_expression, 1U),
        "'$.b'",
        "json unquote arrow path"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT JSON_EXTRACT();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR,
        "json_extract zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT JSON_EXTRACT('{\"a\":1}');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR,
        "json_extract one argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT JSON_EXTRACT('{}', '$', '$.b');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION,
        "json_extract multipath function"
    );
    failures +=
        parser_test_expect_child_count(first_expression, 1U, "json_extract multipath child count");
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST,
        "json_extract multipath list"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT JSON_UNQUOTE();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR,
        "json_unquote zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT JSON_UNQUOTE('\"a\"', '\"b\"');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR,
        "json_unquote many argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT JSON_QUOTE();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_QUOTE_ARGUMENT_COUNT_ERROR,
        "json_quote zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT JSON_QUOTE('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_QUOTE_ARGUMENT_COUNT_ERROR,
        "json_quote many argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE json_extract (json_unquote INT, json_quote INT); "
        "SELECT json_unquote, json_quote FROM json_extract;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT '{\"a\":1}'->'$.a';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_json_construction_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT JSON_ARRAY(), JSON_ARRAY(1, 'x', NULL, TRUE), "
        "JSON_OBJECT(), JSON_OBJECT('a', 1, 'b', NULL) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_ARRAY_FUNCTION,
        "json_array empty function"
    );
    failures +=
        parser_test_expect_child_count(first_expression, 0U, "json_array empty argument count");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_JSON_ARRAY_FUNCTION,
        "json_array populated function"
    );
    failures += parser_test_expect_child_count(
        second_expression,
        1U,
        "json_array argument-list child count"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(second_expression, 0U),
        4U,
        "json_array populated argument count"
    );

    first_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_OBJECT_FUNCTION,
        "json_object empty function"
    );
    failures +=
        parser_test_expect_child_count(first_expression, 0U, "json_object empty argument count");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_JSON_OBJECT_FUNCTION,
        "json_object populated function"
    );
    failures += parser_test_expect_child_count(
        second_expression,
        1U,
        "json_object argument-list child count"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(second_expression, 0U),
        4U,
        "json_object populated argument count"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "json constructors dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO JSON_ARRAY(1), JSON_OBJECT('a', 1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "json constructors do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_JSON_ARRAY_FUNCTION,
        "do json_array"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_JSON_OBJECT_FUNCTION,
        "do json_object"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE json_array (json_object INT); SELECT json_object FROM json_array;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT JSON_ARRAY(*);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_json_set_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_item = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT JSON_SET('{\"a\":1}', '$.a', 2), json_set(j, '$.b', JSON_ARRAY(1)) "
        "AS changed FROM t ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_item = parser_test_child_at(select_list, 1U);
    second_expression = parser_test_child_at(second_item, 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_SET_FUNCTION,
        "json_set function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "JSON_SET('{\"a\":1}', '$.a', 2)",
        "json_set span"
    );
    failures +=
        parser_test_expect_child_count(first_expression, 1U, "json_set argument-list child count");
    arguments = parser_test_child_at(first_expression, 0U);
    failures +=
        parser_test_expect_node(arguments, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, "json_set args");
    failures += parser_test_expect_child_count(arguments, 3U, "json_set argument count");
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "json_set document"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "json_set path"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 2U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "json_set value"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_JSON_SET_FUNCTION,
        "lower json_set"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_item, 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "json_set alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT JSON_REPLACE('{\"a\":1}', '$.a', 2), "
        "json_replace(j, '$.b', JSON_OBJECT('k', 1)) AS changed FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_item = parser_test_child_at(select_list, 1U);
    second_expression = parser_test_child_at(second_item, 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_REPLACE_FUNCTION,
        "json_replace function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "JSON_REPLACE('{\"a\":1}', '$.a', 2)",
        "json_replace span"
    );
    failures += parser_test_expect_child_count(
        first_expression,
        1U,
        "json_replace argument-list child count"
    );
    arguments = parser_test_child_at(first_expression, 0U);
    failures += parser_test_expect_node(
        arguments,
        MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST,
        "json_replace args"
    );
    failures += parser_test_expect_child_count(arguments, 3U, "json_replace argument count");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_JSON_REPLACE_FUNCTION,
        "lower json_replace"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_item, 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "json_replace alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT JSON_INSERT('{\"a\":1}', '$.b', 2), "
        "json_insert(j, '$.b', JSON_OBJECT('k', 1)) AS changed FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_item = parser_test_child_at(select_list, 1U);
    second_expression = parser_test_child_at(second_item, 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_INSERT_FUNCTION,
        "json_insert function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "JSON_INSERT('{\"a\":1}', '$.b', 2)",
        "json_insert span"
    );
    failures += parser_test_expect_child_count(
        first_expression,
        1U,
        "json_insert argument-list child count"
    );
    arguments = parser_test_child_at(first_expression, 0U);
    failures += parser_test_expect_node(
        arguments,
        MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST,
        "json_insert args"
    );
    failures += parser_test_expect_child_count(arguments, 3U, "json_insert argument count");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_JSON_INSERT_FUNCTION,
        "lower json_insert"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_item, 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "json_insert alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT JSON_REMOVE('{\"a\":1}', '$.a'), json_remove(j, '$.b') AS changed FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_item = parser_test_child_at(select_list, 1U);
    second_expression = parser_test_child_at(second_item, 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_REMOVE_FUNCTION,
        "json_remove function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "JSON_REMOVE('{\"a\":1}', '$.a')",
        "json_remove span"
    );
    failures += parser_test_expect_child_count(
        first_expression,
        1U,
        "json_remove argument-list child count"
    );
    arguments = parser_test_child_at(first_expression, 0U);
    failures += parser_test_expect_node(
        arguments,
        MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST,
        "json_remove args"
    );
    failures += parser_test_expect_child_count(arguments, 2U, "json_remove argument count");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_JSON_REMOVE_FUNCTION,
        "lower json_remove"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_item, 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "json_remove alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT JSON_SET();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_SET_ARGUMENT_COUNT_ERROR,
        "json_set zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT JSON_INSERT();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_INSERT_ARGUMENT_COUNT_ERROR,
        "json_insert zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT JSON_REPLACE();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_REPLACE_ARGUMENT_COUNT_ERROR,
        "json_replace zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT JSON_REMOVE();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_REMOVE_ARGUMENT_COUNT_ERROR,
        "json_remove zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO JSON_SET('{\"a\":1}', '$.a', NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "json_set do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_JSON_SET_FUNCTION,
        "do json_set"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE json_set (json_set INT); SELECT json_set FROM json_set;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE json_insert (json_insert INT); SELECT json_insert FROM json_insert;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE json_replace (json_replace INT); SELECT json_replace FROM json_replace;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE json_remove (json_remove INT); SELECT json_remove FROM json_remove;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT JSON_SET('{\"a\":1}', '$.a', 2) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_json_introspection_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT JSON_TYPE('{\"a\":1}'), JSON_LENGTH('{\"a\":[1,2]}'), "
        "JSON_LENGTH(j, '$.a') FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_TYPE_FUNCTION,
        "json_type function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "JSON_TYPE('{\"a\":1}')", "json_type span");
    failures += parser_test_expect_child_count(first_expression, 1U, "json_type argument count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "json_type document"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_JSON_LENGTH_FUNCTION,
        "json_length function"
    );
    failures += parser_test_expect_span_text(
        second_expression,
        "JSON_LENGTH('{\"a\":[1,2]}')",
        "json_length span"
    );
    failures += parser_test_expect_child_count(second_expression, 1U, "json_length argument count");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_JSON_LENGTH_FUNCTION,
        "json_length path function"
    );
    failures +=
        parser_test_expect_child_count(third_expression, 2U, "json_length path argument count");
    failures += parser_test_expect_node(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "json doc"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(third_expression, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "json path"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT JSON_TYPE(JSON_EXTRACT('{\"a\":[1]}', '$.a')), "
        "JSON_LENGTH(JSON_EXTRACT('{\"a\":[1]}', '$.a'));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION,
        "json_type nested json_extract"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION,
        "json_length nested json_extract"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO JSON_TYPE('{\"a\":1}'), JSON_LENGTH('[1,2]');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "json introspection do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_JSON_TYPE_FUNCTION,
        "do json_type"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_JSON_LENGTH_FUNCTION,
        "do json_length"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT JSON_TYPE();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR,
        "json_type zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT JSON_TYPE('{}', '$');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR,
        "json_type many argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT JSON_LENGTH();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR,
        "json_length zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT JSON_LENGTH('{}', '$', '$.a');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR,
        "json_length many argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE json_type (json_length INT); SELECT json_length FROM json_type;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_scalar_subquery_expression(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *inner_select = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT (SELECT DATABASE()), CONCAT('x', (SELECT 1 FROM DUAL)) AS c;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_SCALAR_SUBQUERY,
        "scalar subquery"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "(SELECT DATABASE())",
        "scalar subquery span"
    );
    inner_select = parser_test_child_at(first_expression, 0U);
    failures += parser_test_expect_node(
        inner_select,
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "scalar subquery select"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(inner_select, 0U), 0U), 0U),
        MYLITE_SQL_AST_DATABASE_FUNCTION,
        "scalar subquery inner database"
    );

    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_CONCAT_FUNCTION,
        "concat with scalar subquery"
    );
    arguments = parser_test_child_at(second_expression, 0U);
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "concat scalar subquery string"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(arguments, 1U),
        MYLITE_SQL_AST_SCALAR_SUBQUERY,
        "concat scalar subquery argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(arguments, 1U), 0U), 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "scalar subquery from dual"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT (SELECT DATABASE(), SCHEMA());",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_SCALAR_SUBQUERY,
        "multi-column scalar subquery marker"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(parser_test_child_at(first_expression, 0U), 0U),
        2U,
        "multi-column scalar subquery select list"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_nullif_function(void) {
    enum {
        nullif_nested_if_item_index = 3,
        nullif_nested_ifnull_item_index = 4,
        nullif_nested_coalesce_item_index = 5,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT NULLIF(1,1), NullIf(TRUE,FALSE), nullif(+0,-0) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_NULLIF_FUNCTION,
        "nullif function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "NULLIF(1,1)", "nullif function span");
    failures +=
        parser_test_expect_child_count(first_expression, 2U, "nullif function argument count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "nullif left value"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "nullif right value"
    );
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_NULLIF_FUNCTION, "mixed nullif");
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "nullif TRUE"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 1U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "nullif FALSE"
    );
    failures +=
        parser_test_expect_node(third_expression, MYLITE_SQL_AST_NULLIF_FUNCTION, "lower nullif");
    failures += parser_test_expect_operator(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "nullif positive value"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(third_expression, 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "nullif negative comparison value"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "nullif from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT NULLIF (1,1), (NULLIF(1,2)), NULLIF(NULLIF(1,1),1), "
        "NULLIF(IF(1,2,3),2), NULLIF(IFNULL(NULL,4),4), "
        "NULLIF(COALESCE(NULL,6),6);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_NULLIF_FUNCTION, "spaced nullif");
    failures +=
        parser_test_expect_span_text(first_expression, "NULLIF (1,1)", "spaced nullif span");
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized nullif"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parenthesized, 0U),
        MYLITE_SQL_AST_NULLIF_FUNCTION,
        "wrapped nullif"
    );
    failures +=
        parser_test_expect_span_text(parenthesized, "(NULLIF(1,2))", "parenthesized nullif span");
    failures +=
        parser_test_expect_node(third_expression, MYLITE_SQL_AST_NULLIF_FUNCTION, "outer nullif");
    failures += parser_test_expect_node(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_NULLIF_FUNCTION,
        "inner nullif"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(select_list, nullif_nested_if_item_index),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_IF_FUNCTION,
        "nested if in nullif"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(select_list, nullif_nested_ifnull_item_index),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_IFNULL_FUNCTION,
        "nested ifnull in nullif"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(select_list, nullif_nested_coalesce_item_index),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_COALESCE_FUNCTION,
        "nested coalesce in nullif"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT NULLIF(1, 2 + 3);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_NULLIF_FUNCTION,
        "deferred nullif"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "deferred nullif arithmetic argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT NULLIF();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR,
        "empty nullif argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT NULLIF(1);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR,
        "one nullif argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT NULLIF(1,2,3);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR,
        "three nullif argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT NULLIF(1,,2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_isnull_function(void) {
    enum {
        isnull_nested_if_item_index = 3,
        isnull_nested_ifnull_item_index = 4,
        isnull_nested_coalesce_item_index = 5,
        isnull_nested_nullif_item_index = 6,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT ISNULL(NULL), IsNull(TRUE), isnull(+0) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ISNULL_FUNCTION,
        "isnull function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "ISNULL(NULL)", "isnull function span");
    failures +=
        parser_test_expect_child_count(first_expression, 1U, "isnull function argument count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "isnull NULL"
    );
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_ISNULL_FUNCTION, "mixed isnull");
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "isnull TRUE"
    );
    failures +=
        parser_test_expect_node(third_expression, MYLITE_SQL_AST_ISNULL_FUNCTION, "lower isnull");
    failures += parser_test_expect_operator(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "isnull positive value"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "isnull from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT ISNULL (NULL), (ISNULL(NULL)), ISNULL(ISNULL(NULL)), "
        "ISNULL(IF(0,NULL,2)), ISNULL(IFNULL(NULL,4)), "
        "ISNULL(COALESCE(NULL,NULL)), ISNULL(NULLIF(1,1));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_ISNULL_FUNCTION, "spaced isnull");
    failures +=
        parser_test_expect_span_text(first_expression, "ISNULL (NULL)", "spaced isnull span");
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized isnull"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parenthesized, 0U),
        MYLITE_SQL_AST_ISNULL_FUNCTION,
        "wrapped isnull"
    );
    failures +=
        parser_test_expect_span_text(parenthesized, "(ISNULL(NULL))", "parenthesized isnull span");
    failures +=
        parser_test_expect_node(third_expression, MYLITE_SQL_AST_ISNULL_FUNCTION, "outer isnull");
    failures += parser_test_expect_node(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_ISNULL_FUNCTION,
        "inner isnull"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(select_list, isnull_nested_if_item_index),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_IF_FUNCTION,
        "nested if in isnull"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(select_list, isnull_nested_ifnull_item_index),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_IFNULL_FUNCTION,
        "nested ifnull in isnull"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(select_list, isnull_nested_coalesce_item_index),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_COALESCE_FUNCTION,
        "nested coalesce in isnull"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(select_list, isnull_nested_nullif_item_index),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_NULLIF_FUNCTION,
        "nested nullif in isnull"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ISNULL(1 + 2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ISNULL_FUNCTION,
        "deferred isnull"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "deferred isnull arithmetic argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ISNULL();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR,
        "empty isnull argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT ISNULL(1,2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR,
        "two isnull argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT ISNULL(1,2,3);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR,
        "three isnull argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ISNULL(,1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_uuid_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT UUID(), Uuid(), uuid() FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_UUID_FUNCTION, "uuid function");
    failures += parser_test_expect_span_text(first_expression, "UUID()", "uuid function span");
    failures += parser_test_expect_child_count(first_expression, 0U, "uuid argument count");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_UUID_FUNCTION,
        "mixed uuid function"
    );
    failures += parser_test_expect_span_text(second_expression, "Uuid()", "mixed uuid span");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_UUID_FUNCTION,
        "lower uuid function"
    );
    failures += parser_test_expect_span_text(third_expression, "uuid()", "lower uuid span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "uuid from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT UUID (), (UUID());", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_UUID_FUNCTION, "spaced uuid");
    failures += parser_test_expect_span_text(first_expression, "UUID ()", "spaced uuid span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "wrapped uuid"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_UUID_FUNCTION,
        "uuid child"
    );
    failures +=
        parser_test_expect_span_text(second_expression, "(UUID())", "parenthesized uuid span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT UUID(NULL), UUID(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR,
        "uuid one argument count error"
    );
    arguments = parser_test_child_at(first_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 1U, "uuid one argument count");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR,
        "uuid multiple argument count error"
    );
    arguments = parser_test_child_at(second_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 2U, "uuid multiple argument count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DO UUID();", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "uuid do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_UUID_FUNCTION,
        "do uuid"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE uuid (uuid INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "uuid identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT UUID;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IDENTIFIER,
        "bare uuid identifier"
    );
    failures += parser_test_expect_span_text(first_expression, "UUID", "bare uuid span");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_char_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT CHAR(65), CHAR(77, 121, 83, 81, 76), CHAR(+1), CHAR(n) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_CHAR_FUNCTION, "char integer function");
    failures += parser_test_expect_span_text(expression, "CHAR(65)", "char integer span");
    arguments = parser_test_child_at(expression, 0U);
    failures +=
        parser_test_expect_node(arguments, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, "char args");
    failures += parser_test_expect_child_count(arguments, 1U, "char one argument");
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "char int"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_CHAR_FUNCTION, "char variadic function");
    arguments = parser_test_child_at(expression, 0U);
    failures += parser_test_expect_child_count(
        arguments,
        char_function_variadic_argument_count,
        "char five arguments"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_CHAR_FUNCTION, "char signed function");
    arguments = parser_test_child_at(expression, 0U);
    failures += parser_test_expect_operator(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "char positive argument"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_CHAR_FUNCTION, "char column function");
    arguments = parser_test_child_at(expression, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "char column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "char from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT CHAR (NULL), (CHAR(TRUE,FALSE)) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_CHAR_FUNCTION, "spaced char");
    failures += parser_test_expect_span_text(expression, "CHAR (NULL)", "spaced char span");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized char"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_CHAR_FUNCTION,
        "wrapped char"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DO CHAR(65), CHAR(NULL);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "char do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_CHAR_FUNCTION,
        "do char"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_CHAR_FUNCTION,
        "do null char"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT CHAR();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT CHAR(65 USING utf8mb4);", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_GENERIC_FUNCTION, "char using");
    failures +=
        parser_test_expect_span_text(expression, "CHAR(65 USING utf8mb4)", "char using span");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_charset_collation_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT CHARSET('abc'), COLLATION(v), CHARSET(CAST('ABC' AS BINARY)), "
        "COERCIBILITY(CONVERT('A' USING BINARY)) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_CHARSET_FUNCTION, "charset function");
    failures += parser_test_expect_span_text(expression, "CHARSET('abc')", "charset span");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "charset literal"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_COLLATION_FUNCTION,
        "collation function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "collation column"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_CHARSET_FUNCTION, "charset cast binary");
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_CAST_BINARY_EXPRESSION,
        "charset cast binary argument"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_COERCIBILITY_FUNCTION,
        "coercibility function"
    );
    failures += parser_test_expect_span_text(
        expression,
        "COERCIBILITY(CONVERT('A' USING BINARY))",
        "coercibility span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION,
        "coercibility using binary argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "charset from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT CONVERT('ABC' USING utf8mb4) COLLATE utf8mb4_bin AS c FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_COLLATE_EXPRESSION, "postfix collate");
    failures += parser_test_expect_span_text(
        expression,
        "CONVERT('ABC' USING utf8mb4) COLLATE utf8mb4_bin",
        "postfix collate span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION,
        "postfix collate expression"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT CHARSET ('a'), (COLLATION(CONVERT('A' USING BINARY))), COERCIBILITY('x') "
        "FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_CHARSET_FUNCTION, "spaced charset");
    failures += parser_test_expect_span_text(expression, "CHARSET ('a')", "spaced charset span");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "wrapped collation"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_COLLATION_FUNCTION,
        "wrapped collation function"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_COERCIBILITY_FUNCTION,
        "dual coercibility"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT CHARSET();", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_parse_sql("SELECT COLLATION('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_parse_sql("SELECT COERCIBILITY();", MYLITE_SQL_PARSE_OK, &result);
    failures +=
        parser_test_parse_sql("SELECT COERCIBILITY('a', 'b');", MYLITE_SQL_PARSE_OK, &result);

    failures += parser_test_parse_sql(
        "DO CHARSET('abc'), COLLATION(NULL), COERCIBILITY(NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "charset do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_CHARSET_FUNCTION,
        "do charset"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_COLLATION_FUNCTION,
        "do collation"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 2U),
        MYLITE_SQL_AST_COERCIBILITY_FUNCTION,
        "do coercibility"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE charset_names (charset INT, collation INT, coercibility INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "charset identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_string_length_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT LENGTH(v), OCTET_LENGTH('a'), BIT_LENGTH(1), CHAR_LENGTH(NULL), "
        "CHARACTER_LENGTH(name) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_LENGTH_FUNCTION, "length function");
    failures += parser_test_expect_span_text(expression, "LENGTH(v)", "length span");
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "length column"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_OCTET_LENGTH_FUNCTION,
        "octet_length function"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "octet_length literal"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_BIT_LENGTH_FUNCTION,
        "bit_length function"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION,
        "char_length function"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 4U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_CHARACTER_LENGTH_FUNCTION,
        "character_length function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "string length from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT LENGTH ('a'), (CHAR_LENGTH('a')), BIT_LENGTH(+1) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_LENGTH_FUNCTION, "spaced length");
    failures += parser_test_expect_span_text(expression, "LENGTH ('a')", "spaced length span");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized char_length"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION,
        "wrapped char_length"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_BIT_LENGTH_FUNCTION,
        "signed bit_length"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "bit_length signed argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT LENGTH();", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_LENGTH_ARGUMENT_COUNT_ERROR,
        "empty length argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT CHARACTER_LENGTH('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_CHARACTER_LENGTH_ARGUMENT_COUNT_ERROR,
        "two character_length argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO LENGTH('a'), OCTET_LENGTH('a'), BIT_LENGTH('a'), CHAR_LENGTH('a');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "string length do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_LENGTH_FUNCTION,
        "do length"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 3U),
        MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION,
        "do char_length"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT id FROM t WHERE LENGTH(v);", MYLITE_SQL_PARSE_OK, &result);
    where_clause = parser_test_child_at(parser_test_child_at(result.root, 0U), 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures +=
        parser_test_expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "length truth where");
    failures += parser_test_expect_node(
        predicate,
        MYLITE_SQL_AST_LENGTH_FUNCTION,
        "length truth predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t WHERE CHAR_LENGTH(v) = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    where_clause = parser_test_child_at(parser_test_child_at(result.root, 0U), 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures += parser_test_expect_node(
        predicate,
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "char_length comparison"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(predicate, 0U),
        MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION,
        "char_length comparison lhs"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t WHERE BIT_LENGTH(v) IS NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    where_clause = parser_test_child_at(parser_test_child_at(result.root, 0U), 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures +=
        parser_test_expect_node(predicate, MYLITE_SQL_AST_IS_NULL_PREDICATE, "bit_length is null");
    failures += parser_test_expect_node(
        parser_test_child_at(predicate, 0U),
        MYLITE_SQL_AST_BIT_LENGTH_FUNCTION,
        "bit_length lhs"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t ORDER BY LENGTH(v) DESC;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        parser_test_first_child_kind(select, MYLITE_SQL_AST_ORDER_BY_CLAUSE),
        MYLITE_SQL_AST_ORDER_BY_CLAUSE,
        "length order by clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE t SET bytes = LENGTH(v), chars = CHAR_LENGTH(v), bits = BIT_LENGTH(v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_UPDATE_STATEMENT,
        "length update statement"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE lengths (length INT, char_length INT, bit_length INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "string length identifiers"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_string_codepoint_contexts(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures +=
        parser_test_parse_sql("SELECT ASCII(v), ORD(v) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_ASCII_FUNCTION, "ascii function");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_ORD_FUNCTION, "ord function");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t WHERE ASCII(v) = 65;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    where_clause = parser_test_child_at(parser_test_child_at(result.root, 0U), 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures +=
        parser_test_expect_node(predicate, MYLITE_SQL_AST_COMPARISON_PREDICATE, "ascii where");
    failures += parser_test_expect_node(
        parser_test_child_at(predicate, 0U),
        MYLITE_SQL_AST_ASCII_FUNCTION,
        "ascii where lhs"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT id FROM t ORDER BY ORD(v);", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        parser_test_first_child_kind(select, MYLITE_SQL_AST_ORDER_BY_CLAUSE),
        MYLITE_SQL_AST_ORDER_BY_CLAUSE,
        "ord order by clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE t SET a = ASCII(v), o = ORD(v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_UPDATE_STATEMENT,
        "ascii ord update statement"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_string_case_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT LOWER(v), LCASE('A'), UPPER(1), UCASE(NULL) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_LOWER_FUNCTION, "lower function");
    failures += parser_test_expect_span_text(expression, "LOWER(v)", "lower span");
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "lower column"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_LCASE_FUNCTION, "lcase function");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "lcase literal"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_UPPER_FUNCTION, "upper function");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "upper integer"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_UCASE_FUNCTION, "ucase function");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "ucase null"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "string case from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT LOWER ('A'), (UPPER('a')), LCASE(+1), UCASE(DATABASE()) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_LOWER_FUNCTION, "spaced lower");
    failures += parser_test_expect_span_text(expression, "LOWER ('A')", "spaced lower span");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized upper"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_UPPER_FUNCTION,
        "wrapped upper"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_LCASE_FUNCTION, "signed lcase");
    failures += parser_test_expect_operator(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "lcase signed argument"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_UCASE_FUNCTION, "database ucase");
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_DATABASE_FUNCTION,
        "ucase database argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT LOWER();", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_LOWER_ARGUMENT_COUNT_ERROR,
        "empty lower argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT LCASE();", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_LCASE_ARGUMENT_COUNT_ERROR,
        "empty lcase argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT UPPER('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_UPPER_ARGUMENT_COUNT_ERROR,
        "two upper argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT UCASE('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_UCASE_ARGUMENT_COUNT_ERROR,
        "two ucase argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO LOWER('A'), LCASE('B'), UPPER('c'), UCASE('d');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "string case do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_LOWER_FUNCTION,
        "do lower"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 3U),
        MYLITE_SQL_AST_UCASE_FUNCTION,
        "do ucase"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE case_words (lower INT, lcase INT, upper INT, ucase INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "string case identifiers"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_string_trim_functions(void) {
    enum {
        trim_trailing_item_index = 5,
        trim_both_default_item_index = 6,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT LTRIM(v), RTRIM('a'), TRIM('  a  '), TRIM('x' FROM 'xxaxx'), "
        "TRIM(LEADING 'x' FROM 'xxa'), TRIM(TRAILING 'x' FROM 'axx'), "
        "TRIM(BOTH FROM ' a ') FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_LTRIM_FUNCTION, "ltrim function");
    failures += parser_test_expect_span_text(expression, "LTRIM(v)", "ltrim span");
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "ltrim column"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_RTRIM_FUNCTION, "rtrim function");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "rtrim literal"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_TRIM_FUNCTION, "trim function");
    failures += parser_test_expect_child_count(expression, 1U, "trim default child count");
    expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_TRIM_FUNCTION, "trim remove function");
    failures += parser_test_expect_child_count(expression, 2U, "trim remove child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "trim value child"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "trim remove child"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 4U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_TRIM_LEADING_FUNCTION,
        "trim leading function"
    );
    failures += parser_test_expect_child_count(expression, 2U, "trim leading child count");
    expression =
        parser_test_child_at(parser_test_child_at(select_list, trim_trailing_item_index), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_TRIM_TRAILING_FUNCTION,
        "trim trailing function"
    );
    expression =
        parser_test_child_at(parser_test_child_at(select_list, trim_both_default_item_index), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_TRIM_FUNCTION,
        "trim both default function"
    );
    failures += parser_test_expect_child_count(expression, 1U, "trim both default child count");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "trim from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT LTRIM ('  a'), (RTRIM('a  ')), TRIM(LEADING FROM '  a'), "
        "TRIM(TRAILING FROM 'a  '), TRIM(+1 FROM 1112111) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_LTRIM_FUNCTION, "spaced ltrim");
    failures += parser_test_expect_span_text(expression, "LTRIM ('  a')", "spaced ltrim span");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized rtrim"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_RTRIM_FUNCTION,
        "wrapped rtrim"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_TRIM_LEADING_FUNCTION,
        "trim leading default"
    );
    failures += parser_test_expect_child_count(expression, 1U, "trim leading default child count");
    expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_TRIM_TRAILING_FUNCTION,
        "trim trailing default"
    );
    failures += parser_test_expect_child_count(expression, 1U, "trim trailing default child count");
    expression = parser_test_child_at(parser_test_child_at(select_list, 4U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_TRIM_FUNCTION, "signed trim remove");
    failures += parser_test_expect_operator(
        parser_test_child_at(expression, 1U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "trim signed remove argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT LTRIM();", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_LTRIM_ARGUMENT_COUNT_ERROR,
        "empty ltrim argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT RTRIM('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_RTRIM_ARGUMENT_COUNT_ERROR,
        "two rtrim argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT TRIM();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT TRIM('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO LTRIM(' a'), RTRIM('a '), TRIM(' a '), TRIM('x' FROM 'xax');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "trim do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_LTRIM_FUNCTION,
        "do ltrim"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 3U),
        MYLITE_SQL_AST_TRIM_FUNCTION,
        "do trim"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE trim_words (ltrim INT, rtrim INT, trim INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "trim identifiers");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_string_slice_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    const struct mylite_sql_ast_node *from_join = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT LEFT(v, 2), RIGHT('abc', +1), LEFT(DATABASE(), 6) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_LEFT_FUNCTION, "left function");
    failures += parser_test_expect_span_text(expression, "LEFT(v, 2)", "left span");
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "left column"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "left length"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_RIGHT_FUNCTION, "right function");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "right literal"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(expression, 1U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "right signed length"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_LEFT_FUNCTION, "database left");
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_DATABASE_FUNCTION,
        "left database argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "string slice from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT SUBSTRING(v, 2), SUBSTR('abcdef', 2, 3), MID(v FROM -4 FOR 2) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_SUBSTRING_FUNCTION,
        "substring function"
    );
    failures += parser_test_expect_span_text(expression, "SUBSTRING(v, 2)", "substring span");
    failures += parser_test_expect_child_count(expression, 2U, "substring arity");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_SUBSTR_FUNCTION, "substr function");
    failures += parser_test_expect_child_count(expression, 3U, "substr arity");
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_MID_FUNCTION, "mid function");
    failures += parser_test_expect_span_text(expression, "MID(v FROM -4 FOR 2)", "mid from span");
    failures += parser_test_expect_operator(
        parser_test_child_at(expression, 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "mid negative position"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "substring from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT SUBSTRING('abcdef' FROM 2), SUBSTR('abcdef' FROM 2 FOR 3), "
        "MID('abcdef' FROM 2 FOR 3) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_SUBSTRING_FUNCTION, "substring from");
    failures += parser_test_expect_child_count(expression, 2U, "substring from arity");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_SUBSTR_FUNCTION, "substr from for");
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_MID_FUNCTION, "mid from for");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT SUBSTRING_INDEX(v, '.', 2), SUBSTRING_INDEX ('abc', 'b', -1) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION,
        "substring_index"
    );
    failures += parser_test_expect_span_text(
        expression,
        "SUBSTRING_INDEX(v, '.', 2)",
        "substring_index span"
    );
    failures += parser_test_expect_child_count(expression, 3U, "substring_index arity");
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "index value"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "index delimiter"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 2U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "index count"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION,
        "spaced substring_index"
    );
    failures += parser_test_expect_span_text(
        expression,
        "SUBSTRING_INDEX ('abc', 'b', -1)",
        "spaced substring_index span"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(expression, 2U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "substring_index signed count"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "substring_index from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT LEFT ('abc', 1), (RIGHT('abc', 1)) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_LEFT_FUNCTION, "spaced left");
    failures += parser_test_expect_span_text(expression, "LEFT ('abc', 1)", "spaced left span");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized right"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_RIGHT_FUNCTION,
        "wrapped right"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT LEFT();", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_parse_sql("SELECT LEFT('a');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_parse_sql("SELECT LEFT('a', 1, 2);", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_parse_sql("SELECT RIGHT();", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_parse_sql("SELECT RIGHT('a');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_parse_sql("SELECT RIGHT('a', 1, 2);", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_parse_sql("SELECT SUBSTRING();", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_parse_sql("SELECT SUBSTRING('a');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_parse_sql("SELECT MID('a', 1, 2, 3);", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_parse_sql(
        "SELECT SUBSTRING ('abc', 1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );

    failures += parser_test_parse_sql("SELECT SUBSTRING_INDEX();", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR,
        "substring_index zero argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT SUBSTRING_INDEX('a');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR,
        "substring_index one argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT SUBSTRING_INDEX('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR,
        "substring_index two argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT SUBSTRING_INDEX('a', 'b', 1, 2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR,
        "substring_index many argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("DO LEFT('abc', 1), RIGHT('abc', 1);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "string slice do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_LEFT_FUNCTION,
        "do left"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_RIGHT_FUNCTION,
        "do right"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO SUBSTRING('abc', 1), SUBSTR('abc' FROM 1), MID('abc', 1, 1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "substring do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_SUBSTRING_FUNCTION,
        "do substring"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 2U),
        MYLITE_SQL_AST_MID_FUNCTION,
        "do mid"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("DO SUBSTRING_INDEX('abc', 'b', 1);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "substring_index do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION,
        "do substring_index"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t WHERE SUBSTRING(v, 1, 1) = 'a';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    where_clause = parser_test_child_at(parser_test_child_at(result.root, 0U), 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures += parser_test_expect_node(
        predicate,
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "substring predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(predicate, 0U),
        MYLITE_SQL_AST_SUBSTRING_FUNCTION,
        "substring predicate lhs"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t WHERE SUBSTR(v FROM 1 FOR 1) <=> NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    where_clause = parser_test_child_at(parser_test_child_at(result.root, 0U), 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures += parser_test_expect_operator(
        predicate,
        MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL,
        "substr null-safe predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(predicate, 0U),
        MYLITE_SQL_AST_SUBSTR_FUNCTION,
        "substr predicate lhs"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t WHERE MID(v, 1, 1) IS NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    where_clause = parser_test_child_at(parser_test_child_at(result.root, 0U), 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures +=
        parser_test_expect_node(predicate, MYLITE_SQL_AST_IS_NULL_PREDICATE, "mid is not null");
    failures += parser_test_expect_node(
        parser_test_child_at(predicate, 0U),
        MYLITE_SQL_AST_MID_FUNCTION,
        "mid predicate lhs"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE substring_words (substring INT, substr INT, mid INT, substring_index INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "substring identifiers"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT l.id FROM lefts l LEFT JOIN rights r ON l.k = r.k;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    from_join = parser_test_child_at(parser_test_child_at(result.root, 0U), 1U);
    failures += parser_test_expect_node(
        from_join,
        MYLITE_SQL_AST_FROM_JOIN,
        "left join after left function"
    );
    if (from_join != NULL &&
        mylite_sql_ast_node_join_kind(from_join) != MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER) {
        fprintf(stderr, "left join after left function: expected left outer join kind\n");
        ++failures;
    }
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_string_padding_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT LPAD(v, 5, '0'), RPAD('abc', +4, 'x'), REPEAT(v, 2), SPACE(3) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_LPAD_FUNCTION, "lpad function");
    failures += parser_test_expect_span_text(expression, "LPAD(v, 5, '0')", "lpad span");
    failures += parser_test_expect_child_count(expression, 3U, "lpad arity");
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "lpad column"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "lpad length"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "lpad pad"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_RPAD_FUNCTION, "rpad function");
    failures += parser_test_expect_operator(
        parser_test_child_at(expression, 1U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "rpad signed length"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_REPEAT_FUNCTION, "repeat function");
    failures += parser_test_expect_child_count(expression, 2U, "repeat arity");
    expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_SPACE_FUNCTION, "space function");
    failures += parser_test_expect_child_count(expression, 1U, "space arity");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "padding from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT LPAD ('hi', 4, '?'), RPAD ('hi', 4, '?'), REPEAT ('x', 2), SPACE (2) "
        "FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_LPAD_FUNCTION, "spaced lpad");
    failures += parser_test_expect_span_text(expression, "LPAD ('hi', 4, '?')", "spaced lpad span");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_RPAD_FUNCTION, "spaced rpad");
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_REPEAT_FUNCTION, "spaced repeat");
    expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_SPACE_FUNCTION, "spaced space");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT LPAD();", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_LPAD_ARGUMENT_COUNT_ERROR,
        "lpad zero argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT LPAD('a', 1);", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_LPAD_ARGUMENT_COUNT_ERROR,
        "lpad two argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT LPAD('a', 1, '0', 'x');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_LPAD_ARGUMENT_COUNT_ERROR,
        "lpad many argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT RPAD('a');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_RPAD_ARGUMENT_COUNT_ERROR,
        "rpad one argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT SPACE();", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_SPACE_ARGUMENT_COUNT_ERROR,
        "space zero argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT SPACE(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_SPACE_ARGUMENT_COUNT_ERROR,
        "space many argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT REPEAT('a');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_parse_sql("SELECT REPEAT('a', 1, 2);", MYLITE_SQL_PARSE_OK, &result);

    failures += parser_test_parse_sql(
        "DO LPAD('abc', 5, '0'), RPAD('abc', 5, '0'), REPEAT('x', 2), SPACE(2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "padding do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_LPAD_FUNCTION,
        "do lpad"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_RPAD_FUNCTION,
        "do rpad"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 2U),
        MYLITE_SQL_AST_REPEAT_FUNCTION,
        "do repeat"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 3U),
        MYLITE_SQL_AST_SPACE_FUNCTION,
        "do space"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE padding_words (lpad INT, rpad INT, space INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "padding identifiers"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_string_search_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT LOCATE('bar', s), LOCATE('bar', s, +2), INSTR(s, 'bar'), "
        "POSITION('bar' IN s) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_LOCATE_FUNCTION, "locate function");
    failures += parser_test_expect_child_count(expression, 2U, "locate arity");
    failures += parser_test_expect_span_text(expression, "LOCATE('bar', s)", "locate span");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_LOCATE_FUNCTION, "locate position");
    failures += parser_test_expect_child_count(expression, 3U, "locate position arity");
    failures += parser_test_expect_operator(
        parser_test_child_at(expression, 2U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "locate signed position"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_INSTR_FUNCTION, "instr function");
    failures += parser_test_expect_child_count(expression, 2U, "instr arity");
    expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_POSITION_FUNCTION, "position function");
    failures += parser_test_expect_child_count(expression, 2U, "position arity");
    failures += parser_test_expect_span_text(expression, "POSITION('bar' IN s)", "position span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT LOCATE ('a','abc'), INSTR ('abc','a') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_LOCATE_FUNCTION,
        "spaced locate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_INSTR_FUNCTION,
        "spaced instr"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT LOCATE();", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_LOCATE_ARGUMENT_COUNT_ERROR,
        "locate zero argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT LOCATE('a');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_LOCATE_ARGUMENT_COUNT_ERROR,
        "locate one argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT LOCATE('a','abc',1,2);", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_LOCATE_ARGUMENT_COUNT_ERROR,
        "locate many argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT INSTR();", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_INSTR_ARGUMENT_COUNT_ERROR,
        "instr zero argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT INSTR('abc');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_INSTR_ARGUMENT_COUNT_ERROR,
        "instr one argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT INSTR('abc','a','x');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_INSTR_ARGUMENT_COUNT_ERROR,
        "instr many argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT POSITION ('a' IN 'abc');",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );

    failures += parser_test_parse_sql(
        "DO LOCATE('a','abc'), INSTR('abc','a'), POSITION('a' IN 'abc');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "string search do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_LOCATE_FUNCTION,
        "do locate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_INSTR_FUNCTION,
        "do instr"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 2U),
        MYLITE_SQL_AST_POSITION_FUNCTION,
        "do position"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE string_search_words (locate INT, instr INT, position INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "search identifiers"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_soundex_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT SOUNDEX('abc'), soundex(v) AS code FROM t ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_SOUNDEX_FUNCTION, "soundex function");
    failures += parser_test_expect_child_count(expression, 1U, "soundex arity");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "soundex literal"
    );
    failures += parser_test_expect_span_text(expression, "SOUNDEX('abc')", "soundex span");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_SOUNDEX_FUNCTION, "lower soundex");
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "soundex column"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT SOUNDEX ('abc') AS a, SOUNDEX(('abc')) AS b FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_SOUNDEX_FUNCTION, "spaced soundex");
    failures += parser_test_expect_span_text(expression, "SOUNDEX ('abc')", "spaced soundex span");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "soundex parenthesized argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT SOUNDEX();", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_SOUNDEX_ARGUMENT_COUNT_ERROR,
        "soundex zero argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT SOUNDEX('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_SOUNDEX_ARGUMENT_COUNT_ERROR,
        "soundex many argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("DO SOUNDEX('abc'), SOUNDEX(NULL);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "soundex do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_SOUNDEX_FUNCTION,
        "do soundex"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_SOUNDEX_FUNCTION,
        "do soundex null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE soundex_words (soundex INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "soundex identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_find_in_set_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT FIND_IN_SET('b', 'a,b'), find_in_set(v, 'x,y') AS pos FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_FIND_IN_SET_FUNCTION,
        "find_in_set function"
    );
    failures += parser_test_expect_child_count(expression, 2U, "find_in_set arity");
    failures +=
        parser_test_expect_span_text(expression, "FIND_IN_SET('b', 'a,b')", "find_in_set span");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_FIND_IN_SET_FUNCTION,
        "lower find_in_set"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "find column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "find alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT FIND_IN_SET();", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_FIND_IN_SET_ARGUMENT_COUNT_ERROR,
        "find_in_set zero argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT FIND_IN_SET('a');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_FIND_IN_SET_ARGUMENT_COUNT_ERROR,
        "find_in_set one argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT FIND_IN_SET('a','a','extra');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_FIND_IN_SET_ARGUMENT_COUNT_ERROR,
        "find_in_set many argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DO FIND_IN_SET('x', 'a,x');", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "find_in_set do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_FIND_IN_SET_FUNCTION,
        "do find_in_set"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t WHERE FIND_IN_SET('red', tags);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    where_clause = parser_test_child_at(statement, 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures += parser_test_expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "find where");
    failures += parser_test_expect_node(
        predicate,
        MYLITE_SQL_AST_FIND_IN_SET_FUNCTION,
        "find truth predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t WHERE FIND_IN_SET('red', tags) > 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    where_clause = parser_test_child_at(parser_test_child_at(result.root, 0U), 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures += parser_test_expect_node(
        predicate,
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "find comparison predicate"
    );
    failures += parser_test_expect_operator(
        predicate,
        MYLITE_SQL_AST_OPERATOR_GREATER,
        "find comparison op"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(predicate, 0U),
        MYLITE_SQL_AST_FIND_IN_SET_FUNCTION,
        "find comparison lhs"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t WHERE FIND_IN_SET('red', tags) IS NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    where_clause = parser_test_child_at(parser_test_child_at(result.root, 0U), 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures +=
        parser_test_expect_node(predicate, MYLITE_SQL_AST_IS_NULL_PREDICATE, "find is null");
    failures += parser_test_expect_operator(
        predicate,
        MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL,
        "find is not null op"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(predicate, 0U),
        MYLITE_SQL_AST_FIND_IN_SET_FUNCTION,
        "find is null lhs"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE t SET note = 'hit' WHERE FIND_IN_SET('red', tags);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(result.root, 0U),
        MYLITE_SQL_AST_UPDATE_STATEMENT,
        "find update predicate"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "DELETE FROM t WHERE FIND_IN_SET('blue', tags) > 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(result.root, 0U),
        MYLITE_SQL_AST_DELETE_STATEMENT,
        "find delete predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_strcmp_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT STRCMP('a', 'b'), strcmp(v, 'x') AS cmp FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_STRCMP_FUNCTION, "strcmp function");
    failures += parser_test_expect_child_count(expression, 2U, "strcmp arity");
    failures += parser_test_expect_span_text(expression, "STRCMP('a', 'b')", "strcmp span");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_STRCMP_FUNCTION, "lower strcmp");
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "strcmp column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "strcmp alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT STRCMP ('b','a') AS cmp FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_STRCMP_FUNCTION, "spaced strcmp");
    failures += parser_test_expect_span_text(expression, "STRCMP ('b','a')", "spaced strcmp span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT STRCMP();", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_STRCMP_ARGUMENT_COUNT_ERROR,
        "strcmp zero argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT STRCMP('a');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_STRCMP_ARGUMENT_COUNT_ERROR,
        "strcmp one argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT STRCMP('a','b','c');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_STRCMP_ARGUMENT_COUNT_ERROR,
        "strcmp many argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO STRCMP('x', 'x'), STRCMP(NULL, 'a');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "strcmp do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_STRCMP_FUNCTION,
        "do strcmp"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_STRCMP_FUNCTION,
        "do null strcmp"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE strcmp_words (strcmp INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "strcmp identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_regexp_like_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT REGEXP_LIKE('abc', '^a'), regexp_like(v, '^ab', 'c') AS hit FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION,
        "regexp_like function"
    );
    failures += parser_test_expect_child_count(expression, 2U, "regexp_like two arguments");
    failures +=
        parser_test_expect_span_text(expression, "REGEXP_LIKE('abc', '^a')", "regexp_like span");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION,
        "lower regexp_like"
    );
    failures += parser_test_expect_child_count(expression, 3U, "regexp_like three arguments");
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "regexp column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "regexp_like alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT REGEXP_LIKE();", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_REGEXP_LIKE_ARGUMENT_COUNT_ERROR,
        "regexp_like zero argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT REGEXP_LIKE('a');", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_REGEXP_LIKE_ARGUMENT_COUNT_ERROR,
        "regexp_like one argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT REGEXP_LIKE('a','a','i','extra');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_REGEXP_LIKE_ARGUMENT_COUNT_ERROR,
        "regexp_like many argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DO REGEXP_LIKE('abc', '^a');", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "regexp_like do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION,
        "do regexp_like"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t WHERE REGEXP_LIKE(v, '^ab');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    where_clause = parser_test_child_at(statement, 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures +=
        parser_test_expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "regexp_like where");
    failures += parser_test_expect_node(
        predicate,
        MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION,
        "regexp_like truth predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t WHERE REGEXP_LIKE(v, '^ab') <=> TRUE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    where_clause = parser_test_child_at(parser_test_child_at(result.root, 0U), 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures += parser_test_expect_node(
        predicate,
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "regexp_like comparison predicate"
    );
    failures += parser_test_expect_operator(
        predicate,
        MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL,
        "regexp_like op"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(predicate, 0U),
        MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION,
        "regexp_like comparison lhs"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t WHERE REGEXP_LIKE(v, '^ab') IS NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    where_clause = parser_test_child_at(parser_test_child_at(result.root, 0U), 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures +=
        parser_test_expect_node(predicate, MYLITE_SQL_AST_IS_NULL_PREDICATE, "regexp_like is null");
    failures += parser_test_expect_operator(
        predicate,
        MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL,
        "regexp_like is null op"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(predicate, 0U),
        MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION,
        "regexp_like is null lhs"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE t SET note = 'hit' WHERE REGEXP_LIKE(v, '^rss_.+$');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(result.root, 0U),
        MYLITE_SQL_AST_UPDATE_STATEMENT,
        "regexp_like update predicate"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "DELETE FROM t WHERE REGEXP_LIKE(v, '^rss_.+$') = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(result.root, 0U),
        MYLITE_SQL_AST_DELETE_STATEMENT,
        "regexp_like delete predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_regexp_string_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT REGEXP_INSTR('abc', 'b'), REGEXP_SUBSTR(v, '^ab'), "
        "REGEXP_REPLACE(v, 'b.', 'X') FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_REGEXP_INSTR_FUNCTION,
        "regexp_instr function"
    );
    arguments = parser_test_child_at(expression, 0U);
    failures += parser_test_expect_node(
        arguments,
        MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST,
        "regexp_instr args"
    );
    failures += parser_test_expect_child_count(arguments, 2U, "regexp_instr argument count");
    failures +=
        parser_test_expect_span_text(expression, "REGEXP_INSTR('abc', 'b')", "regexp_instr span");

    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_REGEXP_SUBSTR_FUNCTION,
        "regexp_substr function"
    );
    arguments = parser_test_child_at(expression, 0U);
    failures += parser_test_expect_child_count(arguments, 2U, "regexp_substr argument count");
    failures += parser_test_expect_node(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "regexp_substr arg"
    );

    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_REGEXP_REPLACE_FUNCTION,
        "regexp_replace function"
    );
    arguments = parser_test_child_at(expression, 0U);
    failures += parser_test_expect_child_count(arguments, 3U, "regexp_replace argument count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT regexp_instr('abc', 'b', 1);", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_REGEXP_INSTR_FUNCTION,
        "regexp_instr optional"
    );
    arguments = parser_test_child_at(expression, 0U);
    failures +=
        parser_test_expect_child_count(arguments, 3U, "regexp_instr optional argument count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT REGEXP_INSTR();", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(result.root, 0U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_REGEXP_INSTR_ARGUMENT_COUNT_ERROR,
        "regexp_instr zero argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO REGEXP_SUBSTR('abc', 'b'), REGEXP_REPLACE('abc', 'a', 'z');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "regexp string do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_REGEXP_SUBSTR_FUNCTION,
        "do regexp_substr"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_REGEXP_REPLACE_FUNCTION,
        "do regexp_replace"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}
