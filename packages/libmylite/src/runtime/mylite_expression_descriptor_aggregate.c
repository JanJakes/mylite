#include "mylite_expression_descriptor_aggregate.h"

#include "mylite_connection.h"
#include "mylite_expression_descriptor.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stdbool.h>
#include <stdint.h>

static int infer_group_concat_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_aggregate_callbacks *callbacks
);

static int group_concat_argument_list_has_binary_result(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    const struct mylite_expression_descriptor_aggregate_callbacks *callbacks,
    bool *out_has_binary_result
);

static uint64_t group_concat_text_descriptor_length(
    uint64_t group_concat_max_len,
    uint64_t character_max_length
);

static struct mylite_field_descriptor sum_exact_numeric_descriptor(
    const struct mylite_field_descriptor *argument
);

static struct mylite_field_descriptor avg_exact_numeric_descriptor(
    const struct mylite_field_descriptor *argument
);

static uint64_t sum_exact_numeric_display_length(
    const struct mylite_field_descriptor *argument,
    unsigned int decimals
);

static uint64_t avg_exact_numeric_display_length(
    const struct mylite_field_descriptor *argument,
    unsigned int length_decimals
);

static uint64_t exact_numeric_integral_digits(const struct mylite_field_descriptor *descriptor);

static uint64_t decimal_descriptor_precision(const struct mylite_field_descriptor *descriptor);

static uint64_t saturating_add_u64(uint64_t left, uint64_t right);

static unsigned int saturating_add_uint(unsigned int left, unsigned int right);

static unsigned int min_uint(unsigned int left, unsigned int right);

static bool descriptor_has_exact_numeric_result(const struct mylite_field_descriptor *descriptor);

static bool descriptor_has_string_result(const struct mylite_field_descriptor *descriptor);

static void apply_min_max_aggregate_descriptor_flags(struct mylite_field_descriptor *descriptor);

static bool descriptor_is_binary_string(const struct mylite_field_descriptor *descriptor);

