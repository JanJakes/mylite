#include "mylite_expression.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// NOLINTBEGIN(misc-no-recursion, readability-implicit-bool-conversion)

enum {
    MYLITE_WARNING_INCORRECT_ESCAPE_ARGUMENTS = 1210,
    MYLITE_WARNING_TRUNCATED_WRONG_VALUE = 1292,
    MYLITE_WARNING_DIVISION_BY_ZERO = 1365,
    MYLITE_EXPRESSION_TEXT_BUFFER_SIZE = 64,
    MYLITE_EXPRESSION_DECIMAL_BASE = 10,
    MYLITE_EXPRESSION_UINT64_DIGITS = 19,
    MYLITE_EXPRESSION_BITS_PER_UINT64 = 64,
    MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE = 256,
    MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW = 160,
};

struct numeric_value {
    double real_value;
    int64_t int64_value;
    uint64_t uint64_value;
    bool is_integer;
    bool is_unsigned;
};

struct between_truth {
    int low;
    int high;
};

static int eval_node(const struct mylite_sql_ast_node *node,
                     const struct mylite_expression_eval_context *context,
                     struct mylite_expression_warnings *warnings,
                     struct mylite_expression_value *out_value);
static int eval_unary(const struct mylite_sql_ast_node *node,
                      const struct mylite_expression_eval_context *context,
                      struct mylite_expression_warnings *warnings,
                      struct mylite_expression_value *out_value);
static int eval_binary(const struct mylite_sql_ast_node *node,
                       const struct mylite_expression_eval_context *context,
                       struct mylite_expression_warnings *warnings,
                       struct mylite_expression_value *out_value);
static int eval_logical_and(const struct mylite_sql_ast_node *node,
                            const struct mylite_expression_eval_context *context,
                            struct mylite_expression_warnings *warnings,
                            struct mylite_expression_value *out_value);
static int eval_logical_or(const struct mylite_sql_ast_node *node,
                           const struct mylite_expression_eval_context *context,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value);
static int validate_boolean_shortcut_operand(const struct mylite_sql_ast_node *node,
                                             const struct mylite_expression_eval_context *context,
                                             struct mylite_expression_warnings *warnings);
static int
validate_like_escape_before_shortcut(const struct mylite_sql_ast_node *node,
                                     const struct mylite_expression_eval_context *context,
                                     struct mylite_expression_warnings *warnings);
static bool expression_is_constant_boolean(const struct mylite_sql_ast_node *node,
                                           bool expected_value);
static int eval_ternary(const struct mylite_sql_ast_node *node,
                        const struct mylite_expression_eval_context *context,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value);
static int eval_literal(const struct mylite_sql_ast_node *node,
                        struct mylite_expression_value *out_value);
static int eval_is_expression(enum mylite_sql_ast_operator operator_kind,
                              const struct mylite_sql_ast_node *operand,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value);
static int eval_between(enum mylite_sql_ast_operator operator_kind,
                        const struct mylite_sql_ast_node *node,
                        const struct mylite_expression_eval_context *context,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value);
static int eval_between_bound_truth(const struct mylite_expression_value *value,
                                    const struct mylite_expression_value *bound, bool lower_bound,
                                    struct mylite_expression_warnings *warnings, int *out_truth);
static void set_between_result(enum mylite_sql_ast_operator operator_kind,
                               struct between_truth truth,
                               struct mylite_expression_value *out_value);
static int eval_like(enum mylite_sql_ast_operator operator_kind,
                     const struct mylite_sql_ast_node *node,
                     const struct mylite_expression_eval_context *context,
                     struct mylite_expression_warnings *warnings,
                     struct mylite_expression_value *out_value);
static int eval_in(enum mylite_sql_ast_operator operator_kind,
                   const struct mylite_sql_ast_node *node,
                   const struct mylite_expression_eval_context *context,
                   struct mylite_expression_warnings *warnings,
                   struct mylite_expression_value *out_value);
static int eval_numeric_unary(enum mylite_sql_ast_operator operator_kind,
                              const struct mylite_expression_value *operand,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value);
static int eval_arithmetic(enum mylite_sql_ast_operator operator_kind,
                           const struct mylite_expression_value *left,
                           const struct mylite_expression_value *right,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value);
static int eval_bitwise(enum mylite_sql_ast_operator operator_kind,
                        const struct mylite_expression_value *left,
                        const struct mylite_expression_value *right,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value);
static int eval_comparison(enum mylite_sql_ast_operator operator_kind,
                           const struct mylite_expression_value *left,
                           const struct mylite_expression_value *right,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value);
static int eval_logical(enum mylite_sql_ast_operator operator_kind,
                        const struct mylite_expression_value *left,
                        const struct mylite_expression_value *right,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value);
static int truth_value(const struct mylite_expression_value *value,
                       struct mylite_expression_warnings *warnings, int *out_truth);
static int compare_values(const struct mylite_expression_value *left,
                          const struct mylite_expression_value *right,
                          struct mylite_expression_warnings *warnings, int *out_compare);
static int value_to_numeric(const struct mylite_expression_value *value,
                            struct mylite_expression_warnings *warnings,
                            struct numeric_value *out_numeric);
static int value_to_string(const struct mylite_expression_value *value, char **out_text);
static int append_warning(struct mylite_expression_warnings *warnings, unsigned int code,
                          const char *message);
