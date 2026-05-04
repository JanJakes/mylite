#ifndef MYLITE_RUNTIME_MYLITE_FIELD_DESCRIPTOR_H
#define MYLITE_RUNTIME_MYLITE_FIELD_DESCRIPTOR_H

#include <stdbool.h>
#include <stdint.h>

enum mylite_format_metadata_length {
    mylite_format_null_character_length = 32,
    mylite_format_decimal_literal_extra_length = 33,
    mylite_format_literal_extra_length = 34,
    mylite_format_numeric_descriptor_extra_length = 35,
    mylite_format_string_descriptor_extra_length = 42,
    mylite_format_float_character_length = 61,
};

struct mylite_field_descriptor {
    int type;
    unsigned int flags;
    uint64_t length;
    uint64_t max_length;
    unsigned int decimals;
    unsigned int charset_id;
    bool nullable;
};

#endif
