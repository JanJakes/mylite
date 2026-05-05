#include "mylite_select_row_compare.h"

#include "mylite_expression.h"

struct mylite_select_row_order_comparison {
    enum mylite_sql_ast_operator operator_kind;
    int comparison;
};

static int compare_row_values_for_equality(const struct mylite_row_expression_values *left,
                                           const struct mylite_row_expression_values *right,
                                           struct mylite_expression_warnings *warnings,
                                           int *out_truth);
static int
compare_row_values_for_null_safe_equality(const struct mylite_row_expression_values *left,
                                          const struct mylite_row_expression_values *right,
                                          struct mylite_expression_warnings *warnings,
                                          int *out_truth);
static int compare_row_values_for_order(enum mylite_sql_ast_operator operator_kind,
                                        const struct mylite_row_expression_values *left,
                                        const struct mylite_row_expression_values *right,
                                        struct mylite_expression_warnings *warnings,
                                        int *out_truth);
static int row_order_comparison_truth(struct mylite_select_row_order_comparison comparison,
                                      int *out_truth);

bool mylite_select_row_expression_values_has_null(const struct mylite_row_expression_values *values)
{
    if (values == NULL) {
        return false;
    }
    for (size_t index = 0U; index < values->count; ++index) {
        if (values->items[index].kind == MYLITE_EXPRESSION_VALUE_NULL) {
            return true;
        }
    }
    return false;
}

int mylite_select_compare_row_values(enum mylite_sql_ast_operator operator_kind,
                                     const struct mylite_row_expression_values *left,
                                     const struct mylite_row_expression_values *right,
                                     struct mylite_expression_warnings *warnings, int *out_truth)
{
    int truth = -1;
    int status = MYLITE_OK;

    if (left == NULL || right == NULL || left->count != right->count || out_truth == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return compare_row_values_for_equality(left, right, warnings, out_truth);
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        return compare_row_values_for_null_safe_equality(left, right, warnings, out_truth);
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        status = compare_row_values_for_equality(left, right, warnings, &truth);
        if (status != MYLITE_OK || truth < 0) {
            *out_truth = truth;
            return status;
        }
        *out_truth = truth == 0 ? 1 : 0;
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return compare_row_values_for_order(operator_kind, left, right, warnings, out_truth);
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
    return MYLITE_UNSUPPORTED;
}

static int compare_row_values_for_equality(const struct mylite_row_expression_values *left,
                                           const struct mylite_row_expression_values *right,
                                           struct mylite_expression_warnings *warnings,
                                           int *out_truth)
{
    bool saw_unknown = false;

    for (size_t index = 0U; index < left->count; ++index) {
        const struct mylite_expression_value *left_value = &left->items[index];
        const struct mylite_expression_value *right_value = &right->items[index];
        int comparison = 0;
        int status = MYLITE_OK;

        if (left_value->kind == MYLITE_EXPRESSION_VALUE_NULL ||
            right_value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
            saw_unknown = true;
            continue;
        }
        status = mylite_expression_value_compare(left_value, right_value, warnings, &comparison);
        if (status != 0) {
            return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_UNSUPPORTED;
        }
        if (comparison != 0) {
            *out_truth = 0;
            return MYLITE_OK;
        }
    }
    if (saw_unknown) {
        *out_truth = -1;
    } else {
        *out_truth = 1;
    }
    return MYLITE_OK;
}

static int
compare_row_values_for_null_safe_equality(const struct mylite_row_expression_values *left,
                                          const struct mylite_row_expression_values *right,
                                          struct mylite_expression_warnings *warnings,
                                          int *out_truth)
{
    bool saw_null = false;
    bool last_both_null = false;

    for (size_t index = 0U; index < left->count; ++index) {
        const struct mylite_expression_value *left_value = &left->items[index];
        const struct mylite_expression_value *right_value = &right->items[index];
        bool left_null = left_value->kind == MYLITE_EXPRESSION_VALUE_NULL;
        bool right_null = right_value->kind == MYLITE_EXPRESSION_VALUE_NULL;
        int comparison = 0;
        int status = MYLITE_OK;

        last_both_null = false;
        if (left_null || right_null) {
            saw_null = true;
            if (left_null != right_null) {
                *out_truth = 0;
                return MYLITE_OK;
            }
            last_both_null = true;
            continue;
        }
        status = mylite_expression_value_compare(left_value, right_value, warnings, &comparison);
        if (status != 0) {
            return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_UNSUPPORTED;
        }
        if (comparison != 0) {
            *out_truth = 0;
            return MYLITE_OK;
        }
    }
    if (!saw_null || last_both_null) {
        *out_truth = 1;
    } else {
        *out_truth = 0;
    }
    return MYLITE_OK;
}

static int compare_row_values_for_order(enum mylite_sql_ast_operator operator_kind,
                                        const struct mylite_row_expression_values *left,
                                        const struct mylite_row_expression_values *right,
                                        struct mylite_expression_warnings *warnings, int *out_truth)
{
    for (size_t index = 0U; index < left->count; ++index) {
        const struct mylite_expression_value *left_value = &left->items[index];
        const struct mylite_expression_value *right_value = &right->items[index];
        int comparison = 0;
        int status = MYLITE_OK;

        if (left_value->kind == MYLITE_EXPRESSION_VALUE_NULL ||
            right_value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
            *out_truth = -1;
            return MYLITE_OK;
        }
        status = mylite_expression_value_compare(left_value, right_value, warnings, &comparison);
        if (status != 0) {
            return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_UNSUPPORTED;
        }
        if (comparison != 0) {
            struct mylite_select_row_order_comparison order_comparison = {
                .operator_kind = operator_kind,
                .comparison = comparison,
            };

            return row_order_comparison_truth(order_comparison, out_truth);
        }
    }

    struct mylite_select_row_order_comparison order_comparison = {
        .operator_kind = operator_kind,
        .comparison = 0,
    };

    return row_order_comparison_truth(order_comparison, out_truth);
}

static int row_order_comparison_truth(struct mylite_select_row_order_comparison comparison,
                                      int *out_truth)
{
    bool matched = false;

    switch (comparison.operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_LESS:
        matched = comparison.comparison < 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        matched = comparison.comparison <= 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        matched = comparison.comparison > 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        matched = comparison.comparison >= 0;
        break;
    default:
        return MYLITE_UNSUPPORTED;
    }
    if (matched) {
        *out_truth = 1;
    } else {
        *out_truth = 0;
    }
    return MYLITE_OK;
}
