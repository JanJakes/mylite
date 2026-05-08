#include "mylite_expression_descriptor_numeric.h"

#include "mylite_expression_descriptor.h"
#include "mylite_expression_descriptor_numeric_format.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>

static int infer_round_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks
);

static int infer_truncate_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks
);

static bool round_function_argument_is_approximate_literal(
    const struct mylite_sql_ast_node *argument
);

static bool round_function_constant_scale(
    const struct mylite_sql_ast_node *argument,
    int *out_scale
);

static int round_function_descriptor_scale(int scale);

static void truncate_decimal_descriptor_for_constant_scale(
    struct mylite_field_descriptor *descriptor,
    const struct mylite_field_descriptor *source,
    int scale
);

int mylite_expression_descriptor_infer_numeric_variadic_function(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks
) {
    int status = MYLITE_OK;

    if (callbacks == NULL || callbacks->infer_expression_descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    status = infer_round_function_descriptor(
        database,
        plan,
        expression,
        value,
        result_nullable,
        out_descriptor,
        callbacks
    );
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = mylite_expression_descriptor_infer_format_function(
        database,
        plan,
        expression,
        out_descriptor,
        callbacks
    );
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    return infer_truncate_function_descriptor(
        database,
        plan,
        expression,
        value,
        result_nullable,
        out_descriptor,
        callbacks
    );
}

static int infer_round_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *value_argument = mylite_ast_child_at(arguments, 0U);
    const struct mylite_sql_ast_node *scale_argument = mylite_ast_child_at(arguments, 1U);
    struct mylite_field_descriptor value_descriptor = mylite_expression_descriptor_defaults();
    int scale = 0;
    int status = MYLITE_OK;

    if (name == NULL || !mylite_span_equal_ci(name->span, "ROUND")) {
        return MYLITE_UNSUPPORTED;
    }
    status =
        callbacks
            ->infer_expression_descriptor(database, plan, value_argument, NULL, &value_descriptor);
    if (status != MYLITE_OK) {
        return status;
    }

    if (round_function_argument_is_approximate_literal(value_argument)) {
        *out_descriptor = mylite_expression_descriptor_numeric_double_function(result_nullable);
        return MYLITE_OK;
    }

    if (value_descriptor.type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
        *out_descriptor = value_descriptor;
        out_descriptor->flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        if (round_function_constant_scale(scale_argument, &scale)) {
            int rounded_scale = round_function_descriptor_scale(scale);

            if (rounded_scale <= 0) {
                out_descriptor->decimals = 0U;
                if (out_descriptor->length > value_descriptor.decimals) {
                    out_descriptor->length -= value_descriptor.decimals;
                }
            } else if ((unsigned int)rounded_scale < out_descriptor->decimals) {
                out_descriptor->decimals = (unsigned int)rounded_scale;
            }
        }
        mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
        return MYLITE_OK;
    }
    if (value_descriptor.type == MYLITE_FIELD_TYPE_TINY ||
        value_descriptor.type == MYLITE_FIELD_TYPE_SHORT ||
        value_descriptor.type == MYLITE_FIELD_TYPE_LONG ||
        value_descriptor.type == MYLITE_FIELD_TYPE_LONGLONG ||
        value_descriptor.type == MYLITE_FIELD_TYPE_INT24 ||
        value_descriptor.type == MYLITE_FIELD_TYPE_YEAR) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(result_nullable);
        out_descriptor->length = mylite_mysql_signed_longlong_display_length;
        if ((value_descriptor.flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U) {
            out_descriptor->flags |= MYLITE_FIELD_FLAG_UNSIGNED;
        }
        return MYLITE_OK;
    }
    if (value_descriptor.type == MYLITE_FIELD_TYPE_NULL) {
        *out_descriptor = mylite_expression_descriptor_from_value(value);
        if (out_descriptor->type == MYLITE_FIELD_TYPE_NULL) {
            *out_descriptor = mylite_expression_descriptor_numeric_double_function(true);
        }
        return MYLITE_OK;
    }

    *out_descriptor = mylite_expression_descriptor_numeric_double_function(result_nullable);
    return MYLITE_OK;
}

