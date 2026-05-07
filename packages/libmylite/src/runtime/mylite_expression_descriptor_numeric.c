#include "mylite_expression_descriptor_numeric.h"

#include "mylite_expression_descriptor.h"
#include "mylite_function_names.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stddef.h>
#include <stdint.h>

static bool infer_exp_function_descriptor(
    const struct mylite_sql_ast_node *name,
    struct mylite_field_descriptor *out_descriptor
);

static bool infer_logarithm_function_descriptor(
    const struct mylite_sql_ast_node *name,
    struct mylite_field_descriptor *out_descriptor
);

static bool infer_power_function_descriptor(
    const struct mylite_sql_ast_node *name,
    struct mylite_field_descriptor *out_descriptor
);

static bool infer_sqrt_function_descriptor(
    const struct mylite_sql_ast_node *name,
    struct mylite_field_descriptor *out_descriptor
);

static bool infer_trigonometric_function_descriptor(
    const struct mylite_sql_ast_node *name,
    struct mylite_field_descriptor *out_descriptor
);

static bool infer_inverse_trigonometric_function_descriptor(
    const struct mylite_sql_ast_node *name,
    struct mylite_field_descriptor *out_descriptor
);

static bool infer_angle_conversion_function_descriptor(
    const struct mylite_sql_ast_node *name,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor
);

static bool function_name_is_abs(const struct mylite_sql_ast_node *name);

static bool function_name_is_floor_or_ceil(const struct mylite_sql_ast_node *name);

static bool function_name_is_mod(const struct mylite_sql_ast_node *name);

static bool function_name_is_sign(const struct mylite_sql_ast_node *name);

static int infer_unix_timestamp_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks
);

static struct mylite_field_descriptor unix_timestamp_argument_descriptor(
    const struct mylite_sql_ast_node *argument,
    const struct mylite_field_descriptor *argument_descriptor,
    bool result_nullable
);

static struct mylite_field_descriptor unix_timestamp_integer_descriptor(bool result_nullable);

static struct mylite_field_descriptor unix_timestamp_decimal_descriptor(
    unsigned int decimals,
    bool result_nullable
);

static unsigned int unix_timestamp_text_literal_decimals(
    const struct mylite_sql_ast_node *argument
);

static unsigned int unix_timestamp_fraction_digits_after_dot(
    const char *text,
    size_t length,
    bool *out_found_dot
);

static bool unix_timestamp_text_starts_temporal(const char *text, size_t length);

static bool unix_timestamp_argument_is_approximate(
    const struct mylite_sql_ast_node *argument,
    const struct mylite_field_descriptor *argument_descriptor
);

static int infer_first_argument_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks
);

static struct mylite_field_descriptor abs_function_descriptor(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    const struct mylite_field_descriptor *argument,
    bool result_nullable
);

static struct mylite_field_descriptor floor_ceil_function_descriptor(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_field_descriptor *argument,
    bool result_nullable
);

static struct mylite_field_descriptor mod_function_descriptor(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_field_descriptor *argument
);

static bool numeric_function_first_argument_is_approximate_literal(
    const struct mylite_sql_ast_node *expression
);

static bool field_descriptor_has_integer_type(const struct mylite_field_descriptor *descriptor);

static struct mylite_field_descriptor integer_argument_descriptor(
    const struct mylite_field_descriptor *argument,
    bool nullable
);

static struct mylite_field_descriptor double_descriptor_with_shape(
    bool nullable,
    uint64_t length,
    unsigned int decimals
);

