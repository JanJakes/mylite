#include "parser_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int test_empty_script(void);
static int test_prepared_parameters(void);
static void collect_parameter_indices(
    const struct mylite_sql_ast_node *node,
    size_t *indices,
    size_t capacity,
    size_t *count
);
static int test_use_statements(void);
static int test_select_expression_list(void);
static int test_unary_and_parenthesized_expression(void);
static int test_literal_categories(void);
static int test_qualified_identifier_keyword_part(void);
static int test_comments_are_skipped(void);
static int test_source_span_bounds(void);

int main(void) {
    int failures = 0;

    failures += test_empty_script();
    failures += test_prepared_parameters();
    failures += test_use_statements();
    failures += test_select_expression_list();
    failures += test_unary_and_parenthesized_expression();
    failures += test_literal_categories();
    failures += test_qualified_identifier_keyword_part();
    failures += test_comments_are_skipped();
    failures += test_source_span_bounds();

    return failures == 0 ? 0 : 1;
}

static int test_empty_script(void) {
    struct mylite_sql_parse_result result;
    int failures = 0;

    failures += parser_test_parse_sql("", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(result.root, MYLITE_SQL_AST_SCRIPT, "empty root");
    failures += parser_test_expect_child_count(result.root, 0U, "empty root");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT 1;", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_child_count(result.root, 1U, "trailing semicolon root");
    failures += parser_test_expect_node(
        parser_test_child_at(result.root, 0U),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "trailing semicolon statement"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_prepared_parameters(void) {
    static const struct {
        const char *sql;
        size_t parameter_count;
    } extended_cases[] = {
        {"SELECT * FROM t WHERE c REGEXP ? AND d BETWEEN ? AND ?", 3U},
        {"UPDATE t SET c = c + ?, d = d - ?, e = e * ? WHERE id = ?", 4U},
        {"SELECT CONCAT(?, CONCAT_WS(',', ?, ?)), LEAST(?, ?), COUNT(*) + ? FROM t", 6U},
        {"SELECT CONVERT(? USING utf8mb4) FROM t", 1U},
    };

    static const char select_sql[] =
        "SELECT '?', ?, ? /* ? */ FROM t WHERE c = ? ORDER BY c LIMIT ? OFFSET ?";
    static const char like_sql[] = "SELECT * FROM t WHERE c LIKE ? ESCAPE '\\\\' AND d = ?";
    static const char insert_sql[] = "INSERT INTO t VALUES (?, '?', ?)";
    static const char update_sql[] = "UPDATE t SET c = ? WHERE id = ? LIMIT ?";
    struct mylite_sql_parse_result result;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
    size_t indices[8] = {0};
    size_t count = 0U;
    int failures = 0;

    failures += parser_test_parse_sql(select_sql, MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = select_sql,
            .length = strlen(select_sql),
            .modes = 0U,
            .allow_parameters = true,
        },
        &result
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        fprintf(
            stderr,
            "prepared SELECT: %s at offset %zu '%.*s'\n",
            mylite_sql_parse_status_name(status),
            result.error_token.offset,
            (int)result.error_token.length,
            result.error_token.text == NULL ? "" : result.error_token.text
        );
        ++failures;
    }
    collect_parameter_indices(result.root, indices, 8U, &count);
    failures += parser_test_expect_true(
        result.parameter_count == 5U,
        "prepared SELECT reported parameter count"
    );
    if (count != 5U) {
        fprintf(stderr, "prepared SELECT parameter count: expected 5, got %zu\n", count);
        ++failures;
    }
    for (size_t index = 0U; index < count; ++index) {
        failures += parser_test_expect_true(
            indices[index] == index,
            "prepared SELECT parameter lexical index"
        );
    }
    mylite_sql_parse_result_deinit(&result);

    count = 0U;
    status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = like_sql,
            .length = strlen(like_sql),
            .modes = 0U,
            .allow_parameters = true,
        },
        &result
    );
    failures += parser_test_expect_true(
        status == MYLITE_SQL_PARSE_OK,
        "prepared LIKE parameter parse status"
    );
    collect_parameter_indices(result.root, indices, 8U, &count);
    failures += parser_test_expect_true(
        result.parameter_count == 2U,
        "prepared LIKE reported parameter count"
    );
    failures += parser_test_expect_true(count == 2U, "prepared LIKE parameter count");
    failures += parser_test_expect_true(indices[0] == 0U, "prepared LIKE first parameter");
    failures += parser_test_expect_true(indices[1] == 1U, "prepared LIKE second parameter");
    mylite_sql_parse_result_deinit(&result);

    count = 0U;
    failures += mylite_sql_parse(
                    (struct mylite_sql_parse_config){
                        .input = insert_sql,
                        .length = strlen(insert_sql),
                        .modes = 0U,
                        .allow_parameters = true,
                    },
                    &result
                ) != MYLITE_SQL_PARSE_OK;
    collect_parameter_indices(result.root, indices, 8U, &count);
    failures += parser_test_expect_true(
        result.parameter_count == 2U,
        "prepared INSERT reported parameter count"
    );
    failures += parser_test_expect_true(count == 2U, "prepared INSERT parameter count");
    failures += parser_test_expect_true(indices[0] == 0U, "prepared INSERT first parameter");
    failures += parser_test_expect_true(indices[1] == 1U, "prepared INSERT second parameter");
    mylite_sql_parse_result_deinit(&result);

    count = 0U;
    status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = update_sql,
            .length = strlen(update_sql),
            .modes = 0U,
            .allow_parameters = true,
        },
        &result
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        fprintf(
            stderr,
            "prepared UPDATE: %s at offset %zu '%.*s'\n",
            mylite_sql_parse_status_name(status),
            result.error_token.offset,
            (int)result.error_token.length,
            result.error_token.text == NULL ? "" : result.error_token.text
        );
        ++failures;
    }
    collect_parameter_indices(result.root, indices, 8U, &count);
    failures += parser_test_expect_true(
        result.parameter_count == 3U,
        "prepared UPDATE reported parameter count"
    );
    if (count != 3U) {
        fprintf(stderr, "prepared UPDATE parameter count: expected 3, got %zu\n", count);
        ++failures;
    }
    mylite_sql_parse_result_deinit(&result);

    for (size_t case_index = 0U; case_index < sizeof(extended_cases) / sizeof(extended_cases[0]);
         ++case_index) {
        count = 0U;
        status = mylite_sql_parse(
            (struct mylite_sql_parse_config){
                .input = extended_cases[case_index].sql,
                .length = strlen(extended_cases[case_index].sql),
                .modes = 0U,
                .allow_parameters = true,
            },
            &result
        );
        failures += parser_test_expect_true(
            status == MYLITE_SQL_PARSE_OK,
            "extended prepared parameter parse status"
        );
        collect_parameter_indices(result.root, indices, 8U, &count);
        failures += parser_test_expect_true(
            result.parameter_count == extended_cases[case_index].parameter_count,
            "extended prepared reported parameter count"
        );
        failures += parser_test_expect_true(
            count == extended_cases[case_index].parameter_count,
            "extended prepared AST parameter count"
        );
        for (size_t index = 0U; index < count; ++index) {
            failures += parser_test_expect_true(
                indices[index] == index,
                "extended prepared parameter lexical index"
            );
        }
        mylite_sql_parse_result_deinit(&result);
    }

    failures += mylite_sql_parse(
                    (struct mylite_sql_parse_config){
                        .input = "SELECT * FROM ?",
                        .length = strlen("SELECT * FROM ?"),
                        .modes = 0U,
                        .allow_parameters = true,
                    },
                    &result
                ) != MYLITE_SQL_PARSE_SYNTAX_ERROR;
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static void collect_parameter_indices(
    const struct mylite_sql_ast_node *node,
    size_t *indices,
    size_t capacity,
    size_t *count
) {
    if (node == NULL || indices == NULL || count == NULL) {
        return;
    }
    if (node->kind == MYLITE_SQL_AST_PARAMETER) {
        if (*count < capacity) {
            indices[*count] = mylite_sql_ast_node_parameter_index(node);
        }
        ++*count;
    }
    for (const struct mylite_sql_ast_node *child = node->first_child; child != NULL;
         child = child->next_sibling) {
        collect_parameter_indices(child, indices, capacity, count);
    }
}