static int infer_truncate_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *value_argument = mylite_ast_child_at(arguments, 0U);
    const struct mylite_sql_ast_node *scale_argument = mylite_ast_child_at(arguments, 1U);
    struct mylite_field_descriptor value_descriptor = mylite_expression_descriptor_defaults();
    int scale = 0;
    int status = MYLITE_OK;

    if (name == NULL || !mylite_span_equal_ci(name->span, "TRUNCATE")) {
        return MYLITE_UNSUPPORTED;
    }
    status =
        callbacks
            ->infer_expression_descriptor(database, plan, value_argument, NULL, &value_descriptor);
    if (status != MYLITE_OK) {
        return status;
    }

    if (round_function_argument_is_approximate_literal(value_argument)) {
        *out_descriptor = mylite_expression_descriptor_numeric_double_function(result_nullable);
        return MYLITE_OK;
    }

    if (value_descriptor.type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
        *out_descriptor = value_descriptor;
        out_descriptor->flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        if (round_function_constant_scale(scale_argument, &scale)) {
            truncate_decimal_descriptor_for_constant_scale(
                out_descriptor,
                &value_descriptor,
                scale
            );
        }
        mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
        return MYLITE_OK;
    }
    if (value_descriptor.type == MYLITE_FIELD_TYPE_TINY ||
        value_descriptor.type == MYLITE_FIELD_TYPE_SHORT ||
        value_descriptor.type == MYLITE_FIELD_TYPE_LONG ||
        value_descriptor.type == MYLITE_FIELD_TYPE_LONGLONG ||
        value_descriptor.type == MYLITE_FIELD_TYPE_INT24 ||
        value_descriptor.type == MYLITE_FIELD_TYPE_YEAR) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(result_nullable);
        out_descriptor->length = mylite_mysql_signed_longlong_display_length;
        if ((value_descriptor.flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U) {
            out_descriptor->flags |= MYLITE_FIELD_FLAG_UNSIGNED;
        }
        return MYLITE_OK;
    }
    if (value_descriptor.type == MYLITE_FIELD_TYPE_NULL) {
        *out_descriptor = mylite_expression_descriptor_from_value(value);
        if (out_descriptor->type == MYLITE_FIELD_TYPE_NULL) {
            *out_descriptor = mylite_expression_descriptor_numeric_double_function(true);
        }
        return MYLITE_OK;
    }

    *out_descriptor = mylite_expression_descriptor_numeric_double_function(result_nullable);
    return MYLITE_OK;
}

static bool round_function_argument_is_approximate_literal(
    const struct mylite_sql_ast_node *argument
) {
    argument = mylite_sql_ast_unwrap_parenthesized_expression(argument);
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        argument =
            mylite_sql_ast_unwrap_parenthesized_expression(mylite_ast_child_at(argument, 0U));
    }
    if (argument == NULL || argument->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }
    if (argument->literal_kind != MYLITE_SQL_AST_LITERAL_FLOAT) {
        return false;
    }
    return true;
}

static bool round_function_constant_scale(
    const struct mylite_sql_ast_node *argument,
    int *out_scale
) {
    bool negative = false;
    int64_t scale = 0;

    if (out_scale == NULL) {
        return false;
    }
    *out_scale = 0;
    if (argument == NULL) {
        return true;
    }
    while (argument != NULL && argument->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        argument = mylite_ast_child_at(argument, 0U);
    }
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        negative = argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
        argument = mylite_ast_child_at(argument, 0U);
        while (argument != NULL && argument->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
            argument = mylite_ast_child_at(argument, 0U);
        }
    }
    if (argument == NULL || argument->kind != MYLITE_SQL_AST_LITERAL ||
        argument->literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return false;
    }

    enum { decimal_base = 10 };

    for (size_t index = 0U; index < argument->span.length; ++index) {
        char character = argument->span.text[index];

        if (!isdigit((unsigned char)character)) {
            return false;
        }
        if (scale < INT64_MAX / decimal_base) {
            scale = (scale * decimal_base) + (int64_t)(character - '0');
        } else {
            scale = INT64_MAX;
        }
    }
    if (negative) {
        scale = -scale;
    }
    if (scale > INT_MAX) {
        *out_scale = INT_MAX;
    } else if (scale < INT_MIN) {
        *out_scale = INT_MIN;
    } else {
        *out_scale = (int)scale;
    }
    return true;
}

static int round_function_descriptor_scale(int scale) {
    enum { round_scale_limit = 30 };

    if (scale > round_scale_limit) {
        return round_scale_limit;
    }
    if (scale < -round_scale_limit) {
        return -round_scale_limit;
    }
    return scale;
}

static void truncate_decimal_descriptor_for_constant_scale(
    struct mylite_field_descriptor *descriptor,
    const struct mylite_field_descriptor *source,
    int scale
) {
    int truncated_scale = round_function_descriptor_scale(scale);
    uint64_t remove_length = 0U;

    if (descriptor == NULL || source == NULL) {
        return;
    }
    if (truncated_scale < 0 || truncated_scale == 0) {
        descriptor->decimals = 0U;
        remove_length = source->decimals == 0U ? 0U : (uint64_t)source->decimals + 1U;
    } else if ((unsigned int)truncated_scale < descriptor->decimals) {
        remove_length = (uint64_t)(descriptor->decimals - (unsigned int)truncated_scale);
        descriptor->decimals = (unsigned int)truncated_scale;
    }

    if (remove_length != 0U) {
        descriptor->length =
            descriptor->length > remove_length ? descriptor->length - remove_length : 1U;
    }
}
