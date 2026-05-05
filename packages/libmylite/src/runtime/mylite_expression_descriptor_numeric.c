#include "mylite_expression_descriptor_numeric.h"

#include "mylite_expression_descriptor.h"
#include "mylite_function_names.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

static bool infer_exp_function_descriptor(const struct mylite_sql_ast_node *name,
                                          struct mylite_field_descriptor *out_descriptor);
static bool infer_logarithm_function_descriptor(const struct mylite_sql_ast_node *name,
                                                struct mylite_field_descriptor *out_descriptor);
static bool infer_power_function_descriptor(const struct mylite_sql_ast_node *name,
                                            struct mylite_field_descriptor *out_descriptor);
static bool infer_sqrt_function_descriptor(const struct mylite_sql_ast_node *name,
                                           struct mylite_field_descriptor *out_descriptor);
static bool infer_trigonometric_function_descriptor(const struct mylite_sql_ast_node *name,
                                                    struct mylite_field_descriptor *out_descriptor);
static bool
infer_inverse_trigonometric_function_descriptor(const struct mylite_sql_ast_node *name,
                                                struct mylite_field_descriptor *out_descriptor);
static bool
infer_angle_conversion_function_descriptor(const struct mylite_sql_ast_node *name,
                                           bool result_nullable,
                                           struct mylite_field_descriptor *out_descriptor);
static int infer_round_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    bool result_nullable, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks);
static int infer_format_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks);
static uint64_t
format_function_result_character_length(const struct mylite_sql_ast_node *argument,
                                        const struct mylite_field_descriptor *argument_descriptor);
// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t format_literal_result_character_length(const struct mylite_sql_ast_node *argument);
static uint64_t format_literal_fraction_length(const struct mylite_sql_ast_node *argument);
static int infer_truncate_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    bool result_nullable, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks);
static bool
round_function_argument_is_approximate_literal(const struct mylite_sql_ast_node *argument);
static bool round_function_constant_scale(const struct mylite_sql_ast_node *argument,
                                          int *out_scale);
static int round_function_descriptor_scale(int scale);
static void
truncate_decimal_descriptor_for_constant_scale(struct mylite_field_descriptor *descriptor,
                                               const struct mylite_field_descriptor *source,
                                               int scale);
static struct mylite_field_descriptor double_function_descriptor(bool nullable);

bool mylite_expression_descriptor_infer_fixed_integer_function(
    const struct mylite_sql_ast_node *name, bool result_nullable,
    struct mylite_field_descriptor *out_descriptor)
{
    if (mylite_function_name_has_length_result(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(result_nullable);
        out_descriptor->length = mylite_mysql_length_function_display_length;
        return true;
    }
    if (mylite_function_name_is_bit_count(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(result_nullable);
        out_descriptor->length = mylite_mysql_bit_count_function_display_length;
        return true;
    }
    if (mylite_function_name_is_crc32(name)) {
        *out_descriptor = mylite_expression_descriptor_unsigned_longlong(result_nullable);
        out_descriptor->length = mylite_mysql_crc32_function_display_length;
        return true;
    }
    return false;
}

bool mylite_expression_descriptor_infer_math_function(
    const struct mylite_sql_ast_node *name, bool result_nullable,
    struct mylite_field_descriptor *out_descriptor)
{
    if (infer_exp_function_descriptor(name, out_descriptor)) {
        return true;
    }
    if (infer_logarithm_function_descriptor(name, out_descriptor)) {
        return true;
    }
    if (infer_power_function_descriptor(name, out_descriptor)) {
        return true;
    }
    if (infer_sqrt_function_descriptor(name, out_descriptor)) {
        return true;
    }
    if (infer_trigonometric_function_descriptor(name, out_descriptor)) {
        return true;
    }
    if (infer_inverse_trigonometric_function_descriptor(name, out_descriptor)) {
        return true;
    }
    return infer_angle_conversion_function_descriptor(name, result_nullable, out_descriptor);
}

bool mylite_expression_descriptor_infer_scalar_numeric_function(
    const struct mylite_sql_ast_node *name, const struct mylite_expression_value *value,
    bool result_nullable, struct mylite_field_descriptor *out_descriptor)
{
    if (name != NULL && mylite_span_equal_ci(name->span, "ISNULL")) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(false);
        out_descriptor->length = 1U;
        return true;
    }
    if (name != NULL && mylite_span_equal_ci(name->span, "ABS")) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(result_nullable);
        if (value != NULL && value->kind != MYLITE_EXPRESSION_VALUE_NULL) {
            *out_descriptor = mylite_expression_descriptor_from_value(value);
            mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
        }
        return true;
    }
    if (mylite_function_name_has_integer_result(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(result_nullable);
        out_descriptor->length = mylite_mysql_integer_function_display_length;
        return true;
    }
    if (name != NULL && mylite_span_equal_ci(name->span, "MOD")) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        return true;
    }
    if (name != NULL && mylite_span_equal_ci(name->span, "PI")) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_DOUBLE,
            .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_pi_function_display_length,
            .decimals = mylite_mysql_pi_function_scale,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = false,
        };
        return true;
    }
    return false;
}