static int append_truncation_warning(struct mylite_expression_warnings *warnings, const char *text);
static char *copy_span_text(const char *text, size_t length);
static char *decode_string_literal(const struct mylite_sql_ast_node *node);
static bool decode_string_escape(char escaped, char *out_character);
static const struct mylite_sql_ast_node *child_at(const struct mylite_sql_ast_node *node,
                                                  size_t index);
static bool is_null(const struct mylite_expression_value *value);
static bool is_numeric_kind(enum mylite_expression_value_kind kind);
static bool like_match(const char *value, const char *pattern, char escape);
static bool like_match_here(const char *value, const char *pattern, char escape);
static int ascii_case_fold(int character);

void mylite_expression_value_deinit(struct mylite_expression_value *value)
{
    if (value == NULL) {
        return;
    }

    free(value->text_value);
    *value = (struct mylite_expression_value){0};
}

void mylite_expression_warnings_deinit(struct mylite_expression_warnings *warnings)
{
    if (warnings == NULL) {
        return;
    }

    for (size_t index = 0U; index < warnings->count; ++index) {
        free(warnings->items[index].message);
    }
    free(warnings->items);
    *warnings = (struct mylite_expression_warnings){0};
}

int mylite_expression_eval(const struct mylite_sql_ast_node *expression,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value)
{
    return mylite_expression_eval_with_context(expression, NULL, warnings, out_value);
}

int mylite_expression_eval_with_context(const struct mylite_sql_ast_node *expression,
                                        const struct mylite_expression_eval_context *context,
                                        struct mylite_expression_warnings *warnings,
                                        struct mylite_expression_value *out_value)
{
    if (out_value == NULL) {
        return -1;
    }

    *out_value = (struct mylite_expression_value){0};
    return eval_node(expression, context, warnings, out_value);
}

int mylite_expression_value_copy(const struct mylite_expression_value *value,
                                 struct mylite_expression_value *out_value)
{
    *out_value = *value;
    out_value->text_value = NULL;
    if (value->text_value != NULL) {
        out_value->text_value = copy_span_text(value->text_value, strlen(value->text_value));
        if (out_value->text_value == NULL) {
            return -1;
        }
    }
    return 0;
}

char *mylite_expression_value_to_text(const struct mylite_expression_value *value)
{
    char buffer[MYLITE_EXPRESSION_TEXT_BUFFER_SIZE];

    if (value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return NULL;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return copy_span_text(value->text_value,
                              value->text_value == NULL ? 0U : strlen(value->text_value));
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        int length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value->int64_value);
        return length < 0 ? NULL : copy_span_text(buffer, (size_t)length);
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        int length =
            snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value->uint64_value);
        return length < 0 ? NULL : copy_span_text(buffer, (size_t)length);
    }

    int length = snprintf(buffer, sizeof(buffer), "%.4f", value->real_value);
    return length < 0 ? NULL : copy_span_text(buffer, (size_t)length);
}

int64_t mylite_expression_value_to_int64(const struct mylite_expression_value *value)
{
    if (value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        return value->int64_value;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        return value->uint64_value > (uint64_t)INT64_MAX ? INT64_MAX : (int64_t)value->uint64_value;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        return (int64_t)value->real_value;
    }
    return value->text_value == NULL
               ? 0
               : strtoll(value->text_value, NULL, MYLITE_EXPRESSION_DECIMAL_BASE);
}

int mylite_expression_value_truth(const struct mylite_expression_value *value,
                                  struct mylite_expression_warnings *warnings, int *out_truth)
{
    if (out_truth == NULL) {
        return -1;
    }
    return truth_value(value, warnings, out_truth);
}

bool mylite_expression_is_supported_no_table(const struct mylite_sql_ast_node *expression)
{
    if (expression == NULL) {
        return false;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
        for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
             child = child->next_sibling) {
            if (!mylite_expression_is_supported_no_table(child)) {
                return false;
            }
        }
        return true;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
    default:
        return false;
    }
}

static int eval_node(const struct mylite_sql_ast_node *node,
                     const struct mylite_expression_eval_context *context,
                     struct mylite_expression_warnings *warnings,
                     struct mylite_expression_value *out_value)
{
    if (node == NULL) {
        return -1;
    }
    while (node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        node = child_at(node, 0U);
        if (node == NULL) {
            return -1;
        }
    }
    if (context != NULL && context->eval_constant != NULL &&
        mylite_expression_is_supported_no_table(node)) {
        return context->eval_constant(context->user_data, node, warnings, out_value);
    }

    switch (node->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return eval_literal(node, out_value);
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        if (context != NULL && context->resolve_identifier != NULL) {
            return context->resolve_identifier(context->user_data, node, out_value);
        }
        return -1;
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return eval_unary(node, context, warnings, out_value);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return eval_binary(node, context, warnings, out_value);
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
        return eval_ternary(node, context, warnings, out_value);
    default:
        return -1;
    }
}

static int eval_unary(const struct mylite_sql_ast_node *node,
                      const struct mylite_expression_eval_context *context,
                      struct mylite_expression_warnings *warnings,
                      struct mylite_expression_value *out_value)
{
    struct mylite_expression_value operand = {0};
    int status = 0;

    switch (node->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
        return eval_is_expression(node->operator_kind, child_at(node, 0U), context, warnings,
                                  out_value);
    default:
        break;
    }

