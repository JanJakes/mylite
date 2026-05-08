#include "mylite_expression_descriptor.h"

#include "mylite_metadata_constants.h"

static bool union_field_descriptor_has_text_result(
    const struct mylite_field_descriptor *descriptor
);

static bool union_field_descriptor_uses_binary_text(
    const struct mylite_field_descriptor *descriptor
);

static bool union_field_descriptor_has_decimal_result(
    const struct mylite_field_descriptor *descriptor
);

static bool union_field_descriptor_has_double_result(
    const struct mylite_field_descriptor *descriptor
);

static bool union_field_descriptors_have_mixed_integer_signedness(
    const struct mylite_field_descriptor *left,
    const struct mylite_field_descriptor *right
);

static struct mylite_field_descriptor union_field_descriptor_mixed_integer_result(
    const struct mylite_field_descriptor *left,
    const struct mylite_field_descriptor *right,
    bool nullable
);

static bool union_field_descriptor_has_integer_result(
    const struct mylite_field_descriptor *descriptor
);

static bool union_field_descriptor_has_unsigned_flag(
    const struct mylite_field_descriptor *descriptor
);

static bool union_field_descriptors_require_decimal_for_mixed_integer(
    const struct mylite_field_descriptor *left,
    const struct mylite_field_descriptor *right
);

static uint64_t union_field_descriptor_increment_length(uint64_t length);

void mylite_expression_descriptor_merge_union_operand(
    const mylite_db *database,
    struct mylite_field_descriptor *descriptor,
    const struct mylite_field_descriptor *operand
) {
    bool nullable = false;

    if (descriptor == NULL || operand == NULL) {
        return;
    }

    nullable = descriptor->nullable;
    if (!nullable) {
        nullable = operand->nullable;
    }
    if (descriptor->type == MYLITE_FIELD_TYPE_NULL) {
        *descriptor = *operand;
        mylite_field_descriptor_set_nullable(descriptor, nullable);
        return;
    }
    if (operand->type == MYLITE_FIELD_TYPE_NULL) {
        mylite_field_descriptor_set_nullable(descriptor, nullable);
        return;
    }

    if (union_field_descriptor_has_text_result(descriptor) ||
        union_field_descriptor_has_text_result(operand)) {
        bool binary_text = union_field_descriptor_uses_binary_text(descriptor);

        if (!binary_text) {
            binary_text = union_field_descriptor_uses_binary_text(operand);
        }

        unsigned int text_flags = 0U;

        if (binary_text) {
            text_flags = MYLITE_FIELD_FLAG_BINARY;
        }

        *descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = text_flags,
            .length = mylite_expression_descriptor_max_u64(descriptor->length, operand->length),
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_charset_id(database),
            .nullable = nullable,
        };
    } else if (
        union_field_descriptor_has_double_result(descriptor) ||
        union_field_descriptor_has_double_result(operand)
    ) {
        *descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_DOUBLE,
            .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_expression_descriptor_max_u64(
                mylite_expression_descriptor_max_u64(descriptor->length, operand->length),
                mylite_mysql_double_display_length
            ),
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = nullable,
        };
    } else if (
        union_field_descriptor_has_decimal_result(descriptor) ||
        union_field_descriptor_has_decimal_result(operand)
    ) {
        *descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_NEWDECIMAL,
            .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_expression_descriptor_max_u64(descriptor->length, operand->length),
            .decimals = (unsigned int)
                mylite_expression_descriptor_max_u64(descriptor->decimals, operand->decimals),
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = nullable,
        };
    } else if (union_field_descriptors_have_mixed_integer_signedness(descriptor, operand)) {
        *descriptor = union_field_descriptor_mixed_integer_result(descriptor, operand, nullable);
    } else {
        descriptor->length =
            mylite_expression_descriptor_max_u64(descriptor->length, operand->length);
        descriptor->decimals = (unsigned int)
            mylite_expression_descriptor_max_u64(descriptor->decimals, operand->decimals);
        descriptor->flags |= operand->flags & MYLITE_FIELD_FLAG_UNSIGNED;
        descriptor->flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        descriptor->charset_id = mylite_mysql_binary_charset_id;
    }

    mylite_field_descriptor_set_nullable(descriptor, nullable);
}

