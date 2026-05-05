#include "mylite_expression_descriptor_operator.h"

#include "mylite_expression_descriptor.h"
#include "mylite_expression_descriptor_subquery.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stdbool.h>

static int validate_operator_descriptor_callbacks(
    const struct mylite_expression_descriptor_operator_callbacks *callbacks);

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_descriptor_infer_unary_expression(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_operator_callbacks *callbacks)
{
    struct mylite_field_descriptor operand = mylite_expression_descriptor_defaults();
    bool nullable = true;
    int status = validate_operator_descriptor_callbacks(callbacks);

    if (status != MYLITE_OK) {
        return status;
    }

    switch (expression->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
        *out_descriptor = mylite_expression_descriptor_boolean(false);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
        status = callbacks->infer_expression_descriptor(
            database, plan, mylite_ast_child_at(expression, 0U), NULL, &operand);
        if (status != MYLITE_OK) {
            return status;
        }
        *out_descriptor = mylite_expression_descriptor_boolean(
            mylite_expression_descriptor_is_nullable(&operand));
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
        status = callbacks->infer_expression_descriptor(
            database, plan, mylite_ast_child_at(expression, 0U), NULL, &operand);
        if (status != MYLITE_OK) {
            return status;
        }
        *out_descriptor = mylite_expression_descriptor_unsigned_longlong(
            mylite_expression_descriptor_is_nullable(&operand));
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        status = callbacks->infer_expression_descriptor(
            database, plan, mylite_ast_child_at(expression, 0U), value, &operand);
        if (status != MYLITE_OK) {
            return status;
        }
        nullable = mylite_expression_descriptor_is_nullable(&operand);
        operand.flags &= ~(unsigned int)MYLITE_FIELD_FLAG_UNSIGNED;
        operand.length = mylite_expression_descriptor_max_u64(operand.length, 2U);
        mylite_field_descriptor_set_nullable(&operand, nullable);
        *out_descriptor = operand;
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_NOT_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_from_value(value);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_descriptor_infer_binary_expression(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_operator_callbacks *callbacks)
{
    struct mylite_field_descriptor left = mylite_expression_descriptor_defaults();
    struct mylite_field_descriptor right = mylite_expression_descriptor_defaults();
    bool nullable = true;
    int status = validate_operator_descriptor_callbacks(callbacks);

    if (status != MYLITE_OK) {
        return status;
    }

    status = mylite_expression_descriptor_infer_binary_subquery_expression(
        database, plan, expression, out_descriptor, callbacks->subquery_callbacks);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }

    status = callbacks->infer_expression_descriptor(
        database, plan, mylite_ast_child_at(expression, 0U), NULL, &left);

    if (status == MYLITE_OK) {
        status = callbacks->infer_expression_descriptor(
            database, plan, mylite_ast_child_at(expression, 1U), NULL, &right);
    }
    if (status != MYLITE_OK) {
        return status;
    }

    if (mylite_expression_descriptor_is_nullable(&left) ||
        mylite_expression_descriptor_is_nullable(&right)) {
        nullable = true;
    } else {
        nullable = false;
    }
    switch (expression->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        *out_descriptor = mylite_expression_descriptor_signed_longlong(nullable);
        out_descriptor->length =
            mylite_expression_descriptor_max_u64(left.length, right.length) + 1U;
        if ((left.flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U ||
            (right.flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U) {
            out_descriptor->flags |= MYLITE_FIELD_FLAG_UNSIGNED;
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        *out_descriptor = mylite_expression_descriptor_signed_longlong(nullable);
        out_descriptor->length = mylite_expression_descriptor_max_u64(left.length, right.length);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
        *out_descriptor = mylite_expression_descriptor_decimal(nullable);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
        *out_descriptor = mylite_expression_descriptor_unsigned_longlong(nullable);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_NOT_REGEXP: {
        bool descriptor_nullable = false;

        if (nullable) {
            bool forces_not_null =
                mylite_expression_descriptor_operator_forces_not_null(expression->operator_kind);

            if (!forces_not_null) {
                descriptor_nullable = true;
            }
        }
        *out_descriptor = mylite_expression_descriptor_boolean(descriptor_nullable);
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        *out_descriptor = mylite_expression_descriptor_boolean(false);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_from_value(value);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_descriptor_infer_ternary_expression(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_operator_callbacks *callbacks)
{
    bool nullable = false;
    int status = validate_operator_descriptor_callbacks(callbacks);

    (void)value;
    if (status != MYLITE_OK) {
        return status;
    }

    for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
         child = child->next_sibling) {
        struct mylite_field_descriptor child_descriptor = mylite_expression_descriptor_defaults();

        status =
            callbacks->infer_expression_descriptor(database, plan, child, NULL, &child_descriptor);
        if (status != MYLITE_OK) {
            return status;
        }
        if (mylite_expression_descriptor_is_nullable(&child_descriptor)) {
            nullable = true;
        }
    }

    switch (expression->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_NOT_REGEXP:
        *out_descriptor = mylite_expression_descriptor_boolean(nullable);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_from_value(value);
    return MYLITE_OK;
}

static int validate_operator_descriptor_callbacks(
    const struct mylite_expression_descriptor_operator_callbacks *callbacks)
{
    if (callbacks == NULL || callbacks->infer_expression_descriptor == NULL ||
        callbacks->subquery_callbacks == NULL) {
        return MYLITE_MISUSE;
    }
    return MYLITE_OK;
}