    status = eval_node(child_at(node, 0U), context, warnings, &operand);
    if (status != 0) {
        return status;
    }
    status = eval_numeric_unary(node->operator_kind, &operand, warnings, out_value);
    mylite_expression_value_deinit(&operand);
    return status;
}

static int eval_binary(const struct mylite_sql_ast_node *node,
                       const struct mylite_expression_eval_context *context,
                       struct mylite_expression_warnings *warnings,
                       struct mylite_expression_value *out_value)
{
    struct mylite_expression_value left = {0};
    struct mylite_expression_value right = {0};
    int status = 0;

    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_IN ||
        node->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_IN) {
        return eval_in(node->operator_kind, node, context, warnings, out_value);
    }
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE ||
        node->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_LIKE) {
        return eval_like(node->operator_kind, node, context, warnings, out_value);
    }
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_AND) {
        return eval_logical_and(node, context, warnings, out_value);
    }
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_OR) {
        return eval_logical_or(node, context, warnings, out_value);
    }

    status = eval_node(child_at(node, 0U), context, warnings, &left);
    if (status == 0) {
        status = eval_node(child_at(node, 1U), context, warnings, &right);
    }
    if (status != 0) {
        goto cleanup;
    }

    switch (node->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        status = eval_arithmetic(node->operator_kind, &left, &right, warnings, out_value);
        break;
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
        status = eval_bitwise(node->operator_kind, &left, &right, warnings, out_value);
        break;
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        status = eval_comparison(node->operator_kind, &left, &right, warnings, out_value);
        break;
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
        status = eval_logical(node->operator_kind, &left, &right, warnings, out_value);
        break;
    default:
        status = -1;
        break;
    }

cleanup:
    mylite_expression_value_deinit(&left);
    mylite_expression_value_deinit(&right);
    return status;
}

static int eval_logical_and(const struct mylite_sql_ast_node *node,
                            const struct mylite_expression_eval_context *context,
                            struct mylite_expression_warnings *warnings,
                            struct mylite_expression_value *out_value)
{
    struct mylite_expression_value left = {0};
    struct mylite_expression_value right = {0};
    int left_truth = -1;
    int right_truth = -1;
    int status = 0;

    if (context != NULL && !mylite_expression_is_supported_no_table(child_at(node, 0U)) &&
        expression_is_constant_boolean(child_at(node, 1U), false)) {
        status = validate_boolean_shortcut_operand(child_at(node, 0U), context, warnings);
        if (status != 0) {
            return status;
        }
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 0};
        return 0;
    }

    status = eval_node(child_at(node, 0U), context, warnings, &left);
    if (status == 0) {
        status = truth_value(&left, warnings, &left_truth);
    }
    if (status != 0 || left_truth == 0) {
        mylite_expression_value_deinit(&left);
        if (status == 0) {
            status = validate_boolean_shortcut_operand(child_at(node, 1U), context, warnings);
        }
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 0};
        }
        return status;
    }

    status = eval_node(child_at(node, 1U), context, warnings, &right);
    if (status == 0) {
        status = truth_value(&right, warnings, &right_truth);
    }
    if (status == 0) {
        if (right_truth == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 0};
        } else if (left_truth < 0 || right_truth < 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        } else {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 1};
        }
    }

    mylite_expression_value_deinit(&left);
    mylite_expression_value_deinit(&right);
    return status;
}

static int eval_logical_or(const struct mylite_sql_ast_node *node,
                           const struct mylite_expression_eval_context *context,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value)
{
    struct mylite_expression_value left = {0};
    struct mylite_expression_value right = {0};
    int left_truth = -1;
    int right_truth = -1;
    int status = 0;

    if (context != NULL && !mylite_expression_is_supported_no_table(child_at(node, 0U)) &&
        expression_is_constant_boolean(child_at(node, 1U), true)) {
        status = validate_boolean_shortcut_operand(child_at(node, 0U), context, warnings);
        if (status != 0) {
            return status;
        }
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 1};
        return 0;
    }

    status = eval_node(child_at(node, 0U), context, warnings, &left);
    if (status == 0) {
        status = truth_value(&left, warnings, &left_truth);
    }
    if (status != 0 || left_truth == 1) {
        mylite_expression_value_deinit(&left);
        if (status == 0) {
            status = validate_boolean_shortcut_operand(child_at(node, 1U), context, warnings);
        }
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 1};
        }
        return status;
    }

    status = eval_node(child_at(node, 1U), context, warnings, &right);
    if (status == 0) {
        status = truth_value(&right, warnings, &right_truth);
    }
    if (status == 0) {
        if (right_truth == 1) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 1};
        } else if (left_truth < 0 || right_truth < 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        } else {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 0};
        }
    }

    mylite_expression_value_deinit(&left);
    mylite_expression_value_deinit(&right);
    return status;
}

static int validate_boolean_shortcut_operand(const struct mylite_sql_ast_node *node,
                                             const struct mylite_expression_eval_context *context,
                                             struct mylite_expression_warnings *warnings)
{
    int status = 0;

    if (node == NULL) {
        return 0;
    }
    while (node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        node = child_at(node, 0U);
        if (node == NULL) {
            return 0;
        }
    }
    if ((node->kind == MYLITE_SQL_AST_BINARY_EXPRESSION ||
         node->kind == MYLITE_SQL_AST_TERNARY_EXPRESSION) &&
        (node->operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE ||
         node->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_LIKE)) {
        status = validate_like_escape_before_shortcut(node, context, warnings);
        if (status != 0) {
            return status;
        }
    }

