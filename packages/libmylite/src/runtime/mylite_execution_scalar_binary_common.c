#include "mylite_execution_scalar_binary_internal.h"

int mylite_execution_scalar_binary_format_base_conversion_value(
    struct mylite_db *database,
    uint64_t value,
    unsigned int base,
    char *buffer,
    size_t buffer_size
) {
    static const char base_conversion_digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char reversed[binary_base_conversion_text_capacity];
    size_t digit_count = 0U;

    if (buffer == NULL || buffer_size == 0U || base < binary_base_conversion_binary_base ||
        base > binary_base_conversion_max_base) {
        mylite_execution_set_runtime_error(database, "failed to format base conversion value");
        return MYLITE_ERROR;
    }
    if (value == 0U) {
        if (buffer_size < 2U) {
            mylite_execution_set_runtime_error(database, "failed to format base conversion value");
            return MYLITE_ERROR;
        }
        buffer[0] = '0';
        buffer[1] = '\0';
        return MYLITE_OK;
    }

    while (value != 0U) {
        unsigned int digit = (unsigned int)(value % base);

        if (digit_count == sizeof(reversed)) {
            mylite_execution_set_runtime_error(database, "failed to format base conversion value");
            return MYLITE_ERROR;
        }
        reversed[digit_count] = base_conversion_digits[digit];
        ++digit_count;
        value /= base;
    }
    if (digit_count >= buffer_size) {
        mylite_execution_set_runtime_error(database, "failed to format base conversion value");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < digit_count; ++index) {
        buffer[index] = reversed[digit_count - index - 1U];
    }
    buffer[digit_count] = '\0';
    return MYLITE_OK;
}

int mylite_execution_scalar_binary_evaluate_base_conversion_direct_literal_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value,
    bool *out_handled
) {
    static const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    const struct mylite_sql_ast_node *literal = expression;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;
    bool has_sign = false;
    uint64_t magnitude = 0U;

    if (out_value == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (expression == NULL) {
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return MYLITE_OK;
        }
        literal = mylite_execution_unwrap_parenthesized_expression(
            mylite_execution_child_at(expression, 0U)
        );
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            return MYLITE_OK;
        }
        has_sign = true;
        is_negative = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_OK;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_value->is_null = true;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        out_value->integer = 1U;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_value->integer = 0U;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_OK;
    }
    *out_handled = true;

    if (mylite_execution_parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        mylite_execution_set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (!is_negative) {
        out_value->integer = magnitude;
        return MYLITE_OK;
    }
    if (magnitude > int64_min_magnitude) {
        mylite_execution_set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (magnitude == int64_min_magnitude) {
        out_value->integer = (uint64_t)INT64_MIN;
    } else if (magnitude != 0U) {
        out_value->integer = (uint64_t)(-(int64_t)magnitude);
    }
    return MYLITE_OK;
}
