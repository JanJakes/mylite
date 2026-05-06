#include "mylite_field_descriptor.h"

#include <mylite/mylite.h>

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