    for (const struct mylite_sql_ast_node *child = node->first_child; child != NULL;
         child = child->next_sibling) {
        status = validate_boolean_shortcut_operand(child, context, warnings);
        if (status != 0) {
            return status;
        }
    }
    return 0;
}

static int
validate_like_escape_before_shortcut(const struct mylite_sql_ast_node *node,
                                     const struct mylite_expression_eval_context *context,
                                     struct mylite_expression_warnings *warnings)
{
    const struct mylite_sql_ast_node *escape = child_at(node, 2U);
    struct mylite_expression_value escape_value = {0};
    char *escape_text = NULL;
    int status = 0;

    if (escape == NULL) {
        return 0;
    }
    status = eval_node(escape, context, warnings, &escape_value);
    if (status != 0 || is_null(&escape_value)) {
        mylite_expression_value_deinit(&escape_value);
        return status;
    }

    status = value_to_string(&escape_value, &escape_text);
    if (status == 0 && strlen(escape_text) != 1U) {
        status = append_warning(warnings, MYLITE_WARNING_INCORRECT_ESCAPE_ARGUMENTS,
                                "Incorrect arguments to ESCAPE");
        if (status == 0) {
            status = -1;
        }
    }

    free(escape_text);
    mylite_expression_value_deinit(&escape_value);
    return status;
}

static bool expression_is_constant_boolean(const struct mylite_sql_ast_node *node,
                                           bool expected_value)
{
    while (node != NULL && node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        node = child_at(node, 0U);
    }
    if (node == NULL || node->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }
    if (node->literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        return expected_value;
    }
    if (node->literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        return !expected_value;
    }
    if (node->literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        char *text = copy_span_text(node->span.text, node->span.length);
        char *end = NULL;
        long long value = 0;
        bool matches = false;

        if (text == NULL) {
            return false;
        }
        errno = 0;
        value = strtoll(text, &end, MYLITE_EXPRESSION_DECIMAL_BASE);
        if (errno == 0 && end != text && *end == '\0') {
            matches = (value != 0) == expected_value;
        }
        free(text);
        return matches;
    }
    return false;
}

static int eval_ternary(const struct mylite_sql_ast_node *node,
                        const struct mylite_expression_eval_context *context,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value)
{
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_BETWEEN ||
        node->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN) {
        return eval_between(node->operator_kind, node, context, warnings, out_value);
    }
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE ||
        node->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_LIKE) {
        return eval_like(node->operator_kind, node, context, warnings, out_value);
    }
    return -1;
}

static int eval_literal(const struct mylite_sql_ast_node *node,
                        struct mylite_expression_value *out_value)
{
    char *text = NULL;
    char *end = NULL;
    errno = 0;

    switch (node->literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    case MYLITE_SQL_AST_LITERAL_TRUE:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 1};
        return 0;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 0};
        return 0;
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        text = copy_span_text(node->span.text, node->span.length);
        if (text == NULL) {
            return -1;
        }
        if (text[0] != '-' && strlen(text) >= MYLITE_EXPRESSION_UINT64_DIGITS) {
            unsigned long long unsigned_value =
                strtoull(text, &end, MYLITE_EXPRESSION_DECIMAL_BASE);
            if (errno == 0 && end != text && *end == '\0' &&
                unsigned_value > (unsigned long long)INT64_MAX) {
                out_value->kind = MYLITE_EXPRESSION_VALUE_UINT64;
                out_value->uint64_value = (uint64_t)unsigned_value;
                free(text);
                return 0;
            }
        }
        out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
        out_value->int64_value = strtoll(text, NULL, MYLITE_EXPRESSION_DECIMAL_BASE);
        free(text);
        return 0;
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        text = copy_span_text(node->span.text, node->span.length);
        if (text == NULL) {
            return -1;
        }
        out_value->kind = MYLITE_EXPRESSION_VALUE_REAL;
        out_value->real_value = strtod(text, NULL);
        free(text);
        return 0;
    case MYLITE_SQL_AST_LITERAL_STRING:
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
        out_value->text_value = decode_string_literal(node);
        return out_value->text_value == NULL ? -1 : 0;
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
    case MYLITE_SQL_AST_LITERAL_NONE:
        return -1;
    }
    return -1;
}

static int eval_is_expression(enum mylite_sql_ast_operator operator_kind,
                              const struct mylite_sql_ast_node *operand,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    int truth = -1;
    int result = 0;
    int status = eval_node(operand, context, warnings, &value);

    if (status != 0) {
        return status;
    }

    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
        result = value.kind == MYLITE_EXPRESSION_VALUE_NULL;
        break;
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
        result = value.kind != MYLITE_EXPRESSION_VALUE_NULL;
        break;
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
        status = truth_value(&value, warnings, &truth);
        if (status != 0) {
            break;
        }
        result = operator_kind == MYLITE_SQL_AST_OPERATOR_IS_TRUE ||
                         operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE
                     ? truth == 1
                     : truth == 0;
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE) {
            result = !result;
        }
        break;
    default:
        status = -1;
        break;
    }

    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return status;
    }
    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                  .int64_value = result ? 1 : 0};
    return 0;
}