bool mylite_expression_descriptor_infer_fixed_integer_function(
    const struct mylite_sql_ast_node *name,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor
) {
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
    const struct mylite_sql_ast_node *name,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor
) {
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

int mylite_expression_descriptor_infer_scalar_numeric_function(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    struct mylite_field_descriptor argument = mylite_expression_descriptor_defaults();
    int status = MYLITE_OK;

    if (name != NULL && mylite_span_equal_ci(name->span, "ISNULL")) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(false);
        out_descriptor->length = 1U;
        return MYLITE_OK;
    }
    if (function_name_is_sign(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(result_nullable);
        out_descriptor->length = mylite_mysql_integer_function_display_length;
        return MYLITE_OK;
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
        return MYLITE_OK;
    }
    if (name != NULL && mylite_span_equal_ci(name->span, "UNIX_TIMESTAMP")) {
        return infer_unix_timestamp_function_descriptor(
            database,
            plan,
            expression,
            result_nullable,
            out_descriptor,
            callbacks
        );
    }
    if (name != NULL && mylite_span_equal_ci(name->span, "RAND")) {
        *out_descriptor = mylite_expression_descriptor_numeric_double_function(result_nullable);
        return MYLITE_OK;
    }
    if (!function_name_is_abs(name) && !function_name_is_floor_or_ceil(name) &&
        !function_name_is_mod(name)) {
        return MYLITE_UNSUPPORTED;
    }
    status = infer_first_argument_descriptor(database, plan, expression, &argument, callbacks);
    if (status != MYLITE_OK) {
        return status;
    }
    if (function_name_is_abs(name)) {
        *out_descriptor = abs_function_descriptor(expression, value, &argument, result_nullable);
        return MYLITE_OK;
    }
    if (function_name_is_floor_or_ceil(name)) {
        *out_descriptor = floor_ceil_function_descriptor(expression, &argument, result_nullable);
        return MYLITE_OK;
    }
    *out_descriptor = mod_function_descriptor(expression, &argument);
    return MYLITE_OK;
}

static int infer_unix_timestamp_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    size_t argument_count = arguments == NULL ? 0U : mylite_sql_ast_node_child_count(arguments);
    const struct mylite_sql_ast_node *argument = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor argument_descriptor = mylite_expression_descriptor_defaults();
    int status = MYLITE_OK;

    if (argument_count == 0U) {
        *out_descriptor = unix_timestamp_integer_descriptor(false);
        return MYLITE_OK;
    }
    if (argument_count != 1U) {
        return MYLITE_UNSUPPORTED;
    }
    status = infer_first_argument_descriptor(
        database,
        plan,
        expression,
        &argument_descriptor,
        callbacks
    );
    if (status != MYLITE_OK) {
        return status;
    }
    *out_descriptor =
        unix_timestamp_argument_descriptor(argument, &argument_descriptor, result_nullable);
    return MYLITE_OK;
}

static struct mylite_field_descriptor unix_timestamp_argument_descriptor(
    const struct mylite_sql_ast_node *argument,
    const struct mylite_field_descriptor *argument_descriptor,
    bool result_nullable
) {
    unsigned int decimals = 0U;

    if (argument_descriptor == NULL || argument_descriptor->type == MYLITE_FIELD_TYPE_NULL) {
        return unix_timestamp_integer_descriptor(result_nullable);
    }
    switch (argument_descriptor->type) {
    case MYLITE_FIELD_TYPE_TIME:
    case MYLITE_FIELD_TYPE_DATETIME:
    case MYLITE_FIELD_TYPE_TIMESTAMP:
    case MYLITE_FIELD_TYPE_NEWDECIMAL:
        decimals = argument_descriptor->decimals > mylite_mysql_temporal_max_fsp
                       ? mylite_mysql_temporal_max_fsp
                       : argument_descriptor->decimals;
        return decimals == 0U ? unix_timestamp_integer_descriptor(result_nullable)
                              : unix_timestamp_decimal_descriptor(decimals, result_nullable);
    case MYLITE_FIELD_TYPE_DATE:
    case MYLITE_FIELD_TYPE_TINY:
    case MYLITE_FIELD_TYPE_SHORT:
    case MYLITE_FIELD_TYPE_LONG:
    case MYLITE_FIELD_TYPE_LONGLONG:
    case MYLITE_FIELD_TYPE_INT24:
    case MYLITE_FIELD_TYPE_YEAR:
        return unix_timestamp_integer_descriptor(result_nullable);
    default:
        break;
    }
    if (unix_timestamp_argument_is_approximate(argument, argument_descriptor)) {
        return unix_timestamp_decimal_descriptor(mylite_mysql_temporal_max_fsp, result_nullable);
    }
    if (mylite_expression_descriptor_has_text_result(argument_descriptor) ||
        argument_descriptor->type == MYLITE_FIELD_TYPE_BLOB ||
        argument_descriptor->type == MYLITE_FIELD_TYPE_TINY_BLOB ||
        argument_descriptor->type == MYLITE_FIELD_TYPE_MEDIUM_BLOB ||
        argument_descriptor->type == MYLITE_FIELD_TYPE_LONG_BLOB ||
        argument_descriptor->type == MYLITE_FIELD_TYPE_JSON) {
        decimals = unix_timestamp_text_literal_decimals(argument);
        return decimals == 0U ? unix_timestamp_integer_descriptor(result_nullable)
                              : unix_timestamp_decimal_descriptor(decimals, result_nullable);
    }
    return unix_timestamp_integer_descriptor(result_nullable);
}

