#include "parser_test_support.h"

static int test_case_operator(void);
static int test_do_statement(void);
static int test_version_function(void);
static int test_connection_id_function(void);
static int test_row_count_function(void);
static int test_found_rows_function(void);
static int test_last_insert_id_function(void);
static int test_diagnostics_count_system_variables(void);
static int test_user_variable_assignment_expression(void);
static int test_count_star_aggregate(void);
static int test_min_max_aggregate(void);
static int test_sum_aggregate(void);
static int test_avg_aggregate(void);
static int test_bitwise_aggregate(void);
static int test_group_concat_aggregate(void);

int main(void) {
    int failures = 0;

    failures += test_case_operator();
    failures += test_do_statement();
    failures += test_version_function();
    failures += test_connection_id_function();
    failures += test_row_count_function();
    failures += test_found_rows_function();
    failures += test_last_insert_id_function();
    failures += test_diagnostics_count_system_variables();
    failures += test_user_variable_assignment_expression();
    failures += test_count_star_aggregate();
    failures += test_min_max_aggregate();
    failures += test_sum_aggregate();
    failures += test_avg_aggregate();
    failures += test_bitwise_aggregate();
    failures += test_group_concat_aggregate();

    return failures == 0 ? 0 : 1;
}

static int test_case_operator(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *searched = NULL;
    const struct mylite_sql_ast_node *simple = NULL;
    const struct mylite_sql_ast_node *when_list = NULL;
    const struct mylite_sql_ast_node *when_clause = NULL;
    const struct mylite_sql_ast_node *else_clause = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT CASE WHEN 1 THEN 2 WHEN 0 THEN 3 ELSE 4 END, "
        "CASE 1 WHEN 1 THEN 10 WHEN 2 THEN 20 ELSE 30 END FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    searched = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    simple = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(searched, MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION, "searched case");
    failures += parser_test_expect_span_text(
        searched,
        "CASE WHEN 1 THEN 2 WHEN 0 THEN 3 ELSE 4 END",
        "searched case span"
    );
    failures += parser_test_expect_child_count(searched, 2U, "searched case child count");
    when_list = parser_test_child_at(searched, 0U);
    else_clause = parser_test_child_at(searched, 1U);
    failures +=
        parser_test_expect_node(when_list, MYLITE_SQL_AST_CASE_WHEN_LIST, "searched when list");
    failures += parser_test_expect_child_count(when_list, 2U, "searched when count");
    when_clause = parser_test_child_at(when_list, 0U);
    failures += parser_test_expect_node(
        when_clause,
        MYLITE_SQL_AST_CASE_WHEN_CLAUSE,
        "searched when clause"
    );
    failures += parser_test_expect_span_text(when_clause, "WHEN 1 THEN 2", "searched when span");
    failures += parser_test_expect_literal(
        parser_test_child_at(when_clause, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "searched condition"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(when_clause, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "searched result"
    );
    failures +=
        parser_test_expect_node(else_clause, MYLITE_SQL_AST_CASE_ELSE_CLAUSE, "searched else");
    failures += parser_test_expect_span_text(else_clause, "ELSE 4", "searched else span");

    failures +=
        parser_test_expect_node(simple, MYLITE_SQL_AST_SIMPLE_CASE_EXPRESSION, "simple case");
    failures += parser_test_expect_child_count(simple, 3U, "simple case child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(simple, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "case value"
    );
    when_list = parser_test_child_at(simple, 1U);
    failures +=
        parser_test_expect_node(when_list, MYLITE_SQL_AST_CASE_WHEN_LIST, "simple when list");
    failures += parser_test_expect_child_count(when_list, 2U, "simple when count");
    failures += parser_test_expect_node(
        parser_test_child_at(simple, 2U),
        MYLITE_SQL_AST_CASE_ELSE_CLAUSE,
        "simple else"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "case from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT (CASE WHEN 1 THEN CASE WHEN 0 THEN 2 ELSE 3 END END) AS chosen;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    searched = parser_test_child_at(parenthesized, 0U);
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "wrapped case"
    );
    failures += parser_test_expect_node(
        searched,
        MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION,
        "outer nested case"
    );
    failures += parser_test_expect_child_count(searched, 1U, "no-else searched case child count");
    when_clause = parser_test_child_at(parser_test_child_at(searched, 0U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(when_clause, 1U),
        MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION,
        "inner nested case"
    );
    failures += parser_test_expect_span_text(
        parenthesized,
        "(CASE WHEN 1 THEN CASE WHEN 0 THEN 2 ELSE 3 END END)",
        "wrapped case span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT CASE WHEN 1 THEN 2 END CASE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("CREATE TABLE end (end INT);", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(select, 0U),
        "end",
        "end table identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_do_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "DO 1, NULL, IF(1,2,3), CASE WHEN 1 THEN 4 END;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "do statement");
    failures += parser_test_expect_span_text(
        statement,
        "DO 1, NULL, IF(1,2,3), CASE WHEN 1 THEN 4 END",
        "do statement span"
    );
    failures += parser_test_expect_node(
        expression_list,
        MYLITE_SQL_AST_DO_EXPRESSION_LIST,
        "do expression list"
    );
    failures += parser_test_expect_child_count(expression_list, 4U, "do expression count");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "do int"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "do null"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 2U),
        MYLITE_SQL_AST_IF_FUNCTION,
        "do if"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 3U),
        MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION,
        "do case"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("do +1;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "lowercase do statement");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_UNARY_EXPRESSION,
        "signed do expression"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DO;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("DO 1 FROM DUAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("DO 1 AS x;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("CREATE TABLE do (do INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "do",
        "do table identifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 0U), 0U),
        "do",
        "do column identifier"
    );
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

    failures += parser_test_parse_sql(
        "SELECT VERSION(), Version(), version() FROM DUAL;",
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
        MYLITE_SQL_AST_VERSION_FUNCTION,
        "version function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "VERSION()", "version function span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_VERSION_FUNCTION,
        "mixed-case version function"
    );
    failures +=
        parser_test_expect_span_text(second_expression, "Version()", "mixed-case version span");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_VERSION_FUNCTION,
        "lower version function"
    );
    failures += parser_test_expect_span_text(third_expression, "version()", "lower version span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "version from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT VERSION (), (VERSION());", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_VERSION_FUNCTION,
        "spaced version function"
    );
    failures += parser_test_expect_span_text(first_expression, "VERSION ()", "spaced version span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized version function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_VERSION_FUNCTION,
        "wrapped version function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT VERSION(1), VERSION(NULL), VERSION(1, 2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR,
        "version integer argument error"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "VERSION(1)",
        "version integer argument span"
    );
    arguments = parser_test_child_at(first_expression, 0U);
    failures += parser_test_expect_node(
        arguments,
        MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST,
        "version argument list"
    );
    failures += parser_test_expect_child_count(arguments, 1U, "version one argument count");
    failures += parser_test_expect_literal(
        parser_test_child_at(arguments, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "version integer argument"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR,
        "version null argument error"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR,
        "version multiple argument error"
    );
    arguments = parser_test_child_at(third_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 2U, "version multiple argument count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE version (version INT);", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "version identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT VERSION;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IDENTIFIER,
        "bare version identifier"
    );
    failures += parser_test_expect_span_text(first_expression, "VERSION", "bare version span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT VERSION() LIMIT 1;", MYLITE_SQL_PARSE_OK, &result);
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

    failures += parser_test_parse_sql(
        "SELECT CONNECTION_ID(), Connection_Id(), connection_id() FROM DUAL;",
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
        MYLITE_SQL_AST_CONNECTION_ID_FUNCTION,
        "connection id function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "CONNECTION_ID()", "connection id span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_CONNECTION_ID_FUNCTION,
        "mixed-case connection id function"
    );
    failures += parser_test_expect_span_text(
        second_expression,
        "Connection_Id()",
        "mixed-case connection id span"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_CONNECTION_ID_FUNCTION,
        "lower connection id function"
    );
    failures += parser_test_expect_span_text(
        third_expression,
        "connection_id()",
        "lower connection id span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "connection id from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT CONNECTION_ID (), CONNECTION_ID/**/(), CONNECTION_ID(/* inside */), "
        "(CONNECTION_ID());",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CONNECTION_ID_FUNCTION,
        "spaced connection id function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "CONNECTION_ID ()",
        "spaced connection id span"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_CONNECTION_ID_FUNCTION,
        "comment-before-paren connection id function"
    );
    failures += parser_test_expect_span_text(
        second_expression,
        "CONNECTION_ID/**/()",
        "comment-before-paren connection id span"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_CONNECTION_ID_FUNCTION,
        "commented connection id function"
    );
    failures += parser_test_expect_span_text(
        third_expression,
        "CONNECTION_ID(/* inside */)",
        "commented connection id span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 3U), 0U),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized connection id function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT CONNECTION_ID(1), CONNECTION_ID(NULL), CONNECTION_ID(1, 2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR,
        "connection id integer argument error"
    );
    arguments = parser_test_child_at(first_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 1U, "connection id one argument count");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR,
        "connection id null argument error"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR,
        "connection id multiple argument error"
    );
    arguments = parser_test_child_at(third_expression, 0U);
    failures +=
        parser_test_expect_child_count(arguments, 2U, "connection id multiple argument count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE connection_id (connection_id INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "connection id identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT CONNECTION_ID;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IDENTIFIER,
        "bare connection id identifier"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "CONNECTION_ID", "bare connection id span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT CONNECTION_ID() LIMIT 1;", MYLITE_SQL_PARSE_OK, &result);
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

    failures += parser_test_parse_sql(
        "SELECT ROW_COUNT(), Row_Count(), row_count() FROM DUAL;",
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
        MYLITE_SQL_AST_ROW_COUNT_FUNCTION,
        "row count function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "ROW_COUNT()", "row count function span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_ROW_COUNT_FUNCTION,
        "mixed-case row count function"
    );
    failures +=
        parser_test_expect_span_text(second_expression, "Row_Count()", "mixed-case row count span");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_ROW_COUNT_FUNCTION,
        "lower row count function"
    );
    failures +=
        parser_test_expect_span_text(third_expression, "row_count()", "lower row count span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "row count from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT ROW_COUNT (), (ROW_COUNT());", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ROW_COUNT_FUNCTION,
        "spaced row count function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "ROW_COUNT ()", "spaced row count span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized row count function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_ROW_COUNT_FUNCTION,
        "wrapped row count function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE row_count (row_count INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "row count identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ROW_COUNT;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IDENTIFIER,
        "bare row count identifier"
    );
    failures += parser_test_expect_span_text(first_expression, "ROW_COUNT", "bare row count span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ROW_COUNT(1);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT ROW_COUNT(NULL);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT ROW_COUNT(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT ROW_COUNT() LIMIT 1;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_found_rows_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT FOUND_ROWS(), Found_Rows(), found_rows() FROM DUAL;",
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
        MYLITE_SQL_AST_FOUND_ROWS_FUNCTION,
        "found rows function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "FOUND_ROWS()", "found rows function span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_FOUND_ROWS_FUNCTION,
        "mixed-case found rows function"
    );
    failures += parser_test_expect_span_text(
        second_expression,
        "Found_Rows()",
        "mixed-case found rows span"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_FOUND_ROWS_FUNCTION,
        "lower found rows function"
    );
    failures +=
        parser_test_expect_span_text(third_expression, "found_rows()", "lower found rows span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "found rows from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT FOUND_ROWS (), (FOUND_ROWS());",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_FOUND_ROWS_FUNCTION,
        "spaced found rows function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "FOUND_ROWS ()", "spaced found rows span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized found rows function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_FOUND_ROWS_FUNCTION,
        "wrapped found rows function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE found_rows (found_rows INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "found rows identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT FOUND_ROWS;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IDENTIFIER,
        "bare found rows identifier"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "FOUND_ROWS", "bare found rows span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT FOUND_ROWS(1);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_FOUND_ROWS_ARGUMENT_COUNT_ERROR,
        "found rows one-argument error"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "FOUND_ROWS(1)",
        "found rows one-argument span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT FOUND_ROWS(NULL, 2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_FOUND_ROWS_ARGUMENT_COUNT_ERROR,
        "found rows two-argument error"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "FOUND_ROWS(NULL, 2)",
        "found rows two-argument span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT FOUND_ROWS() LIMIT 1;", MYLITE_SQL_PARSE_OK, &result);
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

    failures += parser_test_parse_sql(
        "SELECT LAST_INSERT_ID(), Last_Insert_Id(), last_insert_id() FROM DUAL;",
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
        MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION,
        "last insert id function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "LAST_INSERT_ID()",
        "last insert id function span"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION,
        "mixed-case last insert id function"
    );
    failures += parser_test_expect_span_text(
        second_expression,
        "Last_Insert_Id()",
        "mixed-case last insert id span"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION,
        "lower last insert id function"
    );
    failures += parser_test_expect_span_text(
        third_expression,
        "last_insert_id()",
        "lower last insert id span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "last insert id from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT LAST_INSERT_ID (), (LAST_INSERT_ID());",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION,
        "spaced last insert id function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "LAST_INSERT_ID ()",
        "spaced last insert id span"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized last insert id function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION,
        "wrapped last insert id function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE last_insert_id (last_insert_id INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "last insert id identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT LAST_INSERT_ID;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IDENTIFIER,
        "bare last insert id identifier"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "LAST_INSERT_ID",
        "bare last insert id span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT LAST_INSERT_ID(1), LAST_INSERT_ID(NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_LAST_INSERT_ID_SET_FUNCTION,
        "last insert id set function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "LAST_INSERT_ID(1)",
        "last insert id set span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL,
        "last insert id arg"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_LAST_INSERT_ID_SET_FUNCTION,
        "last insert id null set function"
    );
    failures += parser_test_expect_span_text(
        second_expression,
        "LAST_INSERT_ID(NULL)",
        "last insert id null set span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT LAST_INSERT_ID(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_LAST_INSERT_ID_ARGUMENT_COUNT_ERROR,
        "last insert id argument count error"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "LAST_INSERT_ID(1, 2)",
        "last insert id arity span"
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT LAST_INSERT_ID() LIMIT 1;", MYLITE_SQL_PARSE_OK, &result);
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

    failures += parser_test_parse_sql(
        "SELECT @@warning_count, @@session.error_count, @@local.Warning_Count FROM DUAL;",
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
        MYLITE_SQL_AST_SYSTEM_VARIABLE,
        "warning count variable"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "@@warning_count", "warning count span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_SYSTEM_VARIABLE,
        "error count variable"
    );
    failures += parser_test_expect_span_text(
        second_expression,
        "@@session.error_count",
        "error count span"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_SYSTEM_VARIABLE,
        "local warning count variable"
    );
    failures += parser_test_expect_span_text(
        third_expression,
        "@@local.Warning_Count",
        "local warning count span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "system variable from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT (@@warning_count), @@session.`warning_count`, @@`error_count`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized warning count variable"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_SYSTEM_VARIABLE,
        "wrapped warning count variable"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_SYSTEM_VARIABLE,
        "quoted warning count variable"
    );
    failures += parser_test_expect_span_text(
        second_expression,
        "@@session.`warning_count`",
        "quoted warning count span"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_SYSTEM_VARIABLE,
        "quoted error count"
    );
    failures += parser_test_expect_span_text(
        third_expression,
        "@@`error_count`",
        "quoted error count span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT @warning_count;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_USER_VARIABLE, "user variable");
    failures +=
        parser_test_expect_span_text(first_expression, "@warning_count", "user variable span");
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT ?;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_user_variable_assignment_expression(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *nested_expression = NULL;
    const struct mylite_sql_ast_node *comparison = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT @a := 1, @b := (@a := @a + 2), @b = 3;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    comparison = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT_EXPRESSION,
        "first assignment expression"
    );
    failures +=
        parser_test_expect_child_count(first_expression, 2U, "first assignment child count");
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_USER_VARIABLE,
        "first assignment target"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "first assignment value"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT_EXPRESSION,
        "second assignment expression"
    );
    nested_expression = parser_test_child_at(parser_test_child_at(second_expression, 1U), 0U);
    failures += parser_test_expect_node(
        nested_expression,
        MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT_EXPRESSION,
        "nested assignment expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(nested_expression, 1U),
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "nested assignment value"
    );
    failures += parser_test_expect_node(
        comparison,
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "equals outside SET remains comparison"
    );
    failures += parser_test_expect_operator(
        comparison,
        MYLITE_SQL_AST_OPERATOR_EQUAL,
        "user variable equality operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DO @d := 5;", MYLITE_SQL_PARSE_OK, &result);
    first_expression =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT_EXPRESSION,
        "DO assignment expression"
    );
    failures += parser_test_expect_span_text(first_expression, "@d := 5", "DO assignment span");
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

    failures += parser_test_parse_sql(
        "SELECT COUNT(*), count(*), Count( * ) FROM DUAL;",
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
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "count star function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "COUNT(*)", "count star function span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "lower count star function"
    );
    failures +=
        parser_test_expect_span_text(second_expression, "count(*)", "lower count star span");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "mixed count star function"
    );
    failures +=
        parser_test_expect_span_text(third_expression, "Count( * )", "mixed count star span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "count from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT COUNT(/* inside */*) FROM t WHERE id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "commented count star function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "COUNT(/* inside */*)",
        "commented count star span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "count from table"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 2U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "count where"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT COUNT(id), count(n), Count( nn ) FROM t;",
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
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "count column function"
    );
    failures += parser_test_expect_span_text(first_expression, "COUNT(id)", "count column span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "lower count column function"
    );
    failures +=
        parser_test_expect_span_text(second_expression, "count(n)", "lower count column span");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "mixed count column function"
    );
    failures +=
        parser_test_expect_span_text(third_expression, "Count( nn )", "mixed count column span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "count column table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT COUNT(/* inside */n), COUNT(`weird name`) FROM t WHERE id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "commented count column function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "COUNT(/* inside */n)",
        "commented count column span"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "quoted count column function"
    );
    failures += parser_test_expect_span_text(
        second_expression,
        "COUNT(`weird name`)",
        "quoted count column span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 2U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "count column where"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT COUNT(t.n), COUNT(db.t.n), COUNT(DISTINCT t.nn) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified count argument"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "COUNT(t.n)", "qualified count span");
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "schema-qualified count argument"
    );
    failures +=
        parser_test_expect_span_text(second_expression, "COUNT(db.t.n)", "schema count span");
    failures += parser_test_expect_node(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified count distinct argument"
    );
    failures += parser_test_expect_span_text(
        third_expression,
        "COUNT(DISTINCT t.nn)",
        "qualified distinct span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT COUNT(1), count(-1), Count( +1 ) FROM t;",
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
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "count integer literal function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "COUNT(1)", "count integer literal span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "lower count negative literal function"
    );
    failures +=
        parser_test_expect_span_text(second_expression, "count(-1)", "lower count negative span");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "mixed count positive literal function"
    );
    failures +=
        parser_test_expect_span_text(third_expression, "Count( +1 )", "mixed count positive span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "count literal table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT COUNT(TRUE), count(false), Count( TRUE ) FROM t;",
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
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "count true literal function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "COUNT(TRUE)", "count true literal span");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "count true literal argument"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "lower count false literal function"
    );
    failures +=
        parser_test_expect_span_text(second_expression, "count(false)", "lower count false span");
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "count false literal argument"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "mixed count true literal function"
    );
    failures +=
        parser_test_expect_span_text(third_expression, "Count( TRUE )", "mixed count true span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "count boolean table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT COUNT(NULLIF(`meta_value` LIKE '%\\\"administrator\\\"%', false)), "
        "COUNT(NULLIF(meta_value = 'a:0:{}', FALSE)), COUNT(*) FROM wp_usermeta "
        "INNER JOIN wp_users ON user_id = ID WHERE meta_key = 'wp_capabilities';",
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
        MYLITE_SQL_AST_COUNT_EXPRESSION_FUNCTION,
        "count nullif like expression function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_NULLIF_FUNCTION,
        "count nullif like argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(first_expression, 0U), 0U),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "count nullif like predicate"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(first_expression, 0U), 1U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "count nullif false argument"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_EXPRESSION_FUNCTION,
        "count nullif equality expression function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(second_expression, 0U), 0U),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "count nullif equality predicate"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "count nullif mixed count star"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_JOIN,
        "count nullif joined source"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT COUNT(/* inside */NULL) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "commented count null literal function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "COUNT(/* inside */NULL)",
        "commented count null literal span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "count literal dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT COUNT(/* inside */FALSE) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "commented count false literal function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "COUNT(/* inside */FALSE)",
        "commented count false literal span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "count boolean dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT COUNT(DISTINCT n), count(distinct `weird name`), Count( DISTINCT nn ) FROM t;",
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
        MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION,
        "count distinct column function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "COUNT(DISTINCT n)",
        "count distinct column span"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION,
        "lower count distinct quoted function"
    );
    failures += parser_test_expect_span_text(
        second_expression,
        "count(distinct `weird name`)",
        "lower count distinct quoted span"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION,
        "mixed count distinct column function"
    );
    failures += parser_test_expect_span_text(
        third_expression,
        "Count( DISTINCT nn )",
        "mixed count distinct column span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "count distinct table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT COUNT(DISTINCT(t.n)), COUNT(DISTINCT (db.t.nn)) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION,
        "parenthesized qualified count distinct function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "COUNT(DISTINCT(t.n))",
        "parenthesized qualified count distinct span"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(first_expression, 0U),
        "t.n",
        "parenthesized qualified count distinct argument"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION,
        "spaced parenthesized qualified count distinct function"
    );
    failures += parser_test_expect_span_text(
        second_expression,
        "COUNT(DISTINCT (db.t.nn))",
        "spaced parenthesized qualified count distinct span"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(second_expression, 0U),
        "db.t.nn",
        "spaced parenthesized qualified count distinct argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT COUNT(/* inside */DISTINCT n), COUNT(DISTINCT/* inside */n) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION,
        "commented count distinct keyword function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "COUNT(/* inside */DISTINCT n)",
        "commented count distinct keyword span"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION,
        "commented count distinct argument function"
    );
    failures += parser_test_expect_span_text(
        second_expression,
        "COUNT(DISTINCT/* inside */n)",
        "commented count distinct argument span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT (COUNT(n));", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized count column function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "wrapped count column function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT (COUNT(DISTINCT n));", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized count distinct column function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION,
        "wrapped count distinct column function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT COUNT(*), COUNT(id);", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "mixed count star"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "mixed count column"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT (COUNT(*));", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized count star function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "wrapped count star function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE count (count INT);", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "count identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT COUNT;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IDENTIFIER,
        "bare count identifier"
    );
    failures += parser_test_expect_span_text(first_expression, "COUNT", "bare count span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT COUNT FROM count;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IDENTIFIER,
        "count column identifier"
    );
    failures += parser_test_expect_span_text(first_expression, "COUNT", "count column span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT COUNT (*) FROM DUAL;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT COUNT/**/(*) FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT COUNT();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT COUNT(1.0);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT COUNT('x');", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT COUNT(+TRUE);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT COUNT(-FALSE);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT COUNT(NOT TRUE);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT COUNT(id + 1) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT COUNT(TRUE + 1) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT COUNT(id, n) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT COUNT(t.*) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT COUNT (DISTINCT id) FROM t;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT COUNT/**/(DISTINCT id) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT COUNT(DISTINCT *) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT COUNT(DISTINCT id, n) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "count distinct expression-list placeholder"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "COUNT(DISTINCT id, n)",
        "count distinct expression-list span"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT COUNT(DISTINCT CONCAT(id, n)) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "count distinct expression placeholder"
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT COUNT(DISTINCT 1) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "count distinct literal placeholder"
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT COUNT(DISTINCT TRUE) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "count distinct boolean placeholder"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT COUNT(DISTINCT id + 1) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
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

    failures += parser_test_parse_sql(
        "SELECT MIN(id), max(n), Max( n ) FROM t;",
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
        MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION,
        "min aggregate"
    );
    failures += parser_test_expect_span_text(first_expression, "MIN(id)", "min aggregate span");
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "min arg"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(first_expression, 0U),
        "id",
        "min arg span"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION,
        "lower max"
    );
    failures += parser_test_expect_span_text(second_expression, "max(n)", "lower max span");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION,
        "mixed max"
    );
    failures += parser_test_expect_span_text(third_expression, "Max( n )", "mixed max span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "min max from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT MAX(/* inside */n) FROM t WHERE id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION,
        "commented max aggregate"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "MAX(/* inside */n)", "commented max span");
    failures += parser_test_expect_span_text(
        parser_test_child_at(first_expression, 0U),
        "n",
        "commented max arg span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 2U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "max where"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT MIN(t.id), MAX(db.t.n) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified min argument"
    );
    failures += parser_test_expect_span_text(first_expression, "MIN(t.id)", "qualified min span");
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "schema-qualified max argument"
    );
    failures += parser_test_expect_span_text(second_expression, "MAX(db.t.n)", "schema max span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT MIN(LOWER(name)), MAX(CONCAT(name, '-x')) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LOWER_FUNCTION,
        "min lower argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_CONCAT_FUNCTION,
        "max concat argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT (MIN(id));", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized min aggregate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION,
        "wrapped min aggregate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE min (max INT, min INT);", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "min max identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT MIN, MAX;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare min identifier");
    failures += parser_test_expect_span_text(first_expression, "MIN", "bare min span");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_IDENTIFIER,
        "bare max identifier"
    );
    failures += parser_test_expect_span_text(second_expression, "MAX", "bare max span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT MIN (id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT MAX/**/(id) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT MIN();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT MAX(1);", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION,
        "max literal aggregate"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT MIN(NULL);", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION,
        "min null aggregate"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT MIN(id, n) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT MIN(*) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT MIN(DISTINCT id) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_sum_aggregate(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *sum_argument = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT SUM(id), sum(n), Sum( n ) FROM t;",
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
        MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION,
        "sum aggregate"
    );
    failures += parser_test_expect_span_text(first_expression, "SUM(id)", "sum aggregate span");
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "sum arg"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(first_expression, 0U),
        "id",
        "sum arg span"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION,
        "lower sum"
    );
    failures += parser_test_expect_span_text(second_expression, "sum(n)", "lower sum span");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION,
        "mixed sum"
    );
    failures += parser_test_expect_span_text(third_expression, "Sum( n )", "mixed sum span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "sum from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT SUM(/* inside */n) FROM t WHERE id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION,
        "commented sum aggregate"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "SUM(/* inside */n)", "commented sum span");
    failures += parser_test_expect_span_text(
        parser_test_child_at(first_expression, 0U),
        "n",
        "commented sum arg span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 2U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "sum where"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT SUM(t.id), SUM(db.t.n) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified sum argument"
    );
    failures += parser_test_expect_span_text(first_expression, "SUM(t.id)", "qualified sum span");
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "schema-qualified sum argument"
    );
    failures += parser_test_expect_span_text(second_expression, "SUM(db.t.n)", "schema sum span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT SUM(data_length + index_length) FROM information_schema.TABLES;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION,
        "sum column plus column aggregate"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "SUM(data_length + index_length)",
        "sum column plus column span"
    );
    sum_argument = parser_test_child_at(first_expression, 0U);
    failures += parser_test_expect_node(
        sum_argument,
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "sum column plus column argument"
    );
    failures +=
        parser_test_expect_operator(sum_argument, MYLITE_SQL_AST_OPERATOR_ADD, "sum add operator");
    failures += parser_test_expect_span_text(
        parser_test_child_at(sum_argument, 0U),
        "data_length",
        "sum add left column"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(sum_argument, 1U),
        "index_length",
        "sum add right column"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT SUM(LENGTH(option_value)) FROM options;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION,
        "sum length aggregate"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "SUM(LENGTH(option_value))",
        "sum length span"
    );
    sum_argument = parser_test_child_at(first_expression, 0U);
    failures +=
        parser_test_expect_node(sum_argument, MYLITE_SQL_AST_LENGTH_FUNCTION, "sum length arg");
    failures += parser_test_expect_span_text(
        parser_test_child_at(sum_argument, 0U),
        "option_value",
        "sum length column"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT SUM(ABS(i)) FROM numbers;", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    sum_argument = parser_test_child_at(first_expression, 0U);
    failures += parser_test_expect_node(
        sum_argument,
        MYLITE_SQL_AST_ABS_FUNCTION,
        "sum numeric function argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT (SUM(id));", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized sum aggregate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION,
        "wrapped sum aggregate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("CREATE TABLE sum (sum INT);", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "sum identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT SUM;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare sum identifier");
    failures += parser_test_expect_span_text(first_expression, "SUM", "bare sum span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT SUM (id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT SUM/**/(id) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT SUM();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT SUM(1);", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION,
        "sum literal aggregate"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT SUM(NULL);", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION,
        "sum null aggregate"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT SUM(id + 1) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "sum identifier plus literal argument"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT SUM(id, n) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT SUM(*) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT SUM(DISTINCT id) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "sum distinct placeholder"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_avg_aggregate(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *fourth_expression = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT AVG(id), avg(n), Avg( n ) FROM t;",
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
        MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION,
        "avg aggregate"
    );
    failures += parser_test_expect_span_text(first_expression, "AVG(id)", "avg aggregate span");
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "avg arg"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(first_expression, 0U),
        "id",
        "avg arg span"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION,
        "lower avg"
    );
    failures += parser_test_expect_span_text(second_expression, "avg(n)", "lower avg span");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION,
        "mixed avg"
    );
    failures += parser_test_expect_span_text(third_expression, "Avg( n )", "mixed avg span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "avg from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT AVG (n), AVG/**/(n), AVG /*x*/ (n), AVG( /*x*/ n ) FROM t;",
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
        MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION,
        "avg whitespace aggregate"
    );
    failures += parser_test_expect_span_text(first_expression, "AVG (n)", "avg whitespace span");
    failures += parser_test_expect_span_text(second_expression, "AVG/**/(n)", "avg comment span");
    failures +=
        parser_test_expect_span_text(third_expression, "AVG /*x*/ (n)", "avg spaced comment span");
    failures +=
        parser_test_expect_span_text(fourth_expression, "AVG( /*x*/ n )", "avg inner comment span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT AVG(t.id), AVG(db.t.n) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified avg argument"
    );
    failures += parser_test_expect_span_text(first_expression, "AVG(t.id)", "qualified avg span");
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "schema-qualified avg argument"
    );
    failures += parser_test_expect_span_text(second_expression, "AVG(db.t.n)", "schema avg span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT (AVG(id));", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized avg aggregate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION,
        "wrapped avg aggregate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("CREATE TABLE avg (avg INT);", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "avg identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT AVG;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare avg identifier");
    failures += parser_test_expect_span_text(first_expression, "AVG", "bare avg span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT AVG();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT AVG(1);", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION,
        "avg literal aggregate"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT AVG(NULL);", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION,
        "avg null aggregate"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT AVG(id + 1) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "avg identifier plus literal argument"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT AVG(id, n) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT AVG(*) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT AVG(DISTINCT id) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "avg distinct placeholder"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_bitwise_aggregate(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *fourth_expression = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT BIT_AND(id), bit_or(n), Bit_Xor( n ) FROM t;",
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
        MYLITE_SQL_AST_BIT_AND_AGGREGATE_FUNCTION,
        "bit_and aggregate"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "BIT_AND(id)", "bit_and aggregate span");
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "bit_and arg"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(first_expression, 0U),
        "id",
        "bit_and arg span"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_BIT_OR_AGGREGATE_FUNCTION,
        "lower bit_or"
    );
    failures += parser_test_expect_span_text(second_expression, "bit_or(n)", "lower bit_or span");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_BIT_XOR_AGGREGATE_FUNCTION,
        "mixed bit_xor"
    );
    failures +=
        parser_test_expect_span_text(third_expression, "Bit_Xor( n )", "mixed bit_xor span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT BIT_AND(/*x*/n), BIT_OR(t.n), BIT_XOR(db.t.n) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_span_text(first_expression, "BIT_AND(/*x*/n)", "commented bit_and span");
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified bit_or argument"
    );
    failures +=
        parser_test_expect_span_text(second_expression, "BIT_OR(t.n)", "qualified bit_or span");
    failures += parser_test_expect_node(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "schema-qualified bit_xor argument"
    );
    failures +=
        parser_test_expect_span_text(third_expression, "BIT_XOR(db.t.n)", "schema bit_xor span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT (BIT_AND(id));", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized bit_and aggregate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_BIT_AND_AGGREGATE_FUNCTION,
        "wrapped bit_and aggregate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT BIT_AND (n), BIT_OR/**/(n), BIT_XOR /*x*/ (n) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("CREATE TABLE bit_and (bit_or INT);", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "bitwise identifier table"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT BIT_XOR;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    fourth_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        fourth_expression,
        MYLITE_SQL_AST_IDENTIFIER,
        "bare bit_xor identifier"
    );
    failures += parser_test_expect_span_text(fourth_expression, "BIT_XOR", "bare bit_xor span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT BIT_AND();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT BIT_AND(1);", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_BIT_AND_AGGREGATE_FUNCTION,
        "bit_and literal aggregate"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT BIT_OR(NULL);", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_BIT_OR_AGGREGATE_FUNCTION,
        "bit_or null aggregate"
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT BIT_OR(id + 1) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "bit_or arithmetic argument"
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT BIT_XOR(id, n) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT BIT_AND(*) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT BIT_XOR(DISTINCT id) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_group_concat_aggregate(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT GROUP_CONCAT(id), group_concat(name ORDER BY id), "
        "Group_Concat(name ORDER BY id DESC SEPARATOR '|') FROM t;",
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
        MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION,
        "group_concat aggregate"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "GROUP_CONCAT(id)", "group_concat span");
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "group_concat arg"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION,
        "lower group_concat aggregate"
    );
    failures += parser_test_expect_span_text(
        second_expression,
        "group_concat(name ORDER BY id)",
        "ordered span"
    );
    order_clause = parser_test_child_at(second_expression, 1U);
    failures +=
        parser_test_expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "group_concat order");
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_clause, 0U),
        "id",
        "group_concat order key"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION,
        "mixed group_concat aggregate"
    );
    order_clause = parser_test_child_at(third_expression, 1U);
    failures +=
        parser_test_expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "desc order clause");
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "desc order direction"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(third_expression, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "separator"
    );
    failures += parser_test_expect_span_text(
        third_expression,
        "Group_Concat(name ORDER BY id DESC SEPARATOR '|')",
        "desc separator span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT GROUP_CONCAT(t.name ORDER BY t.id ASC SEPARATOR \"\") FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified group_concat argument"
    );
    order_clause = parser_test_child_at(first_expression, 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(order_clause, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified group_concat order key"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "empty sep"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT GROUP_CONCAT (id) FROM t;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql_with_ignore_space(
        "SELECT GROUP_CONCAT (id ORDER BY id) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION,
        "ignore_space group_concat"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT GROUP_CONCAT;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IDENTIFIER,
        "bare group_concat identifier"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "GROUP_CONCAT", "bare group_concat span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT GROUP_CONCAT(DISTINCT id) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "group_concat distinct placeholder"
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT GROUP_CONCAT(id, n) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "multi-argument group_concat placeholder"
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT GROUP_CONCAT(id + 1) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT GROUP_CONCAT(id ORDER BY 1) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT GROUP_CONCAT(id ORDER BY id, n) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT GROUP_CONCAT(id SEPARATOR NULL) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT GROUP_CONCAT(id SEPARATOR 1) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}
