#include "mylite_field_descriptor.h"

#include "mylite_metadata_constants.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int field_descriptor_preserve_decimal_text(
    const struct mylite_field_descriptor *descriptor,
    struct mylite_expression_value *value
);
static enum mylite_expression_text_charset field_descriptor_expression_text_charset(
    const struct mylite_field_descriptor *descriptor
);
static bool field_descriptor_uses_binary_text_compare(
    const struct mylite_field_descriptor *descriptor
);
static bool field_descriptor_decimal_value_to_double(
    const struct mylite_expression_value *value,
    double *out_number
);
static char *field_descriptor_format_decimal_text(double value, unsigned int decimals);

void mylite_field_descriptor_set_nullable(
    struct mylite_field_descriptor *descriptor,
    bool nullable
) {
    if (nullable) {
        mylite_field_descriptor_set_not_null(descriptor, false);
    } else {
        mylite_field_descriptor_set_not_null(descriptor, true);
    }
}

void mylite_field_descriptor_set_not_null(
    struct mylite_field_descriptor *descriptor,
    bool not_null
) {
    if (descriptor == NULL) {
        return;
    }
    if (not_null) {
        descriptor->nullable = false;
        descriptor->flags |= MYLITE_FIELD_FLAG_NOT_NULL;
    } else {
        descriptor->nullable = true;
        descriptor->flags &= ~(unsigned int)MYLITE_FIELD_FLAG_NOT_NULL;
    }
}

int mylite_field_descriptor_apply_expression_value_metadata(
    const struct mylite_field_descriptor *descriptor,
    struct mylite_expression_value *value
) {
    if (value == NULL) {
        return 0;
    }
    value->preserve_temporal_fraction_digits =
        mylite_field_descriptor_preserves_temporal_fraction_digits(descriptor);
    value->temporal_type = mylite_field_descriptor_expression_temporal_type(descriptor);
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        value->text_charset = field_descriptor_expression_text_charset(descriptor);
    }
    return field_descriptor_preserve_decimal_text(descriptor, value);
}

bool mylite_field_descriptor_preserves_temporal_fraction_digits(
    const struct mylite_field_descriptor *descriptor
) {
    if (descriptor == NULL) {
        return false;
    }
    if (descriptor->type == MYLITE_FIELD_TYPE_TIME ||
        descriptor->type == MYLITE_FIELD_TYPE_DATETIME) {
        return true;
    }
    return descriptor->type == MYLITE_FIELD_TYPE_TIMESTAMP;
}

enum mylite_expression_temporal_type mylite_field_descriptor_expression_temporal_type(
    const struct mylite_field_descriptor *descriptor
) {
    if (descriptor == NULL) {
        return MYLITE_EXPRESSION_TEMPORAL_NONE;
    }
    switch (descriptor->type) {
    case MYLITE_FIELD_TYPE_DATE:
        return MYLITE_EXPRESSION_TEMPORAL_DATE;
    case MYLITE_FIELD_TYPE_TIME:
        return MYLITE_EXPRESSION_TEMPORAL_TIME;
    case MYLITE_FIELD_TYPE_DATETIME:
        return MYLITE_EXPRESSION_TEMPORAL_DATETIME;
    case MYLITE_FIELD_TYPE_TIMESTAMP:
        return MYLITE_EXPRESSION_TEMPORAL_TIMESTAMP;
    case MYLITE_FIELD_TYPE_DECIMAL:
    case MYLITE_FIELD_TYPE_TINY:
    case MYLITE_FIELD_TYPE_SHORT:
    case MYLITE_FIELD_TYPE_LONG:
    case MYLITE_FIELD_TYPE_FLOAT:
    case MYLITE_FIELD_TYPE_DOUBLE:
    case MYLITE_FIELD_TYPE_NULL:
    case MYLITE_FIELD_TYPE_LONGLONG:
    case MYLITE_FIELD_TYPE_INT24:
    case MYLITE_FIELD_TYPE_YEAR:
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
        return MYLITE_EXPRESSION_TEMPORAL_NONE;
    }
}

