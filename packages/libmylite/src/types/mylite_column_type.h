#ifndef MYLITE_TYPES_MYLITE_COLUMN_TYPE_H
#define MYLITE_TYPES_MYLITE_COLUMN_TYPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum mylite_column_type_status {
    MYLITE_COLUMN_TYPE_OK = 0,
    MYLITE_COLUMN_TYPE_UNKNOWN = 1,
    MYLITE_COLUMN_TYPE_INVALID_SYNTAX = 2,
    MYLITE_COLUMN_TYPE_DISPLAY_WIDTH_OUT_OF_RANGE = 3,
    MYLITE_COLUMN_TYPE_LENGTH_OUT_OF_RANGE = 4,
    MYLITE_COLUMN_TYPE_UNKNOWN_CHARACTER_SET = 5,
    MYLITE_COLUMN_TYPE_UNKNOWN_COLLATION = 6,
    MYLITE_COLUMN_TYPE_COLLATION_CHARACTER_SET_MISMATCH = 7,
    MYLITE_COLUMN_TYPE_PRECISION_OUT_OF_RANGE = 8,
    MYLITE_COLUMN_TYPE_SCALE_OUT_OF_RANGE = 9,
    MYLITE_COLUMN_TYPE_SCALE_EXCEEDS_PRECISION = 10,
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
    MYLITE_COLUMN_TYPE_TEXT_CAPACITY = 64,
};

enum mylite_column_string_binary_type {
    MYLITE_COLUMN_STRING_BINARY_NONE = 0,
    MYLITE_COLUMN_STRING_BINARY_CHAR = 1,
    MYLITE_COLUMN_STRING_BINARY_VARCHAR = 2,
    MYLITE_COLUMN_STRING_BINARY_TINYTEXT = 3,
    MYLITE_COLUMN_STRING_BINARY_TEXT = 4,
    MYLITE_COLUMN_STRING_BINARY_MEDIUMTEXT = 5,
    MYLITE_COLUMN_STRING_BINARY_LONGTEXT = 6,
    MYLITE_COLUMN_STRING_BINARY_BINARY = 7,
    MYLITE_COLUMN_STRING_BINARY_VARBINARY = 8,
    MYLITE_COLUMN_STRING_BINARY_TINYBLOB = 9,
    MYLITE_COLUMN_STRING_BINARY_BLOB = 10,
    MYLITE_COLUMN_STRING_BINARY_MEDIUMBLOB = 11,
    MYLITE_COLUMN_STRING_BINARY_LONGBLOB = 12,
};

enum mylite_column_numeric_type {
    MYLITE_COLUMN_NUMERIC_NONE = 0,
    MYLITE_COLUMN_NUMERIC_DECIMAL = 1,
    MYLITE_COLUMN_NUMERIC_FLOAT = 2,
    MYLITE_COLUMN_NUMERIC_DOUBLE = 3,
};

enum mylite_column_temporal_type {
    MYLITE_COLUMN_TEMPORAL_NONE = 0,
    MYLITE_COLUMN_TEMPORAL_DATE = 1,
    MYLITE_COLUMN_TEMPORAL_TIME = 2,
    MYLITE_COLUMN_TEMPORAL_DATETIME = 3,
    MYLITE_COLUMN_TEMPORAL_TIMESTAMP = 4,
    MYLITE_COLUMN_TEMPORAL_YEAR = 5,
};

struct mylite_column_type_attributes {
    uint64_t length;
    uint64_t precision;
    uint64_t scale;
    const char *character_set;
    size_t character_set_length;
    const char *collation;
    size_t collation_length;
    unsigned int display_width;
    bool has_display_width;
    bool has_signed;
    bool has_unsigned;
    bool has_length;
    bool has_precision;
    bool has_scale;
    bool has_character_set;
    bool has_collation;
    bool has_binary_attribute;
    bool has_byte_attribute;
    bool has_zerofill_attribute;
    bool is_national;
};

struct mylite_column_type_descriptor {
    enum mylite_column_integer_type integer_type;
    enum mylite_column_string_binary_type string_binary_type;
    enum mylite_column_numeric_type numeric_type;
    enum mylite_column_temporal_type temporal_type;
    bool is_unsigned;
    bool is_boolean_alias;
    bool is_binary_string;
    bool is_character_string;
    bool is_exact_numeric;
    bool is_approximate_numeric;
    bool is_deprecated_binary_attribute;
    bool is_alias;
    bool is_zerofill;
    bool has_display_width;
    unsigned int display_width;
    unsigned int storage_bytes;
    unsigned int numeric_precision;
    bool has_numeric_scale;
    unsigned int numeric_scale;
    bool has_datetime_precision;
    unsigned int datetime_precision;
    uint64_t character_maximum_length;
    uint64_t character_octet_length;
    const char *canonical_type_name;
    const char *data_type;
    const char *character_set_name;
    const char *collation_name;
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

enum mylite_column_type_status
mylite_column_type_describe_string_binary(const char *type_name, size_t type_name_length,
                                          struct mylite_column_type_attributes attributes,
                                          struct mylite_column_type_descriptor *out_descriptor);

enum mylite_column_type_status
mylite_column_type_describe_numeric(const char *type_name, size_t type_name_length,
                                    struct mylite_column_type_attributes attributes,
                                    struct mylite_column_type_descriptor *out_descriptor);

enum mylite_column_type_status
mylite_column_type_describe_temporal(const char *type_name, size_t type_name_length,
                                     struct mylite_column_type_attributes attributes,
                                     struct mylite_column_type_descriptor *out_descriptor);

const char *mylite_column_type_status_name(enum mylite_column_type_status status);

#endif
