#include "mylite_expression_descriptor_operator.h"

#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_expression_collation.h"
#include "mylite_expression_descriptor.h"
#include "mylite_expression_descriptor_numeric.h"
#include "mylite_expression_descriptor_subquery.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int validate_operator_descriptor_callbacks(
    const struct mylite_expression_descriptor_operator_callbacks *callbacks
);

static int infer_collate_expression_descriptor(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_field_descriptor *left,
    struct mylite_field_descriptor *out_descriptor
);

static struct mylite_charset_collation_info collate_source_collation_info(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_field_descriptor *left
);

static bool cast_target_collation_info(
    mylite_db *database,
    const struct mylite_sql_ast_node *target,
    struct mylite_charset_collation_info *out_info
);

static bool collation_sets_binary_flag(const struct mylite_collation *collation);

static struct mylite_field_descriptor negative_expression_descriptor(
    const struct mylite_field_descriptor *operand
);

static uint64_t json_document_result_length(mylite_db *database);

static uint64_t json_unquote_json_result_length(mylite_db *database);

static bool descriptor_has_integer_result(const struct mylite_field_descriptor *descriptor);

static bool descriptor_uses_double_arithmetic(const struct mylite_field_descriptor *descriptor);

static bool infer_decimal_arithmetic_descriptor(
    enum mylite_sql_ast_operator operator_kind,
    const struct mylite_field_descriptor *left,
    const struct mylite_field_descriptor *right,
    bool nullable,
    struct mylite_field_descriptor *out_descriptor
);

static bool descriptor_has_exact_numeric_result(const struct mylite_field_descriptor *descriptor);

static unsigned int decimal_arithmetic_scale(
    enum mylite_sql_ast_operator operator_kind,
    const struct mylite_field_descriptor *left,
    const struct mylite_field_descriptor *right
);

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_descriptor_infer_unary_expression(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_operator_callbacks *callbacks
) {
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
            database,
            plan,
            mylite_ast_child_at(expression, 0U),
            NULL,
            &operand
        );
        if (status != MYLITE_OK) {
            return status;
        }
        *out_descriptor = mylite_expression_descriptor_boolean(
            mylite_expression_descriptor_is_nullable(&operand)
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
        status = callbacks->infer_expression_descriptor(
            database,
            plan,
            mylite_ast_child_at(expression, 0U),
            NULL,
            &operand
        );
        if (status != MYLITE_OK) {
            return status;
        }
        *out_descriptor = mylite_expression_descriptor_unsigned_longlong(
            mylite_expression_descriptor_is_nullable(&operand)
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_BINARY_CAST:
        status = callbacks->infer_expression_descriptor(
            database,
            plan,
            mylite_ast_child_at(expression, 0U),
            value,
            &operand
        );
        if (status != MYLITE_OK) {
            return status;
        }
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = MYLITE_FIELD_FLAG_BINARY,
            .length = operand.length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };
        mylite_field_descriptor_set_nullable(out_descriptor, true);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
        status = callbacks->infer_expression_descriptor(
            database,
            plan,
            mylite_ast_child_at(expression, 0U),
            value,
            &operand
        );
        if (status != MYLITE_OK) {
            return status;
        }
        nullable = mylite_expression_descriptor_is_nullable(&operand);
        mylite_field_descriptor_set_nullable(&operand, nullable);
        *out_descriptor = operand;
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        status = callbacks->infer_expression_descriptor(
            database,
            plan,
            mylite_ast_child_at(expression, 0U),
            value,
            &operand
        );
        if (status != MYLITE_OK) {
            return status;
        }
        *out_descriptor = negative_expression_descriptor(&operand);
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
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_COLLATE:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_from_value(value);
    return MYLITE_OK;
}