int mylite_expression_descriptor_infer_numeric_variadic_function(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    bool result_nullable, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks)
{
    int status = MYLITE_OK;

    if (callbacks == NULL || callbacks->infer_expression_descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    status = infer_round_function_descriptor(database, plan, expression, value, result_nullable,
                                             out_descriptor, callbacks);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status =
        infer_format_function_descriptor(database, plan, expression, out_descriptor, callbacks);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    return infer_truncate_function_descriptor(database, plan, expression, value, result_nullable,
                                              out_descriptor, callbacks);
}

static bool infer_exp_function_descriptor(const struct mylite_sql_ast_node *name,
                                          struct mylite_field_descriptor *out_descriptor)
{
    if (!mylite_function_name_is_exp(name)) {
        return false;
    }

    *out_descriptor = double_function_descriptor(true);
    return true;
}

static bool infer_logarithm_function_descriptor(const struct mylite_sql_ast_node *name,
                                                struct mylite_field_descriptor *out_descriptor)
{
    if (!mylite_function_name_is_logarithm(name)) {
        return false;
    }

    *out_descriptor = double_function_descriptor(true);
    return true;
}

static bool infer_power_function_descriptor(const struct mylite_sql_ast_node *name,
                                            struct mylite_field_descriptor *out_descriptor)
{
    if (!mylite_function_name_is_power(name)) {
        return false;
    }

    *out_descriptor = double_function_descriptor(true);
    return true;
}

static bool infer_sqrt_function_descriptor(const struct mylite_sql_ast_node *name,
                                           struct mylite_field_descriptor *out_descriptor)
{
    if (!mylite_function_name_is_sqrt(name)) {
        return false;
    }

    *out_descriptor = double_function_descriptor(true);
    return true;
}

static bool infer_trigonometric_function_descriptor(const struct mylite_sql_ast_node *name,
                                                    struct mylite_field_descriptor *out_descriptor)
{
    if (!mylite_function_name_is_trigonometric(name)) {
        return false;
    }

    *out_descriptor = double_function_descriptor(true);
    return true;
}

static bool
infer_inverse_trigonometric_function_descriptor(const struct mylite_sql_ast_node *name,
                                                struct mylite_field_descriptor *out_descriptor)
{
    if (!mylite_function_name_is_inverse_trigonometric(name)) {
        return false;
    }

    *out_descriptor = double_function_descriptor(true);
    return true;
}

static bool
infer_angle_conversion_function_descriptor(const struct mylite_sql_ast_node *name,
                                           bool result_nullable,
                                           struct mylite_field_descriptor *out_descriptor)
{
    if (!mylite_function_name_is_angle_conversion(name)) {
        return false;
    }

    *out_descriptor = double_function_descriptor(result_nullable);
    return true;
}

static int infer_round_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    bool result_nullable, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks)
{
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
    status = callbacks->infer_expression_descriptor(database, plan, value_argument, NULL,
                                                    &value_descriptor);
    if (status != MYLITE_OK) {
        return status;
    }

    if (round_function_argument_is_approximate_literal(value_argument)) {
        *out_descriptor = double_function_descriptor(result_nullable);
        return MYLITE_OK;
    }

    if (value_descriptor.type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
        *out_descriptor = value_descriptor;
        out_descriptor->flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        if (round_function_constant_scale(scale_argument, &scale)) {
            int rounded_scale = round_function_descriptor_scale(scale);

            if (rounded_scale < 0) {
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
            *out_descriptor = double_function_descriptor(true);
        }
        return MYLITE_OK;
    }

    *out_descriptor = double_function_descriptor(result_nullable);
    return MYLITE_OK;
}

static int infer_format_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *value_argument = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor value_descriptor = mylite_expression_descriptor_defaults();
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);
    uint64_t character_length = 0U;
    int status = MYLITE_OK;

    if (!mylite_function_name_is_format(name)) {
        return MYLITE_UNSUPPORTED;
    }
    status = callbacks->infer_expression_descriptor(database, plan, value_argument, NULL,
                                                    &value_descriptor);
    if (status != MYLITE_OK) {
        return status;
    }

    character_length = format_function_result_character_length(value_argument, &value_descriptor);
    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .length = character_length * max_bytes_per_character,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return MYLITE_OK;
}