static int test_use_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *first_use = NULL;
    const struct mylite_sql_ast_node *second_use = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures +=
        parser_test_parse_sql("USE mylite_seed; USE `select`;", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_child_count(result.root, 2U, "use root");

    first_use = parser_test_child_at(result.root, 0U);
    second_use = parser_test_child_at(result.root, 1U);
    failures += parser_test_expect_node(first_use, MYLITE_SQL_AST_USE_STATEMENT, "first use");
    failures += parser_test_expect_span_text(
        parser_test_child_at(first_use, 0U),
        "mylite_seed",
        "first schema"
    );
    failures += parser_test_expect_node(second_use, MYLITE_SQL_AST_USE_STATEMENT, "second use");
    failures += parser_test_expect_span_text(
        parser_test_child_at(second_use, 0U),
        "`select`",
        "second schema"
    );

    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SET NAMES utf8mb4;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SET_NAMES_STATEMENT,
        "set names statement"
    );
    failures += parser_test_expect_child_count(statement, 1U, "set names child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "utf8mb4",
        "set names charset"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SET NAMES names;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SET_NAMES_STATEMENT, "set names names");
    failures += parser_test_expect_child_count(statement, 1U, "set names names child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "names",
        "names charset identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SET NAMES 'utf8mb4' COLLATE `utf8mb4_0900_ai_ci`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SET_NAMES_STATEMENT,
        "set names collate statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "set names collate child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "set names string charset"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "`utf8mb4_0900_ai_ci`",
        "set names collation"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SET NAMES DEFAULT;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_SET_CHARACTER_SET_DEFAULT_TARGET,
        "set names default target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SET CHARACTER SET UTF8MB4;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT,
        "set character set statement"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "UTF8MB4",
        "set character set target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SET CHARSET DEFAULT;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT,
        "set charset statement"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_SET_CHARACTER_SET_DEFAULT_TARGET,
        "set charset default target"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_expression_list(void) {
    enum {
        expected_select_item_count = 10,
        percent_item_index = 1,
        mod_operator_item_index = 2,
        mod_function_item_index = 3,
        div_operator_item_index = 4,
        slash_operator_item_index = 5,
        string_item_index = 6,
        true_item_index = 7,
        false_item_index = 8,
        null_item_index = 9,
        div_precedence_item_index = 4,
        div_associativity_item_index = 5,
        comparison_item_count = 8,
        comparison_arithmetic_precedence_item_index = 0,
        comparison_associativity_item_index = 1,
        greater_associativity_item_index = 2,
        null_safe_equality_item_index = 3,
        angle_not_equal_item_index = 4,
        bang_not_equal_item_index = 5,
        less_equal_item_index = 6,
        greater_equal_item_index = 7,
        logical_item_count = 9,
        logical_and_comparison_item_index = 0,
        logical_and_arithmetic_item_index = 1,
        logical_or_item_index = 2,
        logical_xor_item_index = 3,
        logical_not_item_index = 4,
        logical_not_group_item_index = 5,
        logical_or_precedence_item_index = 6,
        logical_xor_precedence_item_index = 7,
        logical_result_comparison_item_index = 8,
        is_item_count = 10,
        is_true_item_index = 0,
        is_false_item_index = 1,
        is_unknown_item_index = 2,
        is_not_true_item_index = 3,
        is_null_item_index = 4,
        is_not_null_item_index = 5,
        is_not_precedence_item_index = 6,
        is_comparison_precedence_item_index = 7,
        is_logical_precedence_item_index = 8,
        is_parenthesized_comparison_item_index = 9,
        bitwise_item_count = 10,
        bitwise_or_item_index = 0,
        bitwise_and_item_index = 1,
        bitwise_xor_item_index = 2,
        bitwise_shift_item_index = 3,
        bitwise_shift_arithmetic_item_index = 4,
        bitwise_not_item_index = 5,
        bitwise_not_arithmetic_item_index = 6,
        bitwise_and_or_item_index = 7,
        bitwise_xor_and_item_index = 8,
        bitwise_logical_xor_item_index = 9,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_item = NULL;
    const struct mylite_sql_ast_node *add = NULL;
    const struct mylite_sql_ast_node *multiply = NULL;
    const struct mylite_sql_ast_node *percent = NULL;
    const struct mylite_sql_ast_node *mod_operator = NULL;
    const struct mylite_sql_ast_node *mod_function = NULL;
    const struct mylite_sql_ast_node *div_operator = NULL;
    const struct mylite_sql_ast_node *slash_operator = NULL;
    const struct mylite_sql_ast_node *comparison = NULL;
    const struct mylite_sql_ast_node *logical = NULL;
    const struct mylite_sql_ast_node *is_expression = NULL;
    const struct mylite_sql_ast_node *bitwise = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT 1 + 2 * 3, 5 % 2, 5 MOD 2, MOD(5,2), 5 DIV 2, 5 / 2, "
        "'text', TRUE, FALSE, NULL FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );

    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_item = parser_test_child_at(select_list, 0U);
    add = parser_test_child_at(first_item, 0U);
    multiply = parser_test_child_at(add, 1U);
    percent = parser_test_child_at(parser_test_child_at(select_list, percent_item_index), 0U);
    mod_operator =
        parser_test_child_at(parser_test_child_at(select_list, mod_operator_item_index), 0U);
    mod_function =
        parser_test_child_at(parser_test_child_at(select_list, mod_function_item_index), 0U);
    div_operator =
        parser_test_child_at(parser_test_child_at(select_list, div_operator_item_index), 0U);
    slash_operator =
        parser_test_child_at(parser_test_child_at(select_list, slash_operator_item_index), 0U);

    failures +=
        parser_test_expect_node(select, MYLITE_SQL_AST_SELECT_STATEMENT, "select statement");
    failures += parser_test_expect_node(select_list, MYLITE_SQL_AST_SELECT_LIST, "select list");
    failures +=
        parser_test_expect_child_count(select_list, expected_select_item_count, "select list");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "from dual"
    );

    failures += parser_test_expect_node(add, MYLITE_SQL_AST_BINARY_EXPRESSION, "add expression");
    failures += parser_test_expect_operator(add, MYLITE_SQL_AST_OPERATOR_ADD, "add expression");
    failures += parser_test_expect_span_text(parser_test_child_at(add, 0U), "1", "add left");
    failures +=
        parser_test_expect_node(multiply, MYLITE_SQL_AST_BINARY_EXPRESSION, "multiply expression");
    failures += parser_test_expect_operator(
        multiply,
        MYLITE_SQL_AST_OPERATOR_MULTIPLY,
        "multiply expression"
    );
    failures +=
        parser_test_expect_span_text(parser_test_child_at(multiply, 0U), "2", "multiply left");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(multiply, 1U), "3", "multiply right");
    failures +=
        parser_test_expect_node(percent, MYLITE_SQL_AST_BINARY_EXPRESSION, "percent expression");
    failures +=
        parser_test_expect_operator(percent, MYLITE_SQL_AST_OPERATOR_MODULO, "percent operator");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(percent, 0U), "5", "percent left");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(percent, 1U), "2", "percent right");
    failures +=
        parser_test_expect_node(mod_operator, MYLITE_SQL_AST_BINARY_EXPRESSION, "mod expression");
    failures +=
        parser_test_expect_operator(mod_operator, MYLITE_SQL_AST_OPERATOR_MODULO, "mod operator");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(mod_operator, 0U), "5", "mod left");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(mod_operator, 1U), "2", "mod right");
    failures += parser_test_expect_node(mod_function, MYLITE_SQL_AST_MOD_FUNCTION, "mod function");
    failures += parser_test_expect_child_count(mod_function, 2U, "mod function argument count");
    failures += parser_test_expect_span_text(mod_function, "MOD(5,2)", "mod function span");
    failures +=
        parser_test_expect_node(div_operator, MYLITE_SQL_AST_BINARY_EXPRESSION, "div expression");
    failures += parser_test_expect_operator(
        div_operator,
        MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE,
        "div operator"
    );
    failures +=
        parser_test_expect_span_text(parser_test_child_at(div_operator, 0U), "5", "div left");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(div_operator, 1U), "2", "div right");
    failures += parser_test_expect_node(
        slash_operator,
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "slash expression"
    );
    failures += parser_test_expect_operator(
        slash_operator,
        MYLITE_SQL_AST_OPERATOR_DIVIDE,
        "slash operator"
    );
    failures +=
        parser_test_expect_span_text(parser_test_child_at(slash_operator, 0U), "5", "slash left");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(slash_operator, 1U), "2", "slash right");

    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(select_list, string_item_index), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "string literal"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(select_list, true_item_index), 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "true literal"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(select_list, false_item_index), 0U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "false literal"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(select_list, null_item_index), 0U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "null literal"
    );

    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT 1+2=3, 1<2=1, 3>2>1, NULL<=>NULL, 1<>2, 1!=1, 2<=2, 3>=4;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    failures += parser_test_expect_child_count(
        select_list,
        comparison_item_count,
        "comparison select list"
    );
    comparison = parser_test_child_at(
        parser_test_child_at(select_list, comparison_arithmetic_precedence_item_index),
        0U
    );
    failures += parser_test_expect_operator(
        comparison,
        MYLITE_SQL_AST_OPERATOR_EQUAL,
        "comparison equality"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(comparison, 0U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "comparison arithmetic precedence"
    );
    comparison = parser_test_child_at(
        parser_test_child_at(select_list, comparison_associativity_item_index),
        0U
    );
    failures += parser_test_expect_operator(
        comparison,
        MYLITE_SQL_AST_OPERATOR_EQUAL,
        "comparison associativity"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(comparison, 0U),
        MYLITE_SQL_AST_OPERATOR_LESS,
        "comparison left associativity"
    );
    comparison = parser_test_child_at(
        parser_test_child_at(select_list, greater_associativity_item_index),
        0U
    );
    failures += parser_test_expect_operator(
        comparison,
        MYLITE_SQL_AST_OPERATOR_GREATER,
        "greater associativity"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(comparison, 0U),
        MYLITE_SQL_AST_OPERATOR_GREATER,
        "greater left associativity"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, null_safe_equality_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL,
        "null safe equality expression"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, angle_not_equal_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
        "angle not equal expression"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, bang_not_equal_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
        "bang not equal expression"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, less_equal_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_LESS_EQUAL,
        "less equal expression"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, greater_equal_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL,
        "greater equal expression"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT 1<2 AND 2<3, 1+2 AND 0, 1 OR 0, 1 XOR 0, NOT 1<2, "
        "NOT (1>2), 0 OR 0 AND 1, 1 XOR 1 AND 0, (1 AND 1)=1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    failures +=
        parser_test_expect_child_count(select_list, logical_item_count, "logical select list");
    logical = parser_test_child_at(
        parser_test_child_at(select_list, logical_and_comparison_item_index),
        0U
    );
    failures +=
        parser_test_expect_operator(logical, MYLITE_SQL_AST_OPERATOR_LOGICAL_AND, "logical and");
    failures += parser_test_expect_operator(
        parser_test_child_at(logical, 0U),
        MYLITE_SQL_AST_OPERATOR_LESS,
        "logical and comparison left"
    );
    logical = parser_test_child_at(
        parser_test_child_at(select_list, logical_and_arithmetic_item_index),
        0U
    );
    failures += parser_test_expect_operator(
        logical,
        MYLITE_SQL_AST_OPERATOR_LOGICAL_AND,
        "logical arithmetic and"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(logical, 0U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "logical and arithmetic precedence"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, logical_or_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_OR,
        "logical or"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, logical_xor_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR,
        "logical xor"
    );
    logical = parser_test_child_at(parser_test_child_at(select_list, logical_not_item_index), 0U);
    failures +=
        parser_test_expect_operator(logical, MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT, "logical not");
    failures += parser_test_expect_operator(
        parser_test_child_at(logical, 0U),
        MYLITE_SQL_AST_OPERATOR_LESS,
        "logical not comparison precedence"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, logical_not_group_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT,
        "logical not group"
    );
    logical = parser_test_child_at(
        parser_test_child_at(select_list, logical_or_precedence_item_index),
        0U
    );
    failures +=
        parser_test_expect_operator(logical, MYLITE_SQL_AST_OPERATOR_LOGICAL_OR, "or precedence");
    failures += parser_test_expect_operator(
        parser_test_child_at(logical, 1U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_AND,
        "and binds tighter than or"
    );
    logical = parser_test_child_at(
        parser_test_child_at(select_list, logical_xor_precedence_item_index),
        0U
    );
    failures +=
        parser_test_expect_operator(logical, MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR, "xor precedence");
    failures += parser_test_expect_operator(
        parser_test_child_at(logical, 1U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_AND,
        "and binds tighter than xor"
    );
    comparison = parser_test_child_at(
        parser_test_child_at(select_list, logical_result_comparison_item_index),
        0U
    );
    failures += parser_test_expect_operator(
        comparison,
        MYLITE_SQL_AST_OPERATOR_EQUAL,
        "logical result comparison"
    );
    logical = parser_test_child_at(comparison, 0U);
    failures += parser_test_expect_node(
        logical,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized logical comparison operand"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(logical, 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_AND,
        "logical result compared"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT !1 + 1, NOT 1 + 1;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    logical = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_operator(
        logical,
        MYLITE_SQL_AST_OPERATOR_ADD,
        "bang binds tighter than addition"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(logical, 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT,
        "bang left operand"
    );
    logical = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_operator(
        logical,
        MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT,
        "keyword not keeps lower precedence"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(logical, 0U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "keyword not operand"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT 1 IS TRUE, 0 IS FALSE, NULL IS UNKNOWN, 1 IS NOT TRUE, "
        "1 IS NULL, 1 IS NOT NULL, NOT 1 IS TRUE, 1 = 1 IS TRUE, "
        "1 IS TRUE AND 0, (1 IS TRUE)=1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    failures += parser_test_expect_child_count(select_list, is_item_count, "scalar is select list");
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, is_true_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_IS_TRUE,
        "scalar is true expression"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, is_false_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_IS_FALSE,
        "scalar is false expression"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, is_unknown_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN,
        "scalar is unknown expression"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, is_not_true_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE,
        "scalar is not true expression"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, is_null_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_IS_NULL,
        "scalar is null expression"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, is_not_null_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL,
        "scalar is not null expression"
    );
    logical =
        parser_test_child_at(parser_test_child_at(select_list, is_not_precedence_item_index), 0U);
    failures +=
        parser_test_expect_operator(logical, MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT, "not over is");
    failures += parser_test_expect_operator(
        parser_test_child_at(logical, 0U),
        MYLITE_SQL_AST_OPERATOR_IS_TRUE,
        "is binds tighter than not"
    );
    is_expression = parser_test_child_at(
        parser_test_child_at(select_list, is_comparison_precedence_item_index),
        0U
    );
    failures += parser_test_expect_operator(
        is_expression,
        MYLITE_SQL_AST_OPERATOR_IS_TRUE,
        "comparison then is"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(is_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_EQUAL,
        "comparison binds before is"
    );
    logical = parser_test_child_at(
        parser_test_child_at(select_list, is_logical_precedence_item_index),
        0U
    );
    failures +=
        parser_test_expect_operator(logical, MYLITE_SQL_AST_OPERATOR_LOGICAL_AND, "is then and");
    failures += parser_test_expect_operator(
        parser_test_child_at(logical, 0U),
        MYLITE_SQL_AST_OPERATOR_IS_TRUE,
        "is binds tighter than and"
    );
    comparison = parser_test_child_at(
        parser_test_child_at(select_list, is_parenthesized_comparison_item_index),
        0U
    );
    failures += parser_test_expect_operator(
        comparison,
        MYLITE_SQL_AST_OPERATOR_EQUAL,
        "parenthesized is compare"
    );
    is_expression = parser_test_child_at(comparison, 0U);
    failures += parser_test_expect_node(
        is_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized is comparison operand"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(is_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_IS_TRUE,
        "parenthesized is compared"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT 1+5%2*3, 5*3%4, 5%3%2, -(5%2), 1+5 DIV 2*3, 5 DIV 3 DIV 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    add = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    multiply = parser_test_child_at(add, 1U);
    percent = parser_test_child_at(multiply, 0U);
    failures +=
        parser_test_expect_operator(add, MYLITE_SQL_AST_OPERATOR_ADD, "modulo addition precedence");
    failures +=
        parser_test_expect_operator(multiply, MYLITE_SQL_AST_OPERATOR_MULTIPLY, "modulo multiply");
    failures += parser_test_expect_operator(
        percent,
        MYLITE_SQL_AST_OPERATOR_MODULO,
        "modulo before multiply"
    );
    percent = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_operator(
        percent,
        MYLITE_SQL_AST_OPERATOR_MODULO,
        "multiply before modulo"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(percent, 0U),
        MYLITE_SQL_AST_OPERATOR_MULTIPLY,
        "modulo left multiplication"
    );
    percent = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_operator(
        percent,
        MYLITE_SQL_AST_OPERATOR_MODULO,
        "modulo associativity"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(percent, 0U),
        MYLITE_SQL_AST_OPERATOR_MODULO,
        "modulo left associativity"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(parser_test_child_at(select_list, 3U), 0U),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_OPERATOR_MODULO,
        "unary modulo child"
    );
    div_operator =
        parser_test_child_at(parser_test_child_at(select_list, div_precedence_item_index), 0U);
    multiply = parser_test_child_at(div_operator, 1U);
    failures += parser_test_expect_operator(
        div_operator,
        MYLITE_SQL_AST_OPERATOR_ADD,
        "div addition precedence"
    );
    failures +=
        parser_test_expect_operator(multiply, MYLITE_SQL_AST_OPERATOR_MULTIPLY, "div multiply");
    failures += parser_test_expect_operator(
        parser_test_child_at(multiply, 0U),
        MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE,
        "div before multiply"
    );
    div_operator =
        parser_test_child_at(parser_test_child_at(select_list, div_associativity_item_index), 0U);
    failures += parser_test_expect_operator(
        div_operator,
        MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE,
        "div associativity"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(div_operator, 0U),
        MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE,
        "div left associativity"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT 1+5/2*3, 5/3/2, 5/2 DIV 1;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    add = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    multiply = parser_test_child_at(add, 1U);
    slash_operator = parser_test_child_at(multiply, 0U);
    failures +=
        parser_test_expect_operator(add, MYLITE_SQL_AST_OPERATOR_ADD, "slash addition precedence");
    failures +=
        parser_test_expect_operator(multiply, MYLITE_SQL_AST_OPERATOR_MULTIPLY, "slash multiply");
    failures += parser_test_expect_operator(
        slash_operator,
        MYLITE_SQL_AST_OPERATOR_DIVIDE,
        "slash before multiply"
    );
    slash_operator = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_operator(
        slash_operator,
        MYLITE_SQL_AST_OPERATOR_DIVIDE,
        "slash associativity"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(slash_operator, 0U),
        MYLITE_SQL_AST_OPERATOR_DIVIDE,
        "slash left associativity"
    );
    div_operator = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_operator(
        div_operator,
        MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE,
        "slash div same precedence"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(div_operator, 0U),
        MYLITE_SQL_AST_OPERATOR_DIVIDE,
        "slash before div"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT 1|2, 1&2, 1^2, 1<<2, 1<<2+1, ~1, ~(1+2), 1&2|4, "
        "1^3&2, 1 XOR 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    failures +=
        parser_test_expect_child_count(select_list, bitwise_item_count, "bitwise select list");
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, bitwise_or_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_OR,
        "bitwise or"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, bitwise_and_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_AND,
        "bitwise and"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, bitwise_xor_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_XOR,
        "bitwise xor"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, bitwise_shift_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_LEFT_SHIFT,
        "left shift"
    );
    bitwise = parser_test_child_at(
        parser_test_child_at(select_list, bitwise_shift_arithmetic_item_index),
        0U
    );
    failures += parser_test_expect_operator(
        bitwise,
        MYLITE_SQL_AST_OPERATOR_LEFT_SHIFT,
        "shift arithmetic"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(bitwise, 1U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "addition binds tighter than shift"
    );
    bitwise = parser_test_child_at(parser_test_child_at(select_list, bitwise_not_item_index), 0U);
    failures +=
        parser_test_expect_operator(bitwise, MYLITE_SQL_AST_OPERATOR_BITWISE_NOT, "bitwise not");
    bitwise = parser_test_child_at(
        parser_test_child_at(select_list, bitwise_not_arithmetic_item_index),
        0U
    );
    failures += parser_test_expect_operator(
        bitwise,
        MYLITE_SQL_AST_OPERATOR_BITWISE_NOT,
        "bitwise not group"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(bitwise, 0U),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "bitwise not parenthesized child"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(bitwise, 0U), 0U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "bitwise not sees parenthesized arithmetic"
    );
    bitwise =
        parser_test_child_at(parser_test_child_at(select_list, bitwise_and_or_item_index), 0U);
    failures +=
        parser_test_expect_operator(bitwise, MYLITE_SQL_AST_OPERATOR_BITWISE_OR, "and before or");
    failures += parser_test_expect_operator(
        parser_test_child_at(bitwise, 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_AND,
        "bitwise and binds tighter than or"
    );
    bitwise =
        parser_test_child_at(parser_test_child_at(select_list, bitwise_xor_and_item_index), 0U);
    failures +=
        parser_test_expect_operator(bitwise, MYLITE_SQL_AST_OPERATOR_BITWISE_AND, "xor before and");
    failures += parser_test_expect_operator(
        parser_test_child_at(bitwise, 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_XOR,
        "bitwise xor binds tighter than and"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(select_list, bitwise_logical_xor_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR,
        "keyword xor remains logical xor"
    );
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

    failures += parser_test_parse_sql("SELECT -(1 + 2);", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    unary = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(unary, 0U);
    add = parser_test_child_at(parenthesized, 0U);

    failures +=
        parser_test_expect_node(unary, MYLITE_SQL_AST_UNARY_EXPRESSION, "negative expression");
    failures +=
        parser_test_expect_operator(unary, MYLITE_SQL_AST_OPERATOR_NEGATIVE, "negative expression");
    failures += parser_test_expect_span_text(unary, "-(1 + 2)", "negative expression");
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized expression"
    );
    failures += parser_test_expect_span_text(parenthesized, "(1 + 2)", "parenthesized expression");
    failures += parser_test_expect_node(add, MYLITE_SQL_AST_BINARY_EXPRESSION, "parenthesized add");
    failures += parser_test_expect_operator(add, MYLITE_SQL_AST_OPERATOR_ADD, "parenthesized add");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(add, 0U), "1", "parenthesized add left");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(add, 1U), "2", "parenthesized add right");

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_literal_categories(void) {
    enum { expected_literal_item_count = 5 };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *literal = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT 0xabc, b'10', .25, 1e+3, N'a';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    failures += parser_test_expect_child_count(
        select_list,
        expected_literal_item_count,
        "literal select list"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_LITERAL_HEX,
        "hex literal"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_LITERAL_BIT,
        "bit literal"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(select_list, 2U), 0U),
        MYLITE_SQL_AST_LITERAL_DECIMAL,
        "decimal literal"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(select_list, 3U), 0U),
        MYLITE_SQL_AST_LITERAL_FLOAT,
        "float literal"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(select_list, 4U), 0U),
        MYLITE_SQL_AST_LITERAL_NATIONAL_STRING,
        "national literal"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT 'ab' 'cd' 'ef';", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    literal = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_literal(literal, MYLITE_SQL_AST_LITERAL_STRING, "concatenated literal");
    failures +=
        parser_test_expect_span_text(literal, "'ab' 'cd' 'ef'", "concatenated literal span");
    failures += parser_test_expect_child_count(literal, 3U, "concatenated literal segments");
    failures += parser_test_expect_span_text(
        parser_test_child_at(literal, 0U),
        "'ab'",
        "first literal segment"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(literal, 1U),
        "'cd'",
        "second literal segment"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(literal, 2U),
        "'ef'",
        "third literal segment"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT 'a' N'b';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT * FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    failures += parser_test_expect_child_count(select_list, 1U, "wildcard select list");
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "wildcard item"
    );

    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT *;", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_child_count(
        parser_test_child_at(result.root, 0U),
        1U,
        "bare wildcard select"
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    failures += parser_test_expect_child_count(select_list, 1U, "bare wildcard select list");
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "bare wildcard item"
    );

    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT *, COUNT(*) AS c FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    failures += parser_test_expect_child_count(select_list, 2U, "mixed wildcard select list");
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "mixed wildcard item"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "mixed aggregate item"
    );

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_qualified_identifier_keyword_part(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *qualified = NULL;
    const struct mylite_sql_ast_node *wildcard = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SELECT mydb.select;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    qualified = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);

    failures += parser_test_expect_node(
        qualified,
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified identifier"
    );
    failures +=
        parser_test_expect_span_text(parser_test_child_at(qualified, 0U), "mydb", "qualified left");
    failures += parser_test_expect_span_text(
        parser_test_child_at(qualified, 1U),
        "select",
        "qualified right"
    );

    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT mydb. /* comment */ select;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    qualified = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);

    failures += parser_test_expect_node(
        qualified,
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "commented qualified identifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(qualified, 0U),
        "mydb",
        "commented qualified left"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(qualified, 1U),
        "select",
        "commented qualified right"
    );

    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE names (names INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "names",
        "names table identifier"
    );
    columns = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "names",
        "names column identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE ifnull (ifnull INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "ifnull",
        "ifnull table identifier"
    );
    columns = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "ifnull",
        "ifnull column identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ifnull FROM ifnull;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        "ifnull",
        "ifnull selected identifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(statement, 1U), 0U),
        "ifnull",
        "ifnull table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE coalesce (coalesce INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "coalesce",
        "coalesce table identifier"
    );
    columns = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "coalesce",
        "coalesce column identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT coalesce FROM coalesce;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        "coalesce",
        "coalesce selected identifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(statement, 1U), 0U),
        "coalesce",
        "coalesce table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE duplicate (duplicate INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "duplicate",
        "duplicate table"
    );
    columns = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "duplicate",
        "duplicate column"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT a.n FROM numbers AS a WHERE a.n IS NOT NULL ORDER BY a.id DESC LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    qualified = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        qualified,
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified selected column"
    );
    failures += parser_test_expect_span_text(qualified, "a.n", "qualified selected span");
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_IS_NULL_PREDICATE,
        "qualified where predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 3U), 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified order key"
    );

    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT a.* FROM numbers AS a;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    wildcard = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(wildcard, MYLITE_SQL_AST_QUALIFIED_WILDCARD, "qualified wildcard");
    failures += parser_test_expect_span_text(
        parser_test_child_at(wildcard, 0U),
        "a",
        "qualified wildcard source"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(wildcard, 1U),
        MYLITE_SQL_AST_WILDCARD,
        "wildcard star"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT app.numbers.* FROM app.numbers;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    wildcard = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        wildcard,
        MYLITE_SQL_AST_QUALIFIED_WILDCARD,
        "schema-qualified wildcard"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(wildcard, 0U),
        "app.numbers",
        "schema wildcard source"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT a.*, b.* FROM lefts a JOIN rights b ON a.id = b.id;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    failures += parser_test_expect_child_count(select_list, 2U, "joined qualified wildcard items");
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_QUALIFIED_WILDCARD,
        "joined left wildcard"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_QUALIFIED_WILDCARD,
        "joined right wildcard"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT a.* AS all_columns FROM numbers AS a;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_comments_are_skipped(void) {
    static const char executable_expression_sql[] = "SELECT /*!80000 1 */ + 2;";
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *select_item = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *left = NULL;
    const char *left_text = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "/* regular */ SELECT -- line\n1 /*+ hint */;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_child_count(result.root, 1U, "comment root");
    failures += parser_test_expect_node(
        parser_test_child_at(result.root, 0U),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "comment select"
    );

    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT /*+ MAX_EXECUTION_TIME(1000) */ 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT 1 /*!80000 + 1 */;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(executable_expression_sql, MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    select_item = parser_test_child_at(select_list, 0U);
    expression = parser_test_child_at(select_item, 0U);
    left = parser_test_child_at(expression, 0U);
    left_text = strstr(executable_expression_sql, "1 */");
    failures += parser_test_expect_span_text(left, "1", "executable comment absolute span text");
    if (left == NULL || left_text == NULL ||
        left->span.offset != (size_t)(left_text - executable_expression_sql) ||
        left->span.source_length != sizeof(executable_expression_sql) - 1U) {
        fprintf(stderr, "executable comment token span is not root-relative\n");
        ++failures;
    }
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT /*!80000 1 */ AS executable_alias;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT 1 WHERE /*!80000 1 = 1 */;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT * FROM t WHERE (a,b) = /*!80000 (1,2) */;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT /*!80000 'unterminated */;",
        MYLITE_SQL_PARSE_LEXER_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT 1 /*!80409 + 1 */;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT 1 /*!80410 + 1 */;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT 1 /*!99999 /* */ */;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT 2 /*!12345 /* */ */;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT /*! 9 */;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT /*!99999 9 */ AS skipped_payload_value;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT 1 /* outer /* inner */ */ AS ordinary_nested;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "WITH cte AS (SELECT 0 /*! ) */ SELECT * FROM cte a, cte b;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "WITH cte AS /*! ( */ SELECT 0) SELECT * FROM cte a, cte b;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "ALTER TABLE child ADD CONSTRAINT c2 FOREIGN KEY (fk) REFERENCES parent /*! (id) */ "
        "/*!40008 ON DELETE SET NULL */;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "INSERT /*+ SET_VAR(sort_buffer_size=262144) */ INTO t VALUES (1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "REPLACE /*+ SET_VAR(sort_buffer_size=262144) */ INTO t VALUES (1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "UPDATE /*+ SET_VAR(sort_buffer_size=262144) */ t SET id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "DELETE /*+ SET_VAR(sort_buffer_size=262144) */ FROM t WHERE id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_source_span_bounds(void) {
    static const char source[] = "SELECT 1";
    struct mylite_sql_ast ast;
    int failures = 0;

    failures += parser_test_expect_true(
        mylite_sql_source_span_is_valid((struct mylite_sql_source_span){
            .text = source + 7U,
            .length = 1U,
            .offset = 7U,
            .source_length = sizeof(source) - 1U,
        }),
        "bounded source span"
    );
    failures += parser_test_expect_true(
        !mylite_sql_source_span_is_valid((struct mylite_sql_source_span){
            .text = source,
            .length = 2U,
            .offset = SIZE_MAX,
            .source_length = sizeof(source) - 1U,
        }),
        "overflowing source span offset"
    );

    mylite_sql_ast_init(&ast);
    (void)mylite_sql_ast_new_node(
        &ast,
        MYLITE_SQL_AST_LITERAL,
        (struct mylite_sql_source_span){
            .text = source + 7U,
            .length = 2U,
            .offset = 7U,
            .source_length = sizeof(source) - 1U,
        }
    );
    failures += parser_test_expect_true(
        !mylite_sql_ast_spans_are_within_source(&ast, source, sizeof(source) - 1U),
        "AST rejects out-of-bounds source span"
    );
    mylite_sql_ast_deinit(&ast);

    return failures;
}