static struct mylite_field_descriptor negative_expression_descriptor(
    const struct mylite_field_descriptor *operand
) {
    bool nullable = mylite_expression_descriptor_is_nullable(operand);

    if (descriptor_has_integer_result(operand)) {
        uint64_t length = operand == NULL ? 0U : operand->length;
        struct mylite_field_descriptor descriptor =
            mylite_expression_descriptor_signed_longlong(nullable);

        if (operand != NULL && (operand->flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U) {
            ++length;
        }
        descriptor.length = mylite_expression_descriptor_max_u64(length, 2U);
        return descriptor;
    }
    if (operand != NULL && operand->type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
        struct mylite_field_descriptor descriptor = {
            .type = MYLITE_FIELD_TYPE_NEWDECIMAL,
            .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length =
                operand->length + ((operand->flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U ? 1U : 0U),
            .decimals = operand->decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = nullable,
        };

        mylite_field_descriptor_set_nullable(&descriptor, nullable);
        return descriptor;
    }
    if (operand != NULL && operand->type == MYLITE_FIELD_TYPE_NULL) {
        struct mylite_field_descriptor descriptor = {
            .type = MYLITE_FIELD_TYPE_DOUBLE,
            .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = 17U,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };

        mylite_field_descriptor_set_nullable(&descriptor, true);
        return descriptor;
    }
    return mylite_expression_descriptor_numeric_double_function(nullable);
}

static bool descriptor_has_integer_result(const struct mylite_field_descriptor *descriptor) {
    if (descriptor == NULL) {
        return false;
    }

    switch (descriptor->type) {
    case MYLITE_FIELD_TYPE_TINY:
    case MYLITE_FIELD_TYPE_SHORT:
    case MYLITE_FIELD_TYPE_LONG:
    case MYLITE_FIELD_TYPE_LONGLONG:
    case MYLITE_FIELD_TYPE_INT24:
    case MYLITE_FIELD_TYPE_YEAR:
        return true;
    case MYLITE_FIELD_TYPE_DECIMAL:
    case MYLITE_FIELD_TYPE_FLOAT:
    case MYLITE_FIELD_TYPE_DOUBLE:
    case MYLITE_FIELD_TYPE_NULL:
    case MYLITE_FIELD_TYPE_TIMESTAMP:
    case MYLITE_FIELD_TYPE_DATE:
    case MYLITE_FIELD_TYPE_TIME:
    case MYLITE_FIELD_TYPE_DATETIME:
    case MYLITE_FIELD_TYPE_NEWDATE:
    case MYLITE_FIELD_TYPE_VARCHAR:
    case MYLITE_FIELD_TYPE_BIT:
    case MYLITE_FIELD_TYPE_JSON:
    case MYLITE_FIELD_TYPE_NEWDECIMAL:
    case MYLITE_FIELD_TYPE_ENUM:
    case MYLITE_FIELD_TYPE_SET:
    case MYLITE_FIELD_TYPE_TINY_BLOB:
    case MYLITE_FIELD_TYPE_MEDIUM_BLOB:
    case MYLITE_FIELD_TYPE_LONG_BLOB:
    case MYLITE_FIELD_TYPE_BLOB:
    case MYLITE_FIELD_TYPE_VAR_STRING:
    case MYLITE_FIELD_TYPE_STRING:
    case MYLITE_FIELD_TYPE_GEOMETRY:
    default:
        return false;
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_descriptor_infer_binary_expression(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_operator_callbacks *callbacks
) {
    struct mylite_field_descriptor left = mylite_expression_descriptor_defaults();
    struct mylite_field_descriptor right = mylite_expression_descriptor_defaults();
    bool nullable = true;
    int status = validate_operator_descriptor_callbacks(callbacks);

    if (status != MYLITE_OK) {
        return status;
    }

    status = mylite_expression_descriptor_infer_binary_subquery_expression(
        database,
        plan,
        expression,
        out_descriptor,
        callbacks->subquery_callbacks
    );
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }

    status = callbacks->infer_expression_descriptor(
        database,
        plan,
        mylite_ast_child_at(expression, 0U),
        expression->operator_kind == MYLITE_SQL_AST_OPERATOR_COLLATE ? value : NULL,
        &left
    );

    if (status != MYLITE_OK) {
        return status;
    }
    if (expression->operator_kind == MYLITE_SQL_AST_OPERATOR_COLLATE) {
        return infer_collate_expression_descriptor(database, expression, &left, out_descriptor);
    }

    status = callbacks->infer_expression_descriptor(
        database,
        plan,
        mylite_ast_child_at(expression, 1U),
        NULL,
        &right
    );
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
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY: {
        bool result_unsigned = (left.flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U ||
                               (right.flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U;

        if (infer_decimal_arithmetic_descriptor(
                expression->operator_kind,
                &left,
                &right,
                nullable,
                out_descriptor
            )) {
            return MYLITE_OK;
        }
        if (descriptor_uses_double_arithmetic(&left) || descriptor_uses_double_arithmetic(&right)) {
            *out_descriptor = mylite_expression_descriptor_numeric_double_function(nullable);
            return MYLITE_OK;
        }
        *out_descriptor = mylite_expression_descriptor_signed_longlong(nullable);
        if (result_unsigned) {
            out_descriptor->flags |= MYLITE_FIELD_FLAG_UNSIGNED;
        }
        if (expression->operator_kind == MYLITE_SQL_AST_OPERATOR_MULTIPLY) {
            out_descriptor->length = result_unsigned ? 20U : 21U;
        } else {
            out_descriptor->length =
                mylite_expression_descriptor_max_u64(left.length, right.length) + 1U;
        }
        return MYLITE_OK;
    }
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
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_JSON,
            .flags = MYLITE_FIELD_FLAG_BINARY,
            .length = json_document_result_length(database),
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_charset_id(database),
            .nullable = true,
        };
        mylite_field_descriptor_set_nullable(out_descriptor, true);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_LONG_BLOB,
            .flags = MYLITE_FIELD_FLAG_BINARY,
            .length = json_unquote_json_result_length(database),
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_charset_id(database),
            .nullable = true,
        };
        mylite_field_descriptor_set_nullable(out_descriptor, true);
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
    case MYLITE_SQL_AST_OPERATOR_BINARY_CAST:
    case MYLITE_SQL_AST_OPERATOR_COLLATE:
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

static bool descriptor_uses_double_arithmetic(const struct mylite_field_descriptor *descriptor) {
    if (descriptor == NULL) {
        return false;
    }
    if (descriptor->type == MYLITE_FIELD_TYPE_FLOAT ||
        descriptor->type == MYLITE_FIELD_TYPE_DOUBLE) {
        return true;
    }
    return mylite_expression_descriptor_has_text_result(descriptor);
}

static bool infer_decimal_arithmetic_descriptor(
    enum mylite_sql_ast_operator operator_kind,
    const struct mylite_field_descriptor *left,
    const struct mylite_field_descriptor *right,
    bool nullable,
    struct mylite_field_descriptor *out_descriptor
) {
    struct mylite_field_descriptor descriptor = {0};
    uint64_t length = 0U;

    if (!mylite_expression_descriptor_has_decimal_result(left) &&
        !mylite_expression_descriptor_has_decimal_result(right)) {
        return false;
    }
    if (!descriptor_has_exact_numeric_result(left) || !descriptor_has_exact_numeric_result(right)) {
        return false;
    }
    length = mylite_expression_descriptor_max_u64(left->length, right->length);
    descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_NEWDECIMAL,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = length == UINT64_MAX ? UINT64_MAX : length + 1U,
        .decimals = decimal_arithmetic_scale(operator_kind, left, right),
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = nullable,
    };
    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    *out_descriptor = descriptor;
    return true;
}

static bool descriptor_has_exact_numeric_result(const struct mylite_field_descriptor *descriptor) {
    if (descriptor == NULL) {
        return false;
    }

    switch (descriptor->type) {
    case MYLITE_FIELD_TYPE_TINY:
    case MYLITE_FIELD_TYPE_SHORT:
    case MYLITE_FIELD_TYPE_LONG:
    case MYLITE_FIELD_TYPE_LONGLONG:
    case MYLITE_FIELD_TYPE_INT24:
    case MYLITE_FIELD_TYPE_YEAR:
    case MYLITE_FIELD_TYPE_NEWDECIMAL:
        return true;
    case MYLITE_FIELD_TYPE_DECIMAL:
    case MYLITE_FIELD_TYPE_FLOAT:
    case MYLITE_FIELD_TYPE_DOUBLE:
    case MYLITE_FIELD_TYPE_NULL:
    case MYLITE_FIELD_TYPE_TIMESTAMP:
    case MYLITE_FIELD_TYPE_DATE:
    case MYLITE_FIELD_TYPE_TIME:
    case MYLITE_FIELD_TYPE_DATETIME:
    case MYLITE_FIELD_TYPE_NEWDATE:
    case MYLITE_FIELD_TYPE_VARCHAR:
    case MYLITE_FIELD_TYPE_BIT:
    case MYLITE_FIELD_TYPE_JSON:
    case MYLITE_FIELD_TYPE_ENUM:
    case MYLITE_FIELD_TYPE_SET:
    case MYLITE_FIELD_TYPE_TINY_BLOB:
    case MYLITE_FIELD_TYPE_MEDIUM_BLOB:
    case MYLITE_FIELD_TYPE_LONG_BLOB:
    case MYLITE_FIELD_TYPE_BLOB:
    case MYLITE_FIELD_TYPE_VAR_STRING:
    case MYLITE_FIELD_TYPE_STRING:
    case MYLITE_FIELD_TYPE_GEOMETRY:
    default:
        return false;
    }
}

static unsigned int decimal_arithmetic_scale(
    enum mylite_sql_ast_operator operator_kind,
    const struct mylite_field_descriptor *left,
    const struct mylite_field_descriptor *right
) {
    unsigned int left_decimals =
        mylite_expression_descriptor_has_decimal_result(left) ? left->decimals : 0U;
    unsigned int right_decimals =
        mylite_expression_descriptor_has_decimal_result(right) ? right->decimals : 0U;

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_MULTIPLY) {
        if (left_decimals > UINT32_MAX - right_decimals) {
            return UINT32_MAX;
        }
        return left_decimals + right_decimals;
    }
    return left_decimals > right_decimals ? left_decimals : right_decimals;
}

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_descriptor_infer_ternary_expression(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_operator_callbacks *callbacks
) {
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
    case MYLITE_SQL_AST_OPERATOR_BINARY_CAST:
    case MYLITE_SQL_AST_OPERATOR_COLLATE:
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
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_from_value(value);
    return MYLITE_OK;
}

static uint64_t json_document_result_length(mylite_db *database) {
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);

    if (max_bytes_per_character > UINT64_MAX / mylite_mysql_json_document_length) {
        return mylite_mysql_long_text_length;
    }
    return mylite_mysql_json_document_length * max_bytes_per_character;
}

static uint64_t json_unquote_json_result_length(mylite_db *database) {
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);

    if (max_bytes_per_character > 1U) {
        return mylite_mysql_long_text_length;
    }
    return mylite_mysql_json_document_length * 4U;
}

