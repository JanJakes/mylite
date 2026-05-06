#include "mylite_select_subquery.h"

#include "mylite_span.h"

static bool row_comparison_operator_is_supported(enum mylite_sql_ast_operator operator_kind);

bool mylite_select_subquery_row_expression_is_supported(
    const struct mylite_sql_ast_node *expression)
{
    if (mylite_select_subquery_binary_expression_is_row_in(expression)) {
        return true;
    }
    if (mylite_select_subquery_binary_expression_is_row_scalar(expression)) {
        return true;
    }
    return mylite_select_subquery_quantified_comparison_is_row_alias(expression);
}

bool mylite_select_subquery_row_expression_is_membership(
    const struct mylite_sql_ast_node *expression)
{
    if (mylite_select_subquery_binary_expression_is_row_in(expression)) {
        return true;
    }
    return mylite_select_subquery_quantified_comparison_is_row_alias(expression);
}

bool mylite_select_subquery_row_expression_is_positive_membership(
    const struct mylite_sql_ast_node *expression)
{
    if (mylite_select_subquery_binary_expression_is_row_in(expression)) {
        return expression->operator_kind == MYLITE_SQL_AST_OPERATOR_IN;
    }
    if (mylite_select_subquery_quantified_comparison_is_row_alias(expression)) {
        if (expression->operator_kind != MYLITE_SQL_AST_OPERATOR_EQUAL) {
            return false;
        }
        if (expression->subquery_quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ANY) {
            return true;
        }
        return expression->subquery_quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_SOME;
    }
    return false;
}

bool mylite_select_subquery_binary_expression_is_row(const struct mylite_sql_ast_node *expression)
{
    if (mylite_select_subquery_binary_expression_is_row_in(expression)) {
        return true;
    }
    return mylite_select_subquery_binary_expression_is_row_scalar(expression);
}

bool mylite_select_subquery_binary_expression_is_row_in(
    const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *left =
        mylite_sql_ast_unwrap_parenthesized_expression(mylite_ast_child_at(expression, 0U));
    const struct mylite_sql_ast_node *right =
        mylite_sql_ast_unwrap_parenthesized_expression(mylite_ast_child_at(expression, 1U));

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        left == NULL || left->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR || right == NULL ||
        right->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return false;
    }
    if (expression->operator_kind == MYLITE_SQL_AST_OPERATOR_IN) {
        return true;
    }
    return expression->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_IN;
}

bool mylite_select_subquery_binary_expression_is_row_scalar(
    const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *left =
        mylite_sql_ast_unwrap_parenthesized_expression(mylite_ast_child_at(expression, 0U));
    const struct mylite_sql_ast_node *right =
        mylite_sql_ast_unwrap_parenthesized_expression(mylite_ast_child_at(expression, 1U));

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        left == NULL || left->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR || right == NULL ||
        right->kind != MYLITE_SQL_AST_SUBQUERY_EXPRESSION) {
        return false;
    }
    return row_comparison_operator_is_supported(expression->operator_kind);
}

const struct mylite_sql_ast_node *
mylite_select_subquery_row_select_statement(const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *right =
        mylite_sql_ast_unwrap_parenthesized_expression(mylite_ast_child_at(expression, 1U));

    if (mylite_select_subquery_quantified_comparison_is_row_alias(expression)) {
        return mylite_ast_child_at(expression, 1U);
    }
    if (mylite_select_subquery_binary_expression_is_row_in(expression)) {
        return right;
    }
    if (mylite_select_subquery_binary_expression_is_row_scalar(expression)) {
        return mylite_ast_child_at(right, 0U);
    }
    return NULL;
}

bool mylite_select_subquery_quantified_comparison_has_row_left(
    const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *left =
        mylite_sql_ast_unwrap_parenthesized_expression(mylite_ast_child_at(expression, 0U));

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_QUANTIFIED_COMPARISON ||
        left == NULL) {
        return false;
    }
    return left->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR;
}

bool mylite_select_subquery_quantified_comparison_is_row_alias(
    const struct mylite_sql_ast_node *expression)
{
    if (!mylite_select_subquery_quantified_comparison_has_row_left(expression)) {
        return false;
    }
    if (expression->operator_kind == MYLITE_SQL_AST_OPERATOR_EQUAL) {
        if (expression->subquery_quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ANY) {
            return true;
        }
        return expression->subquery_quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_SOME;
    }
    if (expression->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_EQUAL) {
        return expression->subquery_quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ALL;
    }
    return false;
}

bool mylite_select_subquery_quantified_operator_is_supported(
    enum mylite_sql_ast_operator operator_kind)
{
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return true;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BINARY_CAST:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_NOT_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        break;
    }
    return false;
}

size_t mylite_select_subquery_row_constructor_width(const struct mylite_sql_ast_node *row)
{
    size_t width = 0U;

    if (row == NULL || row->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        return 0U;
    }
    for (const struct mylite_sql_ast_node *child = row->first_child; child != NULL;
         child = child->next_sibling) {
        ++width;
    }
    return width;
}

bool mylite_select_subquery_binary_expression_is_in(const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *right = mylite_ast_child_at(expression, 1U);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return false;
    }
    if (expression->operator_kind != MYLITE_SQL_AST_OPERATOR_IN &&
        expression->operator_kind != MYLITE_SQL_AST_OPERATOR_NOT_IN) {
        return false;
    }
    if (right == NULL) {
        return false;
    }
    switch (right->kind) {
    case MYLITE_SQL_AST_SELECT_STATEMENT:
        return true;
    default:
        return false;
    }
}

static bool row_comparison_operator_is_supported(enum mylite_sql_ast_operator operator_kind)
{
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return true;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BINARY_CAST:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_NOT_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        break;
    }
    return false;
}