static struct mylite_field_descriptor unix_timestamp_integer_descriptor(bool result_nullable) {
    struct mylite_field_descriptor descriptor =
        mylite_expression_descriptor_signed_longlong(result_nullable);

    descriptor.length = mylite_mysql_signed_longlong_display_length;
    return descriptor;
}

static struct mylite_field_descriptor unix_timestamp_decimal_descriptor(
    unsigned int decimals,
    bool result_nullable
) {
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_NEWDECIMAL,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = 13U + decimals,
        .decimals = decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = result_nullable,
    };

    mylite_field_descriptor_set_nullable(&descriptor, result_nullable);
    return descriptor;
}

static unsigned int unix_timestamp_text_literal_decimals(
    const struct mylite_sql_ast_node *argument
) {
    const char *text = NULL;
    size_t length = 0U;
    size_t start = 0U;
    size_t end = 0U;
    bool found_dot = false;
    unsigned int decimals = 0U;

    argument = mylite_sql_ast_unwrap_parenthesized_expression(argument);
    if (argument == NULL || argument->kind != MYLITE_SQL_AST_LITERAL ||
        (argument->literal_kind != MYLITE_SQL_AST_LITERAL_STRING &&
         argument->literal_kind != MYLITE_SQL_AST_LITERAL_NATIONAL_STRING)) {
        return mylite_mysql_temporal_max_fsp;
    }
    text = argument->span.text;
    length = argument->span.length;
    if (text == NULL || length == 0U) {
        return mylite_mysql_temporal_max_fsp;
    }
    start =
        (length >= 3U && (text[0] == 'N' || text[0] == 'n') && (text[1] == '\'' || text[1] == '"'))
            ? 2U
            : 1U;
    end = length > start ? length - 1U : start;
    decimals = unix_timestamp_fraction_digits_after_dot(text + start, end - start, &found_dot);
    if (found_dot) {
        return decimals == 0U ? mylite_mysql_temporal_max_fsp : decimals;
    }
    return unix_timestamp_text_starts_temporal(text + start, end - start)
               ? 0U
               : mylite_mysql_temporal_max_fsp;
}

static unsigned int unix_timestamp_fraction_digits_after_dot(
    const char *text,
    size_t length,
    bool *out_found_dot
) {
    bool in_fraction = false;
    unsigned int digits = 0U;

    *out_found_dot = false;
    for (size_t index = 0U; index < length; ++index) {
        if (text[index] == '.') {
            in_fraction = true;
            *out_found_dot = true;
            continue;
        }
        if (!in_fraction) {
            continue;
        }
        if (text[index] < '0' || text[index] > '9') {
            break;
        }
        if (digits < mylite_mysql_temporal_max_fsp) {
            ++digits;
        }
    }
    return digits;
}

static bool unix_timestamp_text_starts_temporal(const char *text, size_t length) {
    for (size_t index = 0U; index < length; ++index) {
        if (text[index] == ' ' || text[index] == '\t' || text[index] == '\n' ||
            text[index] == '\r') {
            continue;
        }
        return text[index] >= '0' && text[index] <= '9';
    }
    return false;
}

static bool unix_timestamp_argument_is_approximate(
    const struct mylite_sql_ast_node *argument,
    const struct mylite_field_descriptor *argument_descriptor
) {
    argument = mylite_sql_ast_unwrap_parenthesized_expression(argument);
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        argument =
            mylite_sql_ast_unwrap_parenthesized_expression(mylite_ast_child_at(argument, 0U));
    }
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_LITERAL &&
        argument->literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT) {
        return true;
    }
    if (argument_descriptor == NULL) {
        return false;
    }
    return argument_descriptor->type == MYLITE_FIELD_TYPE_FLOAT ||
           argument_descriptor->type == MYLITE_FIELD_TYPE_DOUBLE;
}