static int eval_between(enum mylite_sql_ast_operator operator_kind,
                        const struct mylite_sql_ast_node *node,
                        const struct mylite_expression_eval_context *context,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    struct mylite_expression_value low = {0};
    struct mylite_expression_value high = {0};
    struct between_truth truth = {.low = -1, .high = -1};
    int status = eval_node(child_at(node, 0U), context, warnings, &value);

    if (status == 0) {
        status = eval_node(child_at(node, 1U), context, warnings, &low);
    }
    if (status == 0) {
        status = eval_node(child_at(node, 2U), context, warnings, &high);
    }
    if (status != 0) {
        goto cleanup;
    }
    status = eval_between_bound_truth(&value, &low, true, warnings, &truth.low);
    if (status == 0) {
        status = eval_between_bound_truth(&value, &high, false, warnings, &truth.high);
    }
    if (status == 0) {
        set_between_result(operator_kind, truth, out_value);
    }

cleanup:
    mylite_expression_value_deinit(&value);
    mylite_expression_value_deinit(&low);
    mylite_expression_value_deinit(&high);
    return status;
}

static int eval_between_bound_truth(const struct mylite_expression_value *value,
                                    const struct mylite_expression_value *bound, bool lower_bound,
                                    struct mylite_expression_warnings *warnings, int *out_truth)
{
    int comparison = 0;
    int status = 0;

    *out_truth = -1;
    if (is_null(value) || is_null(bound)) {
        return 0;
    }

    status = compare_values(value, bound, warnings, &comparison);
    if (status != 0) {
        return status;
    }
    *out_truth = lower_bound ? comparison >= 0 : comparison <= 0;
    return 0;
}

static void set_between_result(enum mylite_sql_ast_operator operator_kind,
                               struct between_truth truth,
                               struct mylite_expression_value *out_value)
{
    bool between = operator_kind == MYLITE_SQL_AST_OPERATOR_BETWEEN;

    if (truth.low == 0 || truth.high == 0) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = between ? 0 : 1};
    } else if (truth.low < 0 || truth.high < 0) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    } else {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = between ? 1 : 0};
    }
}

static int eval_like(enum mylite_sql_ast_operator operator_kind,
                     const struct mylite_sql_ast_node *node,
                     const struct mylite_expression_eval_context *context,
                     struct mylite_expression_warnings *warnings,
                     struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    struct mylite_expression_value pattern = {0};
    struct mylite_expression_value escape_value = {0};
    char *value_text = NULL;
    char *pattern_text = NULL;
    char *escape_text = NULL;
    char escape = '\\';
    int status = eval_node(child_at(node, 0U), context, warnings, &value);

    if (status == 0) {
        status = eval_node(child_at(node, 1U), context, warnings, &pattern);
    }
    if (status == 0 && child_at(node, 2U) != NULL) {
        status = eval_node(child_at(node, 2U), context, warnings, &escape_value);
    }
    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&value) || is_null(&pattern) ||
        (child_at(node, 2U) != NULL && is_null(&escape_value))) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = value_to_string(&value, &value_text);
    if (status == 0) {
        status = value_to_string(&pattern, &pattern_text);
    }
    if (status == 0 && child_at(node, 2U) != NULL) {
        status = value_to_string(&escape_value, &escape_text);
        if (status == 0 && strlen(escape_text) != 1U) {
            status = append_warning(warnings, MYLITE_WARNING_INCORRECT_ESCAPE_ARGUMENTS,
                                    "Incorrect arguments to ESCAPE");
            if (status == 0) {
                status = -1;
            }
        }
        if (status == 0) {
            escape = escape_text[0];
        }
    }
    if (status == 0) {
        bool result = like_match(value_text, pattern_text, escape);
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_LIKE) {
            result = !result;
        }
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = result ? 1 : 0};
    }

cleanup:
    free(value_text);
    free(pattern_text);
    free(escape_text);
    mylite_expression_value_deinit(&value);
    mylite_expression_value_deinit(&pattern);
    mylite_expression_value_deinit(&escape_value);
    return status;
}

static int eval_in(enum mylite_sql_ast_operator operator_kind,
                   const struct mylite_sql_ast_node *node,
                   const struct mylite_expression_eval_context *context,
                   struct mylite_expression_warnings *warnings,
                   struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    const struct mylite_sql_ast_node *list = child_at(node, 1U);
    bool saw_null = false;
    int status = eval_node(child_at(node, 0U), context, warnings, &value);

    if (status != 0) {
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_expression_value_deinit(&value);
        return 0;
    }

    for (const struct mylite_sql_ast_node *item = list == NULL ? NULL : list->first_child;
         item != NULL; item = item->next_sibling) {
        struct mylite_expression_value candidate = {0};
        int comparison = 0;

        status = eval_node(item, context, warnings, &candidate);
        if (status != 0) {
            mylite_expression_value_deinit(&candidate);
            break;
        }
        if (is_null(&candidate)) {
            saw_null = true;
            mylite_expression_value_deinit(&candidate);
            continue;
        }
        status = compare_values(&value, &candidate, warnings, &comparison);
        mylite_expression_value_deinit(&candidate);
        if (status != 0) {
            break;
        }
        if (comparison == 0) {
            bool result = operator_kind == MYLITE_SQL_AST_OPERATOR_IN;
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = result ? 1 : 0};
            mylite_expression_value_deinit(&value);
            return 0;
        }
    }
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return status;
    }
    if (saw_null) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_INT64,
        .int64_value = operator_kind == MYLITE_SQL_AST_OPERATOR_IN ? 0 : 1};
    return 0;
}