static uint64_t
format_function_result_character_length(const struct mylite_sql_ast_node *argument,
                                        const struct mylite_field_descriptor *argument_descriptor)
{
    uint64_t literal_length = format_literal_result_character_length(argument);

    if (literal_length != 0U) {
        return literal_length;
    }
    if (argument_descriptor == NULL || argument_descriptor->type == MYLITE_FIELD_TYPE_NULL) {
        return mylite_format_null_character_length;
    }
    switch (argument_descriptor->type) {
    case MYLITE_FIELD_TYPE_TINY:
    case MYLITE_FIELD_TYPE_SHORT:
    case MYLITE_FIELD_TYPE_LONG:
    case MYLITE_FIELD_TYPE_LONGLONG:
    case MYLITE_FIELD_TYPE_INT24:
    case MYLITE_FIELD_TYPE_YEAR:
    case MYLITE_FIELD_TYPE_NEWDECIMAL:
        return argument_descriptor->length + mylite_format_numeric_descriptor_extra_length;
    case MYLITE_FIELD_TYPE_FLOAT:
    case MYLITE_FIELD_TYPE_DOUBLE:
        return mylite_format_float_character_length;
    case MYLITE_FIELD_TYPE_STRING:
    case MYLITE_FIELD_TYPE_VAR_STRING:
    case MYLITE_FIELD_TYPE_VARCHAR:
    case MYLITE_FIELD_TYPE_TINY_BLOB:
    case MYLITE_FIELD_TYPE_MEDIUM_BLOB:
    case MYLITE_FIELD_TYPE_LONG_BLOB:
    case MYLITE_FIELD_TYPE_BLOB:
        return argument_descriptor->length + mylite_format_string_descriptor_extra_length;
    case MYLITE_FIELD_TYPE_DECIMAL:
    case MYLITE_FIELD_TYPE_NULL:
    case MYLITE_FIELD_TYPE_TIMESTAMP:
    case MYLITE_FIELD_TYPE_DATE:
    case MYLITE_FIELD_TYPE_TIME:
    case MYLITE_FIELD_TYPE_DATETIME:
    case MYLITE_FIELD_TYPE_BIT:
    case MYLITE_FIELD_TYPE_NEWDATE:
    case MYLITE_FIELD_TYPE_ENUM:
    case MYLITE_FIELD_TYPE_SET:
    case MYLITE_FIELD_TYPE_GEOMETRY:
    default:
        return mylite_format_string_descriptor_extra_length;
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t format_literal_result_character_length(const struct mylite_sql_ast_node *argument)
{
    uint64_t fraction_length = 0U;

    argument = mylite_sql_ast_unwrap_parenthesized_expression(argument);
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        uint64_t length = format_literal_result_character_length(mylite_ast_child_at(argument, 0U));

        return length == 0U ? 0U : length + 1U;
    }
    if (argument == NULL || argument->kind != MYLITE_SQL_AST_LITERAL) {
        return 0U;
    }
    switch (argument->literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        return mylite_format_null_character_length;
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return argument->span.length + mylite_format_literal_extra_length;
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
        fraction_length = format_literal_fraction_length(argument);
        return argument->span.length + mylite_format_decimal_literal_extra_length + fraction_length;
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        return mylite_format_float_character_length;
    case MYLITE_SQL_AST_LITERAL_STRING:
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        return argument->span.length >= 2U
                   ? argument->span.length - 2U + mylite_format_literal_extra_length
                   : mylite_format_literal_extra_length;
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
    case MYLITE_SQL_AST_LITERAL_NONE:
        break;
    }
    return 0U;
}

static uint64_t format_literal_fraction_length(const struct mylite_sql_ast_node *argument)
{
    const char *text = argument == NULL ? NULL : argument->span.text;
    const char *dot = text == NULL ? NULL : memchr(text, '.', argument->span.length);

    if (dot == NULL) {
        return 0U;
    }
    return (uint64_t)(argument->span.length - (size_t)(dot - text) - 1U);
}

static int infer_truncate_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    bool result_nullable, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks)
{
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
    status = callbacks->infer_expression_descriptor(database, plan, value_argument, NULL,
                                                    &value_descriptor);
    if (status != MYLITE_OK) {
        return status;
    }

    if (round_function_argument_is_approximate_literal(value_argument)) {
        *out_descriptor = double_function_descriptor(result_nullable);
        return MYLITE_OK;
    }

    if (value_descriptor.type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
        *out_descriptor = value_descriptor;
        out_descriptor->flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        if (round_function_constant_scale(scale_argument, &scale)) {
            truncate_decimal_descriptor_for_constant_scale(out_descriptor, &value_descriptor,
                                                           scale);
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
            *out_descriptor = double_function_descriptor(true);
        }
        return MYLITE_OK;
    }

    *out_descriptor = double_function_descriptor(result_nullable);
    return MYLITE_OK;
}

static bool
round_function_argument_is_approximate_literal(const struct mylite_sql_ast_node *argument)
{
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

static bool round_function_constant_scale(const struct mylite_sql_ast_node *argument,
                                          int *out_scale)
{
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

static int round_function_descriptor_scale(int scale)
{
    enum { round_scale_limit = 30 };

    if (scale > round_scale_limit) {
        return round_scale_limit;
    }
    if (scale < -round_scale_limit) {
        return -round_scale_limit;
    }
    return scale;
}

static void
truncate_decimal_descriptor_for_constant_scale(struct mylite_field_descriptor *descriptor,
                                               const struct mylite_field_descriptor *source,
                                               int scale)
{
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

static struct mylite_field_descriptor double_function_descriptor(bool nullable)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_DOUBLE,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = mylite_mysql_double_display_length + 1U,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = nullable,
    };

    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}