static bool function_name_is_abs(const struct mylite_sql_ast_node *name) {
    return name != NULL && mylite_span_equal_ci(name->span, "ABS");
}

static bool function_name_is_floor_or_ceil(const struct mylite_sql_ast_node *name) {
    return name != NULL &&
           (mylite_span_equal_ci(name->span, "FLOOR") || mylite_span_equal_ci(name->span, "CEIL") ||
            mylite_span_equal_ci(name->span, "CEILING"));
}

static bool function_name_is_mod(const struct mylite_sql_ast_node *name) {
    return name != NULL && mylite_span_equal_ci(name->span, "MOD");
}

static bool function_name_is_sign(const struct mylite_sql_ast_node *name) {
    return name != NULL && mylite_span_equal_ci(name->span, "SIGN");
}

static int infer_first_argument_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *argument = mylite_ast_child_at(arguments, 0U);

    if (callbacks == NULL || callbacks->infer_expression_descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    return callbacks->infer_expression_descriptor(database, plan, argument, NULL, out_descriptor);
}

static struct mylite_field_descriptor abs_function_descriptor(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    const struct mylite_field_descriptor *argument,
    bool result_nullable
) {
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();

    if (numeric_function_first_argument_is_approximate_literal(expression)) {
        return mylite_expression_descriptor_numeric_double_function(result_nullable);
    }
    if (argument == NULL || argument->type == MYLITE_FIELD_TYPE_NULL) {
        return double_descriptor_with_shape(true, 17U, 0U);
    }
    if (field_descriptor_has_integer_type(argument)) {
        descriptor = integer_argument_descriptor(argument, result_nullable);
        descriptor.length = argument->length;
        return descriptor;
    }
    if (argument->type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
        descriptor = *argument;
        descriptor.flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        mylite_field_descriptor_set_nullable(&descriptor, result_nullable);
        return descriptor;
    }
    if (argument->type == MYLITE_FIELD_TYPE_DOUBLE || argument->type == MYLITE_FIELD_TYPE_FLOAT ||
        mylite_expression_descriptor_has_text_result(argument) ||
        argument->type == MYLITE_FIELD_TYPE_BLOB || argument->type == MYLITE_FIELD_TYPE_TINY_BLOB ||
        argument->type == MYLITE_FIELD_TYPE_MEDIUM_BLOB ||
        argument->type == MYLITE_FIELD_TYPE_LONG_BLOB || argument->type == MYLITE_FIELD_TYPE_JSON) {
        return mylite_expression_descriptor_numeric_double_function(result_nullable);
    }
    descriptor = mylite_expression_descriptor_from_value(value);
    if (descriptor.type == MYLITE_FIELD_TYPE_NULL || descriptor.type == MYLITE_FIELD_TYPE_INVALID) {
        return mylite_expression_descriptor_numeric_double_function(result_nullable);
    }
    mylite_field_descriptor_set_nullable(&descriptor, result_nullable);
    return descriptor;
}

static struct mylite_field_descriptor floor_ceil_function_descriptor(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_field_descriptor *argument,
    bool result_nullable
) {
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();

    if (numeric_function_first_argument_is_approximate_literal(expression)) {
        return mylite_expression_descriptor_numeric_double_function(result_nullable);
    }
    if (argument == NULL || argument->type == MYLITE_FIELD_TYPE_NULL) {
        return mylite_expression_descriptor_numeric_double_function(true);
    }
    if (field_descriptor_has_integer_type(argument) ||
        argument->type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
        descriptor = mylite_expression_descriptor_signed_longlong(result_nullable);
        descriptor.length = mylite_mysql_integer_function_display_length;
        if ((argument->flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U) {
            descriptor.flags |= MYLITE_FIELD_FLAG_UNSIGNED;
        }
        return descriptor;
    }
    return mylite_expression_descriptor_numeric_double_function(result_nullable);
}

static struct mylite_field_descriptor mod_function_descriptor(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_field_descriptor *argument
) {
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();

    if (numeric_function_first_argument_is_approximate_literal(expression)) {
        return mylite_expression_descriptor_numeric_double_function(true);
    }
    if (argument == NULL || argument->type == MYLITE_FIELD_TYPE_NULL) {
        return double_descriptor_with_shape(true, 2U, 0U);
    }
    if (field_descriptor_has_integer_type(argument)) {
        descriptor = integer_argument_descriptor(argument, true);
        if ((argument->flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U) {
            descriptor.length = argument->length == UINT64_MAX ? UINT64_MAX : argument->length + 1U;
        }
        return descriptor;
    }
    if (argument->type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
        descriptor = *argument;
        descriptor.flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        mylite_field_descriptor_set_nullable(&descriptor, true);
        return descriptor;
    }
    return mylite_expression_descriptor_numeric_double_function(true);
}

static bool numeric_function_first_argument_is_approximate_literal(
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *argument = mylite_ast_child_at(arguments, 0U);

    argument = mylite_sql_ast_unwrap_parenthesized_expression(argument);
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        argument =
            mylite_sql_ast_unwrap_parenthesized_expression(mylite_ast_child_at(argument, 0U));
    }
    return argument != NULL && argument->kind == MYLITE_SQL_AST_LITERAL &&
           argument->literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT;
}

static bool field_descriptor_has_integer_type(const struct mylite_field_descriptor *descriptor) {
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
    default:
        return false;
    }
}

