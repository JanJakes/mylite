#include "mylite_expression_descriptor_numeric.h"

#include "mylite_expression_descriptor.h"
#include "mylite_function_names.h"
#include "mylite_metadata_constants.h"

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