static int eval_numeric_unary(enum mylite_sql_ast_operator operator_kind,
                              const struct mylite_expression_value *operand,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value)
{
    struct numeric_value number = {0};
    int truth = -1;
    int status = 0;

    if (is_null(operand)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT) {
        status = truth_value(operand, warnings, &truth);
        if (status != 0) {
            return status;
        }
        if (truth < 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        } else {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = truth == 0 ? 1 : 0};
        }
        return 0;
    }

    status = value_to_numeric(operand, warnings, &number);
    if (status != 0) {
        return status;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_BITWISE_NOT) {
        uint64_t value = number.is_unsigned ? number.uint64_value : (uint64_t)number.int64_value;
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_UINT64,
                                                      .uint64_value = ~value};
        return 0;
    }
    if (number.is_integer && !number.is_unsigned) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE ? -number.int64_value
                                                                             : number.int64_value};
        return 0;
    }
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_REAL,
        .real_value = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE ? -number.real_value
                                                                        : number.real_value};
    return 0;
}

static int eval_arithmetic(enum mylite_sql_ast_operator operator_kind,
                           const struct mylite_expression_value *left,
                           const struct mylite_expression_value *right,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value)
{
    struct numeric_value left_number = {0};
    struct numeric_value right_number = {0};
    int status = 0;

    if (is_null(left) || is_null(right)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    status = value_to_numeric(left, warnings, &left_number);
    if (status == 0) {
        status = value_to_numeric(right, warnings, &right_number);
    }
    if (status != 0) {
        return status;
    }
    if ((operator_kind == MYLITE_SQL_AST_OPERATOR_DIVIDE ||
         operator_kind == MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE ||
         operator_kind == MYLITE_SQL_AST_OPERATOR_MODULO) &&
        right_number.real_value == 0.0) {
        status = append_warning(warnings, MYLITE_WARNING_DIVISION_BY_ZERO, "Division by 0");
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return status;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_DIVIDE) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_REAL,
                                                      .real_value = left_number.real_value /
                                                                    right_number.real_value};
        return 0;
    }

    if (left_number.is_integer && right_number.is_integer) {
        int64_t left_int =
            left_number.is_unsigned ? (int64_t)left_number.uint64_value : left_number.int64_value;
        int64_t right_int = right_number.is_unsigned ? (int64_t)right_number.uint64_value
                                                     : right_number.int64_value;
        out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
        switch (operator_kind) {
        case MYLITE_SQL_AST_OPERATOR_ADD:
            out_value->int64_value = left_int + right_int;
            return 0;
        case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
            out_value->int64_value = left_int - right_int;
            return 0;
        case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
            out_value->int64_value = left_int * right_int;
            return 0;
        case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
            out_value->int64_value = left_int / right_int;
            return 0;
        case MYLITE_SQL_AST_OPERATOR_MODULO:
            out_value->int64_value = left_int % right_int;
            return 0;
        default:
            break;
        }
    }

    out_value->kind = MYLITE_EXPRESSION_VALUE_REAL;
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
        out_value->real_value = left_number.real_value + right_number.real_value;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        out_value->real_value = left_number.real_value - right_number.real_value;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        out_value->real_value = left_number.real_value * right_number.real_value;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
        out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
        out_value->int64_value = (int64_t)(left_number.real_value / right_number.real_value);
        return 0;
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        out_value->real_value =
            left_number.real_value -
            ((double)((int64_t)(left_number.real_value / right_number.real_value)) *
             right_number.real_value);
        return 0;
    default:
        break;
    }
    return -1;
}

static int eval_bitwise(enum mylite_sql_ast_operator operator_kind,
                        const struct mylite_expression_value *left,
                        const struct mylite_expression_value *right,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value)
{
    struct numeric_value left_number = {0};
    struct numeric_value right_number = {0};
    uint64_t left_int = 0U;
    uint64_t right_int = 0U;
    int status = 0;

    if (is_null(left) || is_null(right)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    status = value_to_numeric(left, warnings, &left_number);
    if (status == 0) {
        status = value_to_numeric(right, warnings, &right_number);
    }
    if (status != 0) {
        return status;
    }
    left_int =
        left_number.is_unsigned ? left_number.uint64_value : (uint64_t)left_number.int64_value;
    right_int =
        right_number.is_unsigned ? right_number.uint64_value : (uint64_t)right_number.int64_value;
    out_value->kind = MYLITE_EXPRESSION_VALUE_UINT64;
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
        out_value->uint64_value = left_int & right_int;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
        out_value->uint64_value = left_int ^ right_int;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
        out_value->uint64_value = left_int | right_int;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
        out_value->uint64_value =
            right_int >= MYLITE_EXPRESSION_BITS_PER_UINT64 ? 0U : left_int << right_int;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
        out_value->uint64_value =
            right_int >= MYLITE_EXPRESSION_BITS_PER_UINT64 ? 0U : left_int >> right_int;
        return 0;
    default:
        break;
    }
    return -1;
}

