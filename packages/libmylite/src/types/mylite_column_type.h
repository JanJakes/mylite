#ifndef MYLITE_TYPES_MYLITE_COLUMN_TYPE_H
#define MYLITE_TYPES_MYLITE_COLUMN_TYPE_H

#include <stdbool.h>
#include <stddef.h>

enum mylite_column_type_status {
    MYLITE_COLUMN_TYPE_OK = 0,
    MYLITE_COLUMN_TYPE_UNKNOWN = 1,
    MYLITE_COLUMN_TYPE_INVALID_SYNTAX = 2,
    MYLITE_COLUMN_TYPE_DISPLAY_WIDTH_OUT_OF_RANGE = 3,
};

enum mylite_column_integer_type {
    MYLITE_COLUMN_INTEGER_NONE = 0,
    MYLITE_COLUMN_INTEGER_TINYINT = 1,
    MYLITE_COLUMN_INTEGER_SMALLINT = 2,
    MYLITE_COLUMN_INTEGER_MEDIUMINT = 3,
    MYLITE_COLUMN_INTEGER_INT = 4,
    MYLITE_COLUMN_INTEGER_BIGINT = 5,
};

enum {
    MYLITE_COLUMN_TYPE_TEXT_CAPACITY = 32,
};

struct mylite_column_type_attributes {
    bool has_display_width;
    unsigned int display_width;
    bool has_signed;
    bool has_unsigned;
};

struct mylite_column_type_descriptor {
    enum mylite_column_integer_type integer_type;
    bool is_unsigned;
    bool is_boolean_alias;
    bool has_display_width;
    unsigned int display_width;
    unsigned int storage_bytes;
    unsigned int numeric_precision;
    unsigned int numeric_scale;
    const char *canonical_type_name;
    const char *data_type;
    const char *signed_min;
    const char *signed_max;
    const char *unsigned_min;
    const char *unsigned_max;
    const char *range_min;
    const char *range_max;
    char column_type[MYLITE_COLUMN_TYPE_TEXT_CAPACITY];
};

enum mylite_column_type_status
mylite_column_type_describe_integer(const char *type_name, size_t type_name_length,
                                    struct mylite_column_type_attributes attributes,
                                    struct mylite_column_type_descriptor *out_descriptor);

const char *mylite_column_type_status_name(enum mylite_column_type_status status);

#endif