static bool union_field_descriptor_has_text_result(
    const struct mylite_field_descriptor *descriptor
) {
    if (descriptor == NULL) {
        return false;
    }
    switch (descriptor->type) {
    case MYLITE_FIELD_TYPE_STRING:
    case MYLITE_FIELD_TYPE_VAR_STRING:
    case MYLITE_FIELD_TYPE_BLOB:
    case MYLITE_FIELD_TYPE_JSON:
        return (descriptor->flags & MYLITE_FIELD_FLAG_NUM) == 0U;
    default:
        return mylite_expression_descriptor_has_text_result(descriptor);
    }
}

static bool union_field_descriptor_uses_binary_text(
    const struct mylite_field_descriptor *descriptor
) {
    if (descriptor == NULL) {
        return false;
    }
    switch (descriptor->type) {
    case MYLITE_FIELD_TYPE_STRING:
    case MYLITE_FIELD_TYPE_VAR_STRING:
    case MYLITE_FIELD_TYPE_BLOB:
    case MYLITE_FIELD_TYPE_JSON:
        return (descriptor->charset_id == mylite_mysql_binary_charset_id ||
                descriptor->charset_id == mylite_mysql_utf8mb4_bin_charset_id ||
                (descriptor->flags & MYLITE_FIELD_FLAG_BINARY) != 0U) != false;
    default:
        return false;
    }
}

static bool union_field_descriptor_has_decimal_result(
    const struct mylite_field_descriptor *descriptor
) {
    return mylite_expression_descriptor_has_decimal_result(descriptor);
}

static bool union_field_descriptor_has_double_result(
    const struct mylite_field_descriptor *descriptor
) {
    return mylite_expression_descriptor_has_double_result(descriptor);
}

static bool union_field_descriptors_have_mixed_integer_signedness(
    const struct mylite_field_descriptor *left,
    const struct mylite_field_descriptor *right
) {
    return union_field_descriptor_has_integer_result(left) &&
           union_field_descriptor_has_integer_result(right) &&
           union_field_descriptor_has_unsigned_flag(left) !=
               union_field_descriptor_has_unsigned_flag(right);
}

static struct mylite_field_descriptor union_field_descriptor_mixed_integer_result(
    const struct mylite_field_descriptor *left,
    const struct mylite_field_descriptor *right,
    bool nullable
) {
    uint64_t length = mylite_expression_descriptor_max_u64(left->length, right->length);
    enum mylite_field_type type = MYLITE_FIELD_TYPE_LONGLONG;
    struct mylite_field_descriptor descriptor;

    if (union_field_descriptors_require_decimal_for_mixed_integer(left, right)) {
        type = MYLITE_FIELD_TYPE_NEWDECIMAL;
        length = union_field_descriptor_increment_length(length);
    }

    descriptor = (struct mylite_field_descriptor){
        .type = type,
        .flags = MYLITE_FIELD_FLAG_NUM,
        .length = length,
        .decimals = 0U,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = nullable,
    };
    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}

static bool union_field_descriptor_has_integer_result(
    const struct mylite_field_descriptor *descriptor
) {
    if (descriptor == NULL) {
        return false;
    }
    switch (descriptor->type) {
    case MYLITE_FIELD_TYPE_TINY:
    case MYLITE_FIELD_TYPE_SHORT:
    case MYLITE_FIELD_TYPE_LONG:
    case MYLITE_FIELD_TYPE_LONGLONG:
    case MYLITE_FIELD_TYPE_INT24:
        return true;
    default:
        return false;
    }
}

static bool union_field_descriptor_has_unsigned_flag(
    const struct mylite_field_descriptor *descriptor
) {
    return descriptor != NULL && (descriptor->flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U;
}

static bool union_field_descriptors_require_decimal_for_mixed_integer(
    const struct mylite_field_descriptor *left,
    const struct mylite_field_descriptor *right
) {
    const struct mylite_field_descriptor *unsigned_descriptor =
        union_field_descriptor_has_unsigned_flag(left) ? left : right;

    return unsigned_descriptor->length >= mylite_mysql_unsigned_longlong_display_length;
}

static uint64_t union_field_descriptor_increment_length(uint64_t length) {
    if (length == UINT64_MAX) {
        return length;
    }
    return length + 1U;
}