static int eval_comparison(enum mylite_sql_ast_operator operator_kind,
                           const struct mylite_expression_value *left,
                           const struct mylite_expression_value *right,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value)
{
    int comparison = 0;
    bool result = false;

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        if (is_null(left) || is_null(right)) {
            result = is_null(left) && is_null(right);
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = result ? 1 : 0};
            return 0;
        }
    } else if (is_null(left) || is_null(right)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    if (compare_values(left, right, warnings, &comparison) != 0) {
        return -1;
    }

    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        result = comparison == 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        result = comparison != 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_LESS:
        result = comparison < 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        result = comparison <= 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        result = comparison > 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        result = comparison >= 0;
        break;
    default:
        return -1;
    }
    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                  .int64_value = result ? 1 : 0};
    return 0;
}

static int eval_logical(enum mylite_sql_ast_operator operator_kind,
                        const struct mylite_expression_value *left,
                        const struct mylite_expression_value *right,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value)
{
    int left_truth = -1;
    int right_truth = -1;
    int status = truth_value(left, warnings, &left_truth);

    if (status == 0) {
        status = truth_value(right, warnings, &right_truth);
    }
    if (status != 0) {
        return status;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_AND) {
        if (left_truth == 0 || right_truth == 0) {
            out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
            out_value->int64_value = 0;
        } else if (left_truth < 0 || right_truth < 0) {
            out_value->kind = MYLITE_EXPRESSION_VALUE_NULL;
        } else {
            out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
            out_value->int64_value = 1;
        }
        return 0;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_OR) {
        if (left_truth == 1 || right_truth == 1) {
            out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
            out_value->int64_value = 1;
        } else if (left_truth < 0 || right_truth < 0) {
            out_value->kind = MYLITE_EXPRESSION_VALUE_NULL;
        } else {
            out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
            out_value->int64_value = 0;
        }
        return 0;
    }
    if (left_truth < 0 || right_truth < 0) {
        out_value->kind = MYLITE_EXPRESSION_VALUE_NULL;
    } else {
        out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
        out_value->int64_value = left_truth != right_truth ? 1 : 0;
    }
    return 0;
}

static int truth_value(const struct mylite_expression_value *value,
                       struct mylite_expression_warnings *warnings, int *out_truth)
{
    struct numeric_value number = {0};
    int status = 0;

    if (is_null(value)) {
        *out_truth = -1;
        return 0;
    }
    status = value_to_numeric(value, warnings, &number);
    if (status != 0) {
        return status;
    }
    *out_truth = number.real_value == 0.0 ? 0 : 1;
    return 0;
}

static int compare_values(const struct mylite_expression_value *left,
                          const struct mylite_expression_value *right,
                          struct mylite_expression_warnings *warnings, int *out_compare)
{
    if (is_numeric_kind(left->kind) || is_numeric_kind(right->kind)) {
        struct numeric_value left_number = {0};
        struct numeric_value right_number = {0};
        int status = value_to_numeric(left, warnings, &left_number);

        if (status == 0) {
            status = value_to_numeric(right, warnings, &right_number);
        }
        if (status != 0) {
            return status;
        }
        *out_compare = (left_number.real_value > right_number.real_value) -
                       (left_number.real_value < right_number.real_value);
        return 0;
    }

    char *left_text = NULL;
    char *right_text = NULL;
    int status = value_to_string(left, &left_text);

    if (status == 0) {
        status = value_to_string(right, &right_text);
    }
    if (status == 0) {
        int comparison = strcmp(left_text, right_text);
        *out_compare = (comparison > 0) - (comparison < 0);
    }
    free(left_text);
    free(right_text);
    return status;
}

static int value_to_numeric(const struct mylite_expression_value *value,
                            struct mylite_expression_warnings *warnings,
                            struct numeric_value *out_numeric)
{
    char *text = NULL;
    char *end = NULL;
    bool saw_digit = false;

    *out_numeric = (struct numeric_value){0};
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        out_numeric->int64_value = value->int64_value;
        out_numeric->uint64_value = (uint64_t)value->int64_value;
        out_numeric->real_value = (double)value->int64_value;
        out_numeric->is_integer = true;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        out_numeric->uint64_value = value->uint64_value;
        out_numeric->int64_value = (int64_t)value->uint64_value;
        out_numeric->real_value = (double)value->uint64_value;
        out_numeric->is_integer = true;
        out_numeric->is_unsigned = true;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        out_numeric->real_value = value->real_value;
        out_numeric->int64_value = (int64_t)value->real_value;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return -1;
    }

    text = value->text_value == NULL ? copy_span_text("", 0U)
                                     : copy_span_text(value->text_value, strlen(value->text_value));
    if (text == NULL) {
        return -1;
    }
    char *start = text;
    while (isspace((unsigned char)*start)) {
        ++start;
    }
    char *scan = start;
    if (*scan == '+' || *scan == '-') {
        ++scan;
    }
    for (; *scan != '\0'; ++scan) {
        if (isdigit((unsigned char)*scan)) {
            saw_digit = true;
            break;
        }
        if (*scan != '.') {
            break;
        }
    }
    if (!saw_digit) {
        int status = append_truncation_warning(warnings, text);
        free(text);
        return status;
    }
    errno = 0;
    out_numeric->real_value = strtod(start, &end);
    out_numeric->int64_value = (int64_t)out_numeric->real_value;
    while (end != NULL && isspace((unsigned char)*end)) {
        ++end;
    }
    if (end == start || (end != NULL && *end != '\0')) {
        int status = append_truncation_warning(warnings, text);
        free(text);
        return status;
    }
    free(text);
    return 0;
}