static int infer_collate_expression_descriptor(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_field_descriptor *left,
    struct mylite_field_descriptor *out_descriptor
) {
    const struct mylite_sql_ast_node *collation_node = mylite_ast_child_at(expression, 1U);
    char *collation_name = mylite_copy_schema_text_span(collation_node);
    const struct mylite_collation *collation = NULL;
    const struct mylite_charset_collation_info source =
        collate_source_collation_info(database, expression, left);
    int status = MYLITE_OK;

    if (collation_name == NULL) {
        return MYLITE_NOMEM;
    }
    collation = mylite_collation_lookup(collation_name);
    if (collation == NULL) {
        status = mylite_diagnostics_set_unknown_collation_error(database, collation_name);
    } else if (
        source.coercibility != mylite_mysql_coercibility_numeric &&
        !mylite_ascii_case_equal(source.character_set, collation->character_set)
    ) {
        status = mylite_diagnostics_set_collation_charset_error(
            database,
            collation->name,
            source.character_set
        );
    } else {
        *out_descriptor = *left;
        if (mylite_expression_descriptor_has_text_result(out_descriptor)) {
            if (collation_sets_binary_flag(collation)) {
                out_descriptor->flags |= MYLITE_FIELD_FLAG_BINARY;
            } else {
                out_descriptor->flags &= ~(unsigned int)MYLITE_FIELD_FLAG_BINARY;
            }
        }
    }
    free(collation_name);
    return status;
}