static struct mylite_field_descriptor integer_argument_descriptor(
    const struct mylite_field_descriptor *argument,
    bool nullable
) {
    struct mylite_field_descriptor descriptor =
        mylite_expression_descriptor_signed_longlong(nullable);

    descriptor.length = argument == NULL ? descriptor.length : argument->length;
    if (argument != NULL && (argument->flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U) {
        descriptor.flags |= MYLITE_FIELD_FLAG_UNSIGNED;
    }
    return descriptor;
}

static struct mylite_field_descriptor double_descriptor_with_shape(
    bool nullable,
    uint64_t length,
    unsigned int decimals
) {
    struct mylite_field_descriptor descriptor =
        mylite_expression_descriptor_numeric_double_function(nullable);

    descriptor.length = length;
    descriptor.decimals = decimals;
    return descriptor;
}

static bool infer_exp_function_descriptor(
    const struct mylite_sql_ast_node *name,
    struct mylite_field_descriptor *out_descriptor
) {
    if (!mylite_function_name_is_exp(name)) {
        return false;
    }

    *out_descriptor = mylite_expression_descriptor_numeric_double_function(true);
    return true;
}

static bool infer_logarithm_function_descriptor(
    const struct mylite_sql_ast_node *name,
    struct mylite_field_descriptor *out_descriptor
) {
    if (!mylite_function_name_is_logarithm(name)) {
        return false;
    }

    *out_descriptor = mylite_expression_descriptor_numeric_double_function(true);
    return true;
}

static bool infer_power_function_descriptor(
    const struct mylite_sql_ast_node *name,
    struct mylite_field_descriptor *out_descriptor
) {
    if (!mylite_function_name_is_power(name)) {
        return false;
    }

    *out_descriptor = mylite_expression_descriptor_numeric_double_function(true);
    return true;
}

static bool infer_sqrt_function_descriptor(
    const struct mylite_sql_ast_node *name,
    struct mylite_field_descriptor *out_descriptor
) {
    if (!mylite_function_name_is_sqrt(name)) {
        return false;
    }

    *out_descriptor = mylite_expression_descriptor_numeric_double_function(true);
    return true;
}

static bool infer_trigonometric_function_descriptor(
    const struct mylite_sql_ast_node *name,
    struct mylite_field_descriptor *out_descriptor
) {
    if (!mylite_function_name_is_trigonometric(name)) {
        return false;
    }

    *out_descriptor = mylite_expression_descriptor_numeric_double_function(true);
    return true;
}

static bool infer_inverse_trigonometric_function_descriptor(
    const struct mylite_sql_ast_node *name,
    struct mylite_field_descriptor *out_descriptor
) {
    if (!mylite_function_name_is_inverse_trigonometric(name)) {
        return false;
    }

    *out_descriptor = mylite_expression_descriptor_numeric_double_function(true);
    return true;
}

static bool infer_angle_conversion_function_descriptor(
    const struct mylite_sql_ast_node *name,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor
) {
    if (!mylite_function_name_is_angle_conversion(name)) {
        return false;
    }

    *out_descriptor = mylite_expression_descriptor_numeric_double_function(result_nullable);
    return true;
}

struct mylite_field_descriptor mylite_expression_descriptor_numeric_double_function(bool nullable) {
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