static int value_to_string(const struct mylite_expression_value *value, char **out_text)
{
    *out_text = mylite_expression_value_to_text(value);
    return *out_text == NULL && !is_null(value) ? -1 : 0;
}

static int append_warning(struct mylite_expression_warnings *warnings, unsigned int code,
                          const char *message)
{
    struct mylite_expression_warning *items = NULL;
    char *copy = NULL;

    if (warnings == NULL) {
        return 0;
    }
    copy = copy_span_text(message, strlen(message));
    if (copy == NULL) {
        return -1;
    }
    items = realloc(warnings->items, (warnings->count + 1U) * sizeof(*warnings->items));
    if (items == NULL) {
        free(copy);
        return -1;
    }
    warnings->items = items;
    warnings->items[warnings->count++] =
        (struct mylite_expression_warning){.code = code, .message = copy};
    return 0;
}

static int append_truncation_warning(struct mylite_expression_warnings *warnings, const char *text)
{
    char message[MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE];
    int length = snprintf(message, sizeof(message), "Truncated incorrect DOUBLE value: '%.*s'",
                          MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW, text == NULL ? "" : text);

    if (length < 0) {
        return -1;
    }
    return append_warning(warnings, MYLITE_WARNING_TRUNCATED_WRONG_VALUE, message);
}

static char *copy_span_text(const char *text, size_t length)
{
    char *copy = malloc(length + 1U);

    if (copy == NULL) {
        return NULL;
    }
    if (length != 0U) {
        memcpy(copy, text, length);
    }
    copy[length] = '\0';
    return copy;
}

static char *decode_string_literal(const struct mylite_sql_ast_node *node)
{
    const char *text = node->span.text;
    size_t length = node->span.length;
    size_t start = 0U;
    size_t end = length;
    char *decoded = NULL;
    size_t output = 0U;

    if (length >= 2U && (text[0] == '\'' || text[0] == '"')) {
        start = 1U;
        end = length - 1U;
    } else if (length >= 3U && (text[0] == 'N' || text[0] == 'n') &&
               (text[1] == '\'' || text[1] == '"')) {
        start = 2U;
        end = length - 1U;
    }

    decoded = malloc(end >= start ? end - start + 1U : 1U);
    if (decoded == NULL) {
        return NULL;
    }
    for (size_t index = start; index < end; ++index) {
        if (text[index] == '\\' && index + 1U < end) {
            char escaped = '\0';

            if (decode_string_escape(text[index + 1U], &escaped)) {
                decoded[output++] = escaped;
                ++index;
            } else {
                decoded[output++] = text[index];
            }
        } else if ((text[index] == '\'' || text[index] == '"') && index + 1U < end &&
                   text[index + 1U] == text[index]) {
            decoded[output++] = text[index++];
        } else {
            decoded[output++] = text[index];
        }
    }
    decoded[output] = '\0';
    return decoded;
}

static bool decode_string_escape(char escaped, char *out_character)
{
    switch (escaped) {
    case '\'':
    case '"':
    case '\\':
        *out_character = escaped;
        return true;
    case 'b':
        *out_character = '\b';
        return true;
    case 'n':
        *out_character = '\n';
        return true;
    case 'r':
        *out_character = '\r';
        return true;
    case 't':
        *out_character = '\t';
        return true;
    case 'Z':
        *out_character = '\x1A';
        return true;
    default:
        return false;
    }
}

static const struct mylite_sql_ast_node *child_at(const struct mylite_sql_ast_node *node,
                                                  size_t index)
{
    const struct mylite_sql_ast_node *child = node == NULL ? NULL : node->first_child;

    for (size_t current = 0U; current < index && child != NULL; ++current) {
        child = child->next_sibling;
    }
    return child;
}

static bool is_null(const struct mylite_expression_value *value)
{
    return value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL;
}

static bool is_numeric_kind(enum mylite_expression_value_kind kind)
{
    return kind == MYLITE_EXPRESSION_VALUE_INT64 || kind == MYLITE_EXPRESSION_VALUE_UINT64 ||
           kind == MYLITE_EXPRESSION_VALUE_REAL;
}

static bool like_match(const char *value, const char *pattern, char escape)
{
    return like_match_here(value == NULL ? "" : value, pattern == NULL ? "" : pattern, escape);
}

static bool like_match_here(const char *value, const char *pattern, char escape)
{
    if (*pattern == '\0') {
        return *value == '\0';
    }
    if (*pattern == '%') {
        do {
            if (like_match_here(value, pattern + 1, escape)) {
                return true;
            }
        } while (*value++ != '\0');
        return false;
    }
    if (*pattern == escape && pattern[1] != '\0') {
        return *value != '\0' &&
               ascii_case_fold((unsigned char)*value) ==
                   ascii_case_fold((unsigned char)pattern[1]) &&
               like_match_here(value + 1, pattern + 2, escape);
    }
    if (*pattern == '_') {
        return *value != '\0' && like_match_here(value + 1, pattern + 1, escape);
    }
    return *value != '\0' &&
           ascii_case_fold((unsigned char)*value) == ascii_case_fold((unsigned char)*pattern) &&
           like_match_here(value + 1, pattern + 1, escape);
}

static int ascii_case_fold(int character)
{
    return character >= 'A' && character <= 'Z' ? character - 'A' + 'a' : character;
}

// NOLINTEND(misc-no-recursion, readability-implicit-bool-conversion)