static struct mylite_charset_collation_info collate_source_collation_info(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_field_descriptor *left
) {
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *target = mylite_ast_child_at(source, 1U);
    struct mylite_charset_collation_info info = {0};

    if (source != NULL && source->kind == MYLITE_SQL_AST_CAST_EXPRESSION &&
        cast_target_collation_info(database, target, &info)) {
        return info;
    }
    return mylite_expression_descriptor_collation_info(left, mylite_mysql_coercibility_implicit);
}

static bool cast_target_collation_info(
    mylite_db *database,
    const struct mylite_sql_ast_node *target,
    struct mylite_charset_collation_info *out_info
) {
    char *charset_name = NULL;

    if (target == NULL || target->kind != MYLITE_SQL_AST_COLUMN_TYPE) {
        return false;
    }
    if (target->column_type == MYLITE_SQL_AST_COLUMN_TYPE_BINARY) {
        *out_info = mylite_expression_binary_collation_info(mylite_mysql_coercibility_implicit);
        return true;
    }
    if (target->column_type != MYLITE_SQL_AST_COLUMN_TYPE_CHAR) {
        return false;
    }
    if (!target->has_column_character_set) {
        *out_info = mylite_expression_connection_collation_info(
            database,
            mylite_mysql_coercibility_implicit
        );
        return true;
    }
    charset_name = mylite_copy_unquoted_span_text(target->column_character_set);
    if (charset_name == NULL) {
        return false;
    }
    *out_info = mylite_expression_charset_collation_info(charset_name);
    out_info->coercibility = mylite_mysql_coercibility_implicit;
    free(charset_name);
    return true;
}

static bool collation_sets_binary_flag(const struct mylite_collation *collation) {
    const char *name = collation == NULL ? NULL : collation->name;
    size_t length = name == NULL ? 0U : strlen(name);

    return mylite_ascii_case_equal(name, mylite_mysql_binary_charset_name) ||
           (length > 4U && mylite_ascii_case_equal(name + length - 4U, "_bin"));
}

static int validate_operator_descriptor_callbacks(
    const struct mylite_expression_descriptor_operator_callbacks *callbacks
) {
    if (callbacks == NULL || callbacks->infer_expression_descriptor == NULL ||
        callbacks->subquery_callbacks == NULL) {
        return MYLITE_MISUSE;
    }
    return MYLITE_OK;
}