static int field_descriptor_preserve_decimal_text(
    const struct mylite_field_descriptor *descriptor,
    struct mylite_expression_value *value
) {
    double number = 0.0;
    char *text = NULL;

    if (descriptor == NULL || value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL ||
        (descriptor->type != MYLITE_FIELD_TYPE_DECIMAL &&
         descriptor->type != MYLITE_FIELD_TYPE_NEWDECIMAL)) {
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return 0;
    }
    if (!field_descriptor_decimal_value_to_double(value, &number)) {
        return 0;
    }
    text = field_descriptor_format_decimal_text(number, descriptor->decimals);
    if (text == NULL) {
        return -1;
    }
    free(value->text_value);
    value->text_value = text;
    value->text_length = strlen(text);
    value->preserve_real_text = true;
    value->compact_real_text = false;
    return 0;
}

static enum mylite_expression_text_charset field_descriptor_expression_text_charset(
    const struct mylite_field_descriptor *descriptor
) {
    if (descriptor == NULL) {
        return MYLITE_EXPRESSION_TEXT_CHARSET_UNKNOWN;
    }
    switch (descriptor->type) {
    case MYLITE_FIELD_TYPE_STRING:
    case MYLITE_FIELD_TYPE_VAR_STRING:
    case MYLITE_FIELD_TYPE_BLOB:
    case MYLITE_FIELD_TYPE_JSON:
        if (field_descriptor_uses_binary_text_compare(descriptor)) {
            return MYLITE_EXPRESSION_TEXT_CHARSET_BINARY;
        }
        if (descriptor->charset_id == mylite_mysql_latin1_swedish_ci_charset_id) {
            return MYLITE_EXPRESSION_TEXT_CHARSET_LATIN1;
        }
        if (descriptor->charset_id == mylite_mysql_utf8mb4_0900_ai_ci_charset_id) {
            return MYLITE_EXPRESSION_TEXT_CHARSET_UTF8MB4;
        }
        return MYLITE_EXPRESSION_TEXT_CHARSET_UNKNOWN;
    default:
        return MYLITE_EXPRESSION_TEXT_CHARSET_UNKNOWN;
    }
}

static bool field_descriptor_uses_binary_text_compare(
    const struct mylite_field_descriptor *descriptor
) {
    return descriptor != NULL && (descriptor->charset_id == mylite_mysql_binary_charset_id ||
                                  descriptor->charset_id == mylite_mysql_utf8mb4_bin_charset_id ||
                                  (descriptor->flags & MYLITE_FIELD_FLAG_BINARY) != 0U);
}

static bool field_descriptor_decimal_value_to_double(
    const struct mylite_expression_value *value,
    double *out_number
) {
    char *end = NULL;
    double number = 0.0;
    bool has_number = false;

    if (value == NULL || out_number == NULL) {
        return false;
    }
    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_INT64:
        *out_number = (double)value->int64_value;
        return true;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        *out_number = (double)value->uint64_value;
        return true;
    case MYLITE_EXPRESSION_VALUE_REAL:
        *out_number = value->real_value;
        return true;
    case MYLITE_EXPRESSION_VALUE_TEXT:
        if (value->text_value == NULL) {
            return false;
        }
        number = strtod(value->text_value, &end);
        has_number = end != value->text_value;
        if (!has_number) {
            return false;
        }
        *out_number = number;
        return true;
    case MYLITE_EXPRESSION_VALUE_NULL:
        return false;
    }
    return false;
}

static char *field_descriptor_format_decimal_text(double value, unsigned int decimals) {
    int length = snprintf(NULL, 0U, "%.*f", (int)decimals, value);
    char *text = NULL;
    int written = 0;

    if (length < 0) {
        return NULL;
    }
    text = malloc((size_t)length + 1U);
    if (text == NULL) {
        return NULL;
    }
    written = snprintf(text, (size_t)length + 1U, "%.*f", (int)decimals, value);
    if (written != length) {
        free(text);
        return NULL;
    }
    return text;
}