int mylite_expression_descriptor_infer_aggregate_expression(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_aggregate_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *argument = mylite_ast_child_at(expression, 1U);
    struct mylite_field_descriptor argument_descriptor = mylite_expression_descriptor_defaults();
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();

    if (callbacks == NULL || callbacks->infer_expression_descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    if (expression->aggregate_argument == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION) {
        int status =
            callbacks
                ->infer_expression_descriptor(database, plan, argument, NULL, &argument_descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
    }

    switch (expression->aggregate_kind) {
    case MYLITE_SQL_AST_AGGREGATE_COUNT:
        descriptor = mylite_expression_descriptor_signed_longlong(false);
        descriptor.length = mylite_mysql_signed_longlong_display_length;
        mylite_field_descriptor_set_not_null(&descriptor, true);
        *out_descriptor = descriptor;
        return MYLITE_OK;
    case MYLITE_SQL_AST_AGGREGATE_SUM:
        if (descriptor_has_exact_numeric_result(&argument_descriptor)) {
            descriptor = sum_exact_numeric_descriptor(&argument_descriptor);
        } else {
            descriptor = (struct mylite_field_descriptor){
                .type = MYLITE_FIELD_TYPE_DOUBLE,
                .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
                .length = mylite_mysql_double_display_length + 1U,
                .decimals = mylite_mysql_not_fixed_decimals,
                .charset_id = mylite_mysql_binary_charset_id,
                .nullable = true,
            };
            mylite_field_descriptor_set_nullable(&descriptor, true);
        }
        *out_descriptor = descriptor;
        return MYLITE_OK;
    case MYLITE_SQL_AST_AGGREGATE_AVG:
        if (!descriptor_has_exact_numeric_result(&argument_descriptor)) {
            descriptor = (struct mylite_field_descriptor){
                .type = MYLITE_FIELD_TYPE_DOUBLE,
                .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
                .length = mylite_mysql_double_display_length + 1U,
                .decimals = mylite_mysql_not_fixed_decimals,
                .charset_id = mylite_mysql_binary_charset_id,
                .nullable = true,
            };
            mylite_field_descriptor_set_nullable(&descriptor, true);
        } else {
            descriptor = avg_exact_numeric_descriptor(&argument_descriptor);
        }
        *out_descriptor = descriptor;
        return MYLITE_OK;
    case MYLITE_SQL_AST_AGGREGATE_MIN:
    case MYLITE_SQL_AST_AGGREGATE_MAX:
        descriptor = argument_descriptor;
        mylite_field_descriptor_set_nullable(&descriptor, true);
        apply_min_max_aggregate_descriptor_flags(&descriptor);
        *out_descriptor = descriptor;
        return MYLITE_OK;
    case MYLITE_SQL_AST_AGGREGATE_GROUP_CONCAT: {
        return infer_group_concat_descriptor(database, plan, expression, out_descriptor, callbacks);
    }
    case MYLITE_SQL_AST_AGGREGATE_NONE:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_defaults();
    return MYLITE_OK;
}

static int infer_group_concat_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_aggregate_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    uint64_t group_concat_max_len = mylite_connection_group_concat_max_len(database);
    uint64_t character_max_length =
        mylite_expression_descriptor_connection_character_max_length(database);
    bool has_binary_result = false;
    int status = group_concat_argument_list_has_binary_result(
        database,
        plan,
        arguments,
        callbacks,
        &has_binary_result
    );
    struct mylite_field_descriptor descriptor = {0};

    if (status != MYLITE_OK) {
        return status;
    }
    if (has_binary_result) {
        descriptor = (struct mylite_field_descriptor){
            .type = group_concat_max_len <= 512U ? MYLITE_FIELD_TYPE_VAR_STRING
                                                 : MYLITE_FIELD_TYPE_BLOB,
            .flags = MYLITE_FIELD_FLAG_BINARY,
            .length = group_concat_max_len,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };
    } else {
        descriptor = (struct mylite_field_descriptor){
            .type = group_concat_max_len <= 512U ? MYLITE_FIELD_TYPE_VAR_STRING
                                                 : MYLITE_FIELD_TYPE_BLOB,
            .flags = 0U,
            .length =
                group_concat_text_descriptor_length(group_concat_max_len, character_max_length),
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_collation_id(database),
            .nullable = true,
        };
    }
    mylite_field_descriptor_set_nullable(&descriptor, true);
    *out_descriptor = descriptor;
    return MYLITE_OK;
}

static uint64_t group_concat_text_descriptor_length(
    uint64_t group_concat_max_len,
    uint64_t character_max_length
) {
    enum { mysql_group_concat_long_text_length_factor = 16U };

    uint64_t multiplier = character_max_length;

    if (multiplier == 0U) {
        multiplier = 1U;
    }
    if (group_concat_max_len > 512U) {
        if (multiplier > UINT64_MAX / mysql_group_concat_long_text_length_factor) {
            return UINT64_MAX;
        }
        multiplier *= mysql_group_concat_long_text_length_factor;
    }
    return group_concat_max_len > UINT64_MAX / multiplier ? UINT64_MAX
                                                          : group_concat_max_len * multiplier;
}

static struct mylite_field_descriptor sum_exact_numeric_descriptor(
    const struct mylite_field_descriptor *argument
) {
    unsigned int decimals =
        argument->type == MYLITE_FIELD_TYPE_NEWDECIMAL ? argument->decimals : 0U;
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_decimal(true);

    descriptor.length = sum_exact_numeric_display_length(argument, decimals);
    descriptor.decimals = decimals;
    return descriptor;
}

static struct mylite_field_descriptor avg_exact_numeric_descriptor(
    const struct mylite_field_descriptor *argument
) {
    enum {
        mysql_avg_extra_scale = 4U,
        mysql_decimal_max_scale = 30U,
    };

    unsigned int argument_decimals =
        argument->type == MYLITE_FIELD_TYPE_NEWDECIMAL ? argument->decimals : 0U;
    unsigned int length_decimals = saturating_add_uint(argument_decimals, mysql_avg_extra_scale);
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_decimal(true);

    descriptor.length = avg_exact_numeric_display_length(argument, length_decimals);
    descriptor.decimals = min_uint(length_decimals, mysql_decimal_max_scale);
    return descriptor;
}

static uint64_t sum_exact_numeric_display_length(
    const struct mylite_field_descriptor *argument,
    unsigned int decimals
) {
    enum { mysql_sum_extra_integral_digits = 23U };

    uint64_t length = saturating_add_u64(
        exact_numeric_integral_digits(argument),
        mysql_sum_extra_integral_digits
    );

    if (decimals != 0U) {
        length = saturating_add_u64(length, (uint64_t)decimals + 1U);
    }
    return length;
}

static uint64_t avg_exact_numeric_display_length(
    const struct mylite_field_descriptor *argument,
    unsigned int length_decimals
) {
    uint64_t length = saturating_add_u64(exact_numeric_integral_digits(argument), 1U);

    length = saturating_add_u64(length, 1U);
    return saturating_add_u64(length, length_decimals);
}

static uint64_t exact_numeric_integral_digits(const struct mylite_field_descriptor *descriptor) {
    uint64_t precision = 0U;

    if (descriptor == NULL) {
        return 0U;
    }
    if (descriptor->type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
        precision = decimal_descriptor_precision(descriptor);
        if (precision < descriptor->decimals) {
            return 0U;
        }
        return precision - descriptor->decimals;
    }
    if ((descriptor->flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U) {
        return descriptor->length;
    }
    return descriptor->length == 0U ? 0U : descriptor->length - 1U;
}

static uint64_t decimal_descriptor_precision(const struct mylite_field_descriptor *descriptor) {
    uint64_t precision = descriptor == NULL ? 0U : descriptor->length;

    if (descriptor == NULL) {
        return 0U;
    }
    if ((descriptor->flags & MYLITE_FIELD_FLAG_UNSIGNED) == 0U && precision > 0U) {
        --precision;
    }
    if (descriptor->decimals != 0U && precision > 0U) {
        --precision;
    }
    return precision;
}

static uint64_t saturating_add_u64(uint64_t left, uint64_t right) {
    if (left > UINT64_MAX - right) {
        return UINT64_MAX;
    }
    return left + right;
}

static unsigned int saturating_add_uint(unsigned int left, unsigned int right) {
    if (left > UINT32_MAX - right) {
        return UINT32_MAX;
    }
    return left + right;
}

static unsigned int min_uint(unsigned int left, unsigned int right) {
    return left < right ? left : right;
}

static bool descriptor_has_exact_numeric_result(const struct mylite_field_descriptor *descriptor) {
    if (descriptor == NULL) {
        return false;
    }
    return (descriptor->flags & MYLITE_FIELD_FLAG_NUM) != 0U &&
           descriptor->type != MYLITE_FIELD_TYPE_FLOAT &&
           descriptor->type != MYLITE_FIELD_TYPE_DOUBLE;
}

static bool descriptor_has_string_result(const struct mylite_field_descriptor *descriptor) {
    if (descriptor == NULL || (descriptor->flags & MYLITE_FIELD_FLAG_NUM) != 0U) {
        return false;
    }
    switch (descriptor->type) {
    case MYLITE_FIELD_TYPE_VARCHAR:
    case MYLITE_FIELD_TYPE_TINY_BLOB:
    case MYLITE_FIELD_TYPE_MEDIUM_BLOB:
    case MYLITE_FIELD_TYPE_LONG_BLOB:
    case MYLITE_FIELD_TYPE_BLOB:
    case MYLITE_FIELD_TYPE_VAR_STRING:
    case MYLITE_FIELD_TYPE_STRING:
    case MYLITE_FIELD_TYPE_JSON:
        return true;
    case MYLITE_FIELD_TYPE_DECIMAL:
    case MYLITE_FIELD_TYPE_TINY:
    case MYLITE_FIELD_TYPE_SHORT:
    case MYLITE_FIELD_TYPE_LONG:
    case MYLITE_FIELD_TYPE_FLOAT:
    case MYLITE_FIELD_TYPE_DOUBLE:
    case MYLITE_FIELD_TYPE_NULL:
    case MYLITE_FIELD_TYPE_TIMESTAMP:
    case MYLITE_FIELD_TYPE_LONGLONG:
    case MYLITE_FIELD_TYPE_INT24:
    case MYLITE_FIELD_TYPE_DATE:
    case MYLITE_FIELD_TYPE_TIME:
    case MYLITE_FIELD_TYPE_DATETIME:
    case MYLITE_FIELD_TYPE_YEAR:
    case MYLITE_FIELD_TYPE_NEWDATE:
    case MYLITE_FIELD_TYPE_BIT:
    case MYLITE_FIELD_TYPE_NEWDECIMAL:
    case MYLITE_FIELD_TYPE_ENUM:
    case MYLITE_FIELD_TYPE_SET:
    case MYLITE_FIELD_TYPE_GEOMETRY:
    default:
        return false;
    }
}

static void apply_min_max_aggregate_descriptor_flags(struct mylite_field_descriptor *descriptor) {
    const unsigned int field_only_flags =
        MYLITE_FIELD_FLAG_PRI_KEY | MYLITE_FIELD_FLAG_UNIQUE_KEY | MYLITE_FIELD_FLAG_MULTIPLE_KEY |
        MYLITE_FIELD_FLAG_AUTO_INCREMENT | MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE |
        MYLITE_FIELD_FLAG_ON_UPDATE_NOW | MYLITE_FIELD_FLAG_PART_KEY;

    descriptor->flags &= ~field_only_flags;
    if ((descriptor->flags & MYLITE_FIELD_FLAG_NUM) != 0U) {
        descriptor->flags |= MYLITE_FIELD_FLAG_BINARY;
    }
    if (descriptor_has_string_result(descriptor)) {
        descriptor->decimals = mylite_mysql_not_fixed_decimals;
    }
}

static int group_concat_argument_list_has_binary_result(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    const struct mylite_expression_descriptor_aggregate_callbacks *callbacks,
    bool *out_has_binary_result
) {
    *out_has_binary_result = false;
    for (const struct mylite_sql_ast_node *argument = arguments == NULL ? NULL
                                                                        : arguments->first_child;
         argument != NULL;
         argument = argument->next_sibling) {
        struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
        int status =
            callbacks->infer_expression_descriptor(database, plan, argument, NULL, &descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
        if (descriptor_is_binary_string(&descriptor)) {
            *out_has_binary_result = true;
            return MYLITE_OK;
        }
    }
    return MYLITE_OK;
}

static bool descriptor_is_binary_string(const struct mylite_field_descriptor *descriptor) {
    if (descriptor == NULL || descriptor->type == MYLITE_FIELD_TYPE_NULL) {
        return false;
    }
    if ((descriptor->flags & MYLITE_FIELD_FLAG_NUM) != 0U) {
        return false;
    }
    return descriptor->charset_id == mylite_mysql_binary_charset_id;
}
