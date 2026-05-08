#include "mylite_column_type.h"

#include <stdio.h>
#include <string.h>

struct mylite_integer_type_info {
    const char *canonical_type_name;
    enum mylite_column_integer_type integer_type;
    unsigned int storage_bytes;
    unsigned int signed_precision;
    unsigned int unsigned_precision;
    const char *signed_min;
    const char *signed_max;
    const char *unsigned_min;
    const char *unsigned_max;
};

struct mylite_integer_alias {
    const char *name;
    enum mylite_column_integer_type integer_type;
    bool is_boolean_alias;
};

struct mylite_string_binary_alias {
    const char *name;
    enum mylite_column_string_binary_type type;
    bool is_alias;
};

struct mylite_charset_info {
    const char *name;
    const char *default_collation;
    const char *binary_collation;
    unsigned int max_bytes_per_character;
    bool is_binary;
};

struct mylite_collation_info {
    const char *name;
    const char *character_set_name;
};

struct mylite_string_binary_column_type_format {
    enum mylite_column_string_binary_type type;
    uint64_t length;
};

struct mylite_numeric_alias {
    const char *name;
    enum mylite_column_numeric_type type;
    bool is_alias;
};

struct mylite_numeric_format {
    enum mylite_column_numeric_type type;
    unsigned int precision;
    unsigned int scale;
    bool has_scale;
    bool is_unsigned;
    bool is_zerofill;
};

struct mylite_temporal_type_info {
    const char *canonical_type_name;
    enum mylite_column_temporal_type type;
    unsigned int base_storage_bytes;
    bool permits_fractional_seconds;
    bool has_datetime_precision;
    const char *range_min;
    const char *range_max;
};

struct mylite_precision_scale {
    uint64_t precision;
    uint64_t scale;
};

struct mylite_precision_scale_limits {
    uint64_t precision_max;
    uint64_t scale_max;
};

enum {
    display_width_max = 255U,
    bit_precision_default = 1U,
    bit_precision_max = 64U,
    decimal_precision_max = 65U,
    decimal_scale_max = 30U,
    float_binary_precision_max = 53U,
    float_binary_precision_cutover = 24U,
    approximate_display_width_max = 255U,
    approximate_scale_max = 30U,
    temporal_fractional_seconds_precision_max = 6U,
    year_display_width = 4U,
};

static const uint64_t decimal_default_precision = 10ULL;
static const uint64_t float_default_precision = 12ULL;
static const uint64_t double_default_precision = 22ULL;
static const uint64_t char_binary_length_max = 255ULL;
static const uint64_t varbinary_length_max = 65535ULL;
static const uint64_t blob_text_length_max = 4294967295ULL;
static const uint64_t tiny_capacity = 255ULL;
static const uint64_t regular_capacity = 65535ULL;
static const uint64_t medium_capacity = 16777215ULL;
static const uint64_t long_capacity = 4294967295ULL;

static const struct mylite_integer_type_info *lookup_integer_type_info(
    enum mylite_column_integer_type integer_type
);

static bool lookup_integer_alias(
    const char *type_name,
    size_t type_name_length,
    enum mylite_column_integer_type *out_integer_type,
    bool *out_is_boolean_alias
);

static bool lookup_string_binary_alias(
    const char *type_name,
    size_t type_name_length,
    enum mylite_column_string_binary_type *out_type,
    bool *out_is_alias
);

static bool lookup_numeric_alias(
    const char *type_name,
    size_t type_name_length,
    enum mylite_column_numeric_type *out_type,
    bool *out_is_alias
);

static const struct mylite_temporal_type_info *lookup_temporal_type_info(
    const char *type_name,
    size_t type_name_length
);

static const struct mylite_charset_info *effective_charset(
    struct mylite_column_type_attributes attributes,
    enum mylite_column_type_status *out_status
);

static const struct mylite_charset_info *lookup_charset(const char *name, size_t name_length);

static const struct mylite_collation_info *lookup_collation(const char *name, size_t name_length);

static enum mylite_column_type_status check_collation_matches_charset(
    const struct mylite_charset_info *charset,
    const struct mylite_collation_info *collation
);

static bool ascii_case_equal(const char *left, size_t left_length, const char *right);

static char ascii_lower(unsigned char byte);

static void fill_integer_descriptor(
    const struct mylite_integer_type_info *info,
    bool is_unsigned,
    bool is_boolean_alias,
    struct mylite_column_type_attributes attributes,
    struct mylite_column_type_descriptor *out_descriptor
);

static void format_integer_column_type(
    const struct mylite_integer_type_info *info,
    bool is_unsigned,
    bool is_boolean_alias,
    struct mylite_column_type_attributes attributes,
    char *out_column_type,
    size_t column_type_size
);

static void fill_bit_descriptor(
    unsigned int precision,
    struct mylite_column_type_descriptor *out_descriptor
);

static enum mylite_column_type_status describe_decimal(
    struct mylite_column_type_attributes attributes,
    bool is_alias,
    struct mylite_column_type_descriptor *out_descriptor
);

static enum mylite_column_type_status describe_float(
    struct mylite_column_type_attributes attributes,
    bool is_alias,
    struct mylite_column_type_descriptor *out_descriptor
);

static enum mylite_column_type_status describe_double(
    enum mylite_column_numeric_type requested_type,
    struct mylite_column_type_attributes attributes,
    bool is_alias,
    struct mylite_column_type_descriptor *out_descriptor
);

static enum mylite_column_type_status check_precision_scale(
    struct mylite_precision_scale value,
    struct mylite_precision_scale_limits limits
);

static bool numeric_attributes_are_unsigned(struct mylite_column_type_attributes attributes);

static bool float_descriptor_is_alias(
    bool is_alias,
    enum mylite_column_numeric_type effective_type
);

static bool numeric_type_is_approximate(enum mylite_column_numeric_type type);

static void fill_numeric_descriptor(
    struct mylite_numeric_format format,
    bool is_alias,
    struct mylite_column_type_descriptor *out_descriptor
);

static const char *name_for_numeric_type(enum mylite_column_numeric_type type);

static void format_numeric_column_type(
    struct mylite_numeric_format format,
    char *out_column_type,
    size_t column_type_size
);

static void fill_temporal_descriptor(
    const struct mylite_temporal_type_info *info,
    struct mylite_column_type_attributes attributes,
    struct mylite_column_type_descriptor *out_descriptor
);

static unsigned int fractional_storage_bytes(unsigned int precision);

static void format_temporal_column_type(
    const struct mylite_temporal_type_info *info,
    struct mylite_column_type_attributes attributes,
    char *out_column_type,
    size_t column_type_size
);

static enum mylite_column_type_status describe_char_varchar(
    enum mylite_column_string_binary_type requested_type,
    struct mylite_column_type_attributes attributes,
    struct mylite_column_type_descriptor *out_descriptor
);

static enum mylite_column_type_status describe_text_blob(
    enum mylite_column_string_binary_type requested_type,
    struct mylite_column_type_attributes attributes,
    struct mylite_column_type_descriptor *out_descriptor
);

static enum mylite_column_type_status describe_binary_varbinary(
    enum mylite_column_string_binary_type requested_type,
    struct mylite_column_type_attributes attributes,
    struct mylite_column_type_descriptor *out_descriptor
);

static void fill_string_descriptor(
    enum mylite_column_string_binary_type type,
    uint64_t length,
    const struct mylite_charset_info *charset,
    const struct mylite_collation_info *collation,
    bool is_deprecated_binary_attribute,
    bool is_alias,
    struct mylite_column_type_descriptor *out_descriptor
);

static void fill_binary_descriptor(
    enum mylite_column_string_binary_type type,
    uint64_t length,
    bool is_alias,
    struct mylite_column_type_descriptor *out_descriptor
);

static enum mylite_column_string_binary_type text_family_for_capacity(uint64_t capacity);

static enum mylite_column_string_binary_type blob_family_for_capacity(uint64_t capacity);

static uint64_t capacity_for_string_binary_type(enum mylite_column_string_binary_type type);

static const char *name_for_string_binary_type(enum mylite_column_string_binary_type type);

static void format_string_binary_column_type(
    struct mylite_string_binary_column_type_format format,
    char *out_column_type,
    size_t column_type_size
);

enum mylite_column_type_status mylite_column_type_describe_integer(
    const char *type_name,
    size_t type_name_length,
    struct mylite_column_type_attributes attributes,
    struct mylite_column_type_descriptor *out_descriptor
) {
    const struct mylite_integer_type_info *info = NULL;
    enum mylite_column_integer_type integer_type = MYLITE_COLUMN_INTEGER_NONE;
    bool is_boolean_alias = false;
    bool is_unsigned = false;

    if (out_descriptor == NULL) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    *out_descriptor = (struct mylite_column_type_descriptor){0};

    if (type_name == NULL ||
        !lookup_integer_alias(type_name, type_name_length, &integer_type, &is_boolean_alias)) {
        return MYLITE_COLUMN_TYPE_UNKNOWN;
    }
    if (is_boolean_alias &&
        (attributes.has_display_width || attributes.has_signed || attributes.has_unsigned)) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    if (attributes.has_display_width && attributes.display_width > display_width_max) {
        return MYLITE_COLUMN_TYPE_DISPLAY_WIDTH_OUT_OF_RANGE;
    }

    info = lookup_integer_type_info(integer_type);
    if (info == NULL) {
        return MYLITE_COLUMN_TYPE_UNKNOWN;
    }

    is_unsigned = attributes.has_unsigned;
    fill_integer_descriptor(info, is_unsigned, is_boolean_alias, attributes, out_descriptor);
    return MYLITE_COLUMN_TYPE_OK;
}

enum mylite_column_type_status mylite_column_type_describe_bit(
    const char *type_name,
    size_t type_name_length,
    struct mylite_column_type_attributes attributes,
    struct mylite_column_type_descriptor *out_descriptor
) {
    uint64_t precision = bit_precision_default;

    if (out_descriptor == NULL) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    *out_descriptor = (struct mylite_column_type_descriptor){0};

    if (type_name == NULL || !ascii_case_equal(type_name, type_name_length, "BIT")) {
        return MYLITE_COLUMN_TYPE_UNKNOWN;
    }
    if (attributes.has_display_width || attributes.has_length || attributes.has_scale ||
        attributes.has_signed || attributes.has_unsigned || attributes.has_character_set ||
        attributes.has_collation || attributes.has_binary_attribute ||
        attributes.has_byte_attribute || attributes.has_zerofill_attribute ||
        attributes.is_national) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    if (attributes.has_precision) {
        precision = attributes.precision;
    }
    if (precision == 0ULL || precision > bit_precision_max) {
        return MYLITE_COLUMN_TYPE_PRECISION_OUT_OF_RANGE;
    }

    fill_bit_descriptor((unsigned int)precision, out_descriptor);
    return MYLITE_COLUMN_TYPE_OK;
}

enum mylite_column_type_status mylite_column_type_describe_string_binary(
    const char *type_name,
    size_t type_name_length,
    struct mylite_column_type_attributes attributes,
    struct mylite_column_type_descriptor *out_descriptor
) {
    enum mylite_column_string_binary_type requested_type = MYLITE_COLUMN_STRING_BINARY_NONE;
    bool is_alias = false;

    if (out_descriptor == NULL) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    *out_descriptor = (struct mylite_column_type_descriptor){0};

    if (type_name == NULL ||
        !lookup_string_binary_alias(type_name, type_name_length, &requested_type, &is_alias)) {
        return MYLITE_COLUMN_TYPE_UNKNOWN;
    }

    if (ascii_case_equal(type_name, type_name_length, "NCHAR") ||
        ascii_case_equal(type_name, type_name_length, "NATIONAL CHAR") ||
        ascii_case_equal(type_name, type_name_length, "NVARCHAR") ||
        ascii_case_equal(type_name, type_name_length, "NATIONAL VARCHAR")) {
        attributes.is_national = true;
    }

    switch (requested_type) {
    case MYLITE_COLUMN_STRING_BINARY_CHAR:
    case MYLITE_COLUMN_STRING_BINARY_VARCHAR:
        return describe_char_varchar(requested_type, attributes, out_descriptor);
    case MYLITE_COLUMN_STRING_BINARY_TINYTEXT:
    case MYLITE_COLUMN_STRING_BINARY_TEXT:
    case MYLITE_COLUMN_STRING_BINARY_MEDIUMTEXT:
    case MYLITE_COLUMN_STRING_BINARY_LONGTEXT:
        return describe_text_blob(requested_type, attributes, out_descriptor);
    case MYLITE_COLUMN_STRING_BINARY_BINARY:
    case MYLITE_COLUMN_STRING_BINARY_VARBINARY:
        return describe_binary_varbinary(requested_type, attributes, out_descriptor);
    case MYLITE_COLUMN_STRING_BINARY_TINYBLOB:
    case MYLITE_COLUMN_STRING_BINARY_BLOB:
    case MYLITE_COLUMN_STRING_BINARY_MEDIUMBLOB:
    case MYLITE_COLUMN_STRING_BINARY_LONGBLOB:
        return describe_text_blob(requested_type, attributes, out_descriptor);
    case MYLITE_COLUMN_STRING_BINARY_NONE:
        break;
    }

    return MYLITE_COLUMN_TYPE_UNKNOWN;
}

enum mylite_column_type_status mylite_column_type_describe_numeric(
    const char *type_name,
    size_t type_name_length,
    struct mylite_column_type_attributes attributes,
    struct mylite_column_type_descriptor *out_descriptor
) {
    enum mylite_column_numeric_type requested_type = MYLITE_COLUMN_NUMERIC_NONE;
    bool is_alias = false;

    if (out_descriptor == NULL) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    *out_descriptor = (struct mylite_column_type_descriptor){0};

    if (type_name == NULL ||
        !lookup_numeric_alias(type_name, type_name_length, &requested_type, &is_alias)) {
        return MYLITE_COLUMN_TYPE_UNKNOWN;
    }

    switch (requested_type) {
    case MYLITE_COLUMN_NUMERIC_DECIMAL:
        return describe_decimal(attributes, is_alias, out_descriptor);
    case MYLITE_COLUMN_NUMERIC_FLOAT:
        return describe_float(attributes, is_alias, out_descriptor);
    case MYLITE_COLUMN_NUMERIC_DOUBLE:
        return describe_double(requested_type, attributes, is_alias, out_descriptor);
    case MYLITE_COLUMN_NUMERIC_NONE:
        break;
    }

    return MYLITE_COLUMN_TYPE_UNKNOWN;
}

enum mylite_column_type_status mylite_column_type_describe_temporal(
    const char *type_name,
    size_t type_name_length,
    struct mylite_column_type_attributes attributes,
    struct mylite_column_type_descriptor *out_descriptor
) {
    const struct mylite_temporal_type_info *info = NULL;

    if (out_descriptor == NULL) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    *out_descriptor = (struct mylite_column_type_descriptor){0};

    info = lookup_temporal_type_info(type_name, type_name_length);
    if (info == NULL) {
        return MYLITE_COLUMN_TYPE_UNKNOWN;
    }

    if (attributes.has_display_width || attributes.has_length || attributes.has_scale ||
        attributes.has_signed || attributes.has_unsigned || attributes.has_character_set ||
        attributes.has_collation || attributes.has_binary_attribute ||
        attributes.has_byte_attribute || attributes.has_zerofill_attribute ||
        attributes.is_national) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    if (info->type == MYLITE_COLUMN_TEMPORAL_DATE && attributes.has_precision) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    if (info->type == MYLITE_COLUMN_TEMPORAL_YEAR && attributes.has_precision &&
        attributes.precision != year_display_width) {
        return MYLITE_COLUMN_TYPE_DISPLAY_WIDTH_OUT_OF_RANGE;
    }
    if (info->permits_fractional_seconds && attributes.has_precision &&
        attributes.precision > temporal_fractional_seconds_precision_max) {
        return MYLITE_COLUMN_TYPE_PRECISION_OUT_OF_RANGE;
    }

    fill_temporal_descriptor(info, attributes, out_descriptor);
    return MYLITE_COLUMN_TYPE_OK;
}

const char *mylite_column_type_status_name(enum mylite_column_type_status status) {
    switch (status) {
    case MYLITE_COLUMN_TYPE_OK:
        return "ok";
    case MYLITE_COLUMN_TYPE_UNKNOWN:
        return "unknown";
    case MYLITE_COLUMN_TYPE_INVALID_SYNTAX:
        return "invalid_syntax";
    case MYLITE_COLUMN_TYPE_DISPLAY_WIDTH_OUT_OF_RANGE:
        return "display_width_out_of_range";
    case MYLITE_COLUMN_TYPE_LENGTH_OUT_OF_RANGE:
        return "length_out_of_range";
    case MYLITE_COLUMN_TYPE_UNKNOWN_CHARACTER_SET:
        return "unknown_character_set";
    case MYLITE_COLUMN_TYPE_UNKNOWN_COLLATION:
        return "unknown_collation";
    case MYLITE_COLUMN_TYPE_COLLATION_CHARACTER_SET_MISMATCH:
        return "collation_character_set_mismatch";
    case MYLITE_COLUMN_TYPE_PRECISION_OUT_OF_RANGE:
        return "precision_out_of_range";
    case MYLITE_COLUMN_TYPE_SCALE_OUT_OF_RANGE:
        return "scale_out_of_range";
    case MYLITE_COLUMN_TYPE_SCALE_EXCEEDS_PRECISION:
        return "scale_exceeds_precision";
    }

    return "unknown";
}

static const struct mylite_integer_type_info *lookup_integer_type_info(
    enum mylite_column_integer_type integer_type
) {
    static const struct mylite_integer_type_info types[] = {
        {
            .canonical_type_name = "tinyint",
            .integer_type = MYLITE_COLUMN_INTEGER_TINYINT,
            .storage_bytes = 1U,
            .signed_precision = 3U,
            .unsigned_precision = 3U,
            .signed_min = "-128",
            .signed_max = "127",
            .unsigned_min = "0",
            .unsigned_max = "255",
        },
        {
            .canonical_type_name = "smallint",
            .integer_type = MYLITE_COLUMN_INTEGER_SMALLINT,
            .storage_bytes = 2U,
            .signed_precision = 5U,
            .unsigned_precision = 5U,
            .signed_min = "-32768",
            .signed_max = "32767",
            .unsigned_min = "0",
            .unsigned_max = "65535",
        },
        {
            .canonical_type_name = "mediumint",
            .integer_type = MYLITE_COLUMN_INTEGER_MEDIUMINT,
            .storage_bytes = 3U,
            .signed_precision = 7U,
            .unsigned_precision = 7U,
            .signed_min = "-8388608",
            .signed_max = "8388607",
            .unsigned_min = "0",
            .unsigned_max = "16777215",
        },
        {
            .canonical_type_name = "int",
            .integer_type = MYLITE_COLUMN_INTEGER_INT,
            .storage_bytes = 4U,
            .signed_precision = 10U,
            .unsigned_precision = 10U,
            .signed_min = "-2147483648",
            .signed_max = "2147483647",
            .unsigned_min = "0",
            .unsigned_max = "4294967295",
        },
        {
            .canonical_type_name = "bigint",
            .integer_type = MYLITE_COLUMN_INTEGER_BIGINT,
            .storage_bytes = 8U,
            .signed_precision = 19U,
            .unsigned_precision = 20U,
            .signed_min = "-9223372036854775808",
            .signed_max = "9223372036854775807",
            .unsigned_min = "0",
            .unsigned_max = "18446744073709551615",
        },
    };

    for (size_t index = 0U; index < sizeof(types) / sizeof(types[0]); ++index) {
        if (types[index].integer_type == integer_type) {
            return &types[index];
        }
    }
    return NULL;
}

static bool lookup_integer_alias(
    const char *type_name,
    size_t type_name_length,
    enum mylite_column_integer_type *out_integer_type,
    bool *out_is_boolean_alias
) {
    static const struct mylite_integer_alias aliases[] = {
        {"TINYINT", MYLITE_COLUMN_INTEGER_TINYINT, false},
        {"INT1", MYLITE_COLUMN_INTEGER_TINYINT, false},
        {"SMALLINT", MYLITE_COLUMN_INTEGER_SMALLINT, false},
        {"INT2", MYLITE_COLUMN_INTEGER_SMALLINT, false},
        {"MEDIUMINT", MYLITE_COLUMN_INTEGER_MEDIUMINT, false},
        {"MIDDLEINT", MYLITE_COLUMN_INTEGER_MEDIUMINT, false},
        {"INT3", MYLITE_COLUMN_INTEGER_MEDIUMINT, false},
        {"INT", MYLITE_COLUMN_INTEGER_INT, false},
        {"INTEGER", MYLITE_COLUMN_INTEGER_INT, false},
        {"INT4", MYLITE_COLUMN_INTEGER_INT, false},
        {"BIGINT", MYLITE_COLUMN_INTEGER_BIGINT, false},
        {"INT8", MYLITE_COLUMN_INTEGER_BIGINT, false},
        {"BOOL", MYLITE_COLUMN_INTEGER_TINYINT, true},
        {"BOOLEAN", MYLITE_COLUMN_INTEGER_TINYINT, true},
    };

    if (out_integer_type == NULL || out_is_boolean_alias == NULL) {
        return false;
    }

    for (size_t index = 0U; index < sizeof(aliases) / sizeof(aliases[0]); ++index) {
        if (ascii_case_equal(type_name, type_name_length, aliases[index].name)) {
            *out_integer_type = aliases[index].integer_type;
            *out_is_boolean_alias = aliases[index].is_boolean_alias;
            return true;
        }
    }
    return false;
}

static bool lookup_string_binary_alias(
    const char *type_name,
    size_t type_name_length,
    enum mylite_column_string_binary_type *out_type,
    bool *out_is_alias
) {
    static const struct mylite_string_binary_alias aliases[] = {
        {"CHAR", MYLITE_COLUMN_STRING_BINARY_CHAR, false},
        {"CHARACTER", MYLITE_COLUMN_STRING_BINARY_CHAR, false},
        {"NCHAR", MYLITE_COLUMN_STRING_BINARY_CHAR, true},
        {"NATIONAL CHAR", MYLITE_COLUMN_STRING_BINARY_CHAR, true},
        {"VARCHAR", MYLITE_COLUMN_STRING_BINARY_VARCHAR, false},
        {"CHAR VARYING", MYLITE_COLUMN_STRING_BINARY_VARCHAR, true},
        {"CHARACTER VARYING", MYLITE_COLUMN_STRING_BINARY_VARCHAR, true},
        {"NVARCHAR", MYLITE_COLUMN_STRING_BINARY_VARCHAR, true},
        {"NATIONAL VARCHAR", MYLITE_COLUMN_STRING_BINARY_VARCHAR, true},
        {"TINYTEXT", MYLITE_COLUMN_STRING_BINARY_TINYTEXT, false},
        {"TEXT", MYLITE_COLUMN_STRING_BINARY_TEXT, false},
        {"MEDIUMTEXT", MYLITE_COLUMN_STRING_BINARY_MEDIUMTEXT, false},
        {"LONGTEXT", MYLITE_COLUMN_STRING_BINARY_LONGTEXT, false},
        {"LONG VARCHAR", MYLITE_COLUMN_STRING_BINARY_MEDIUMTEXT, true},
        {"BINARY", MYLITE_COLUMN_STRING_BINARY_BINARY, false},
        {"VARBINARY", MYLITE_COLUMN_STRING_BINARY_VARBINARY, false},
        {"TINYBLOB", MYLITE_COLUMN_STRING_BINARY_TINYBLOB, false},
        {"BLOB", MYLITE_COLUMN_STRING_BINARY_BLOB, false},
        {"MEDIUMBLOB", MYLITE_COLUMN_STRING_BINARY_MEDIUMBLOB, false},
        {"LONGBLOB", MYLITE_COLUMN_STRING_BINARY_LONGBLOB, false},
        {"LONG VARBINARY", MYLITE_COLUMN_STRING_BINARY_MEDIUMBLOB, true},
    };

    if (out_type == NULL || out_is_alias == NULL) {
        return false;
    }

    for (size_t index = 0U; index < sizeof(aliases) / sizeof(aliases[0]); ++index) {
        if (ascii_case_equal(type_name, type_name_length, aliases[index].name)) {
            *out_type = aliases[index].type;
            *out_is_alias = aliases[index].is_alias;
            return true;
        }
    }
    return false;
}

static bool lookup_numeric_alias(
    const char *type_name,
    size_t type_name_length,
    enum mylite_column_numeric_type *out_type,
    bool *out_is_alias
) {
    static const struct mylite_numeric_alias aliases[] = {
        {"DECIMAL", MYLITE_COLUMN_NUMERIC_DECIMAL, false},
        {"DEC", MYLITE_COLUMN_NUMERIC_DECIMAL, true},
        {"NUMERIC", MYLITE_COLUMN_NUMERIC_DECIMAL, true},
        {"FIXED", MYLITE_COLUMN_NUMERIC_DECIMAL, true},
        {"FLOAT", MYLITE_COLUMN_NUMERIC_FLOAT, false},
        {"FLOAT4", MYLITE_COLUMN_NUMERIC_FLOAT, true},
        {"DOUBLE", MYLITE_COLUMN_NUMERIC_DOUBLE, false},
        {"DOUBLE PRECISION", MYLITE_COLUMN_NUMERIC_DOUBLE, true},
        {"REAL", MYLITE_COLUMN_NUMERIC_DOUBLE, true},
        {"FLOAT8", MYLITE_COLUMN_NUMERIC_DOUBLE, true},
    };

    if (out_type == NULL || out_is_alias == NULL) {
        return false;
    }

    for (size_t index = 0U; index < sizeof(aliases) / sizeof(aliases[0]); ++index) {
        if (ascii_case_equal(type_name, type_name_length, aliases[index].name)) {
            *out_type = aliases[index].type;
            *out_is_alias = aliases[index].is_alias;
            return true;
        }
    }
    return false;
}

static const struct mylite_temporal_type_info *lookup_temporal_type_info(
    const char *type_name,
    size_t type_name_length
) {
    static const struct mylite_temporal_type_info types[] = {
        {
            .canonical_type_name = "date",
            .type = MYLITE_COLUMN_TEMPORAL_DATE,
            .base_storage_bytes = 3U,
            .permits_fractional_seconds = false,
            .has_datetime_precision = false,
            .range_min = "1000-01-01",
            .range_max = "9999-12-31",
        },
        {
            .canonical_type_name = "time",
            .type = MYLITE_COLUMN_TEMPORAL_TIME,
            .base_storage_bytes = 3U,
            .permits_fractional_seconds = true,
            .has_datetime_precision = true,
            .range_min = "-838:59:59.000000",
            .range_max = "838:59:59.000000",
        },
        {
            .canonical_type_name = "datetime",
            .type = MYLITE_COLUMN_TEMPORAL_DATETIME,
            .base_storage_bytes = 5U,
            .permits_fractional_seconds = true,
            .has_datetime_precision = true,
            .range_min = "1000-01-01 00:00:00.000000",
            .range_max = "9999-12-31 23:59:59.499999",
        },
        {
            .canonical_type_name = "timestamp",
            .type = MYLITE_COLUMN_TEMPORAL_TIMESTAMP,
            .base_storage_bytes = 4U,
            .permits_fractional_seconds = true,
            .has_datetime_precision = true,
            .range_min = "1970-01-01 00:00:01.000000",
            .range_max = "2038-01-19 03:14:07.499999",
        },
        {
            .canonical_type_name = "year",
            .type = MYLITE_COLUMN_TEMPORAL_YEAR,
            .base_storage_bytes = 1U,
            .permits_fractional_seconds = false,
            .has_datetime_precision = false,
            .range_min = "0000",
            .range_max = "2155",
        },
    };

    for (size_t index = 0U; index < sizeof(types) / sizeof(types[0]); ++index) {
        if (ascii_case_equal(type_name, type_name_length, types[index].canonical_type_name)) {
            return &types[index];
        }
    }
    return NULL;
}

static const struct mylite_charset_info *effective_charset(
    struct mylite_column_type_attributes attributes,
    enum mylite_column_type_status *out_status
) {
    const struct mylite_charset_info *charset = NULL;
    const struct mylite_collation_info *collation = NULL;

    if (out_status == NULL) {
        return NULL;
    }
    *out_status = MYLITE_COLUMN_TYPE_OK;

    if (attributes.is_national) {
        charset = lookup_charset("utf8mb3", strlen("utf8mb3"));
    } else if (attributes.has_character_set) {
        charset = lookup_charset(attributes.character_set, attributes.character_set_length);
        if (charset == NULL) {
            *out_status = MYLITE_COLUMN_TYPE_UNKNOWN_CHARACTER_SET;
            return NULL;
        }
    }

    if (attributes.has_collation) {
        collation = lookup_collation(attributes.collation, attributes.collation_length);
        if (collation == NULL) {
            *out_status = MYLITE_COLUMN_TYPE_UNKNOWN_COLLATION;
            return NULL;
        }
        if (charset == NULL) {
            charset = lookup_charset(
                collation->character_set_name,
                strlen(collation->character_set_name)
            );
        }
    }

    if (charset == NULL) {
        charset = lookup_charset("utf8mb4", strlen("utf8mb4"));
    }

    if (collation != NULL) {
        *out_status = check_collation_matches_charset(charset, collation);
        if (*out_status != MYLITE_COLUMN_TYPE_OK) {
            return NULL;
        }
    }

    return charset;
}

static const struct mylite_charset_info *lookup_charset(const char *name, size_t name_length) {
    static const struct mylite_charset_info charsets[] = {
        {"utf8mb4", "utf8mb4_0900_ai_ci", "utf8mb4_bin", 4U, false},
        {"utf8mb3", "utf8mb3_general_ci", "utf8mb3_bin", 3U, false},
        {"latin1", "latin1_swedish_ci", "latin1_bin", 1U, false},
        {"binary", NULL, NULL, 1U, true},
    };

    for (size_t index = 0U; index < sizeof(charsets) / sizeof(charsets[0]); ++index) {
        if (ascii_case_equal(name, name_length, charsets[index].name)) {
            return &charsets[index];
        }
    }
    return NULL;
}

static const struct mylite_collation_info *lookup_collation(const char *name, size_t name_length) {
    static const struct mylite_collation_info collations[] = {
        {"binary", "binary"},
        {"utf8mb4_0900_ai_ci", "utf8mb4"},
        {"utf8mb4_unicode_520_ci", "utf8mb4"},
        {"utf8mb4_unicode_ci", "utf8mb4"},
        {"utf8mb4_bin", "utf8mb4"},
        {"utf8mb3_general_ci", "utf8mb3"},
        {"utf8mb3_bin", "utf8mb3"},
        {"latin1_swedish_ci", "latin1"},
        {"latin1_bin", "latin1"},
    };

    for (size_t index = 0U; index < sizeof(collations) / sizeof(collations[0]); ++index) {
        if (ascii_case_equal(name, name_length, collations[index].name)) {
            return &collations[index];
        }
    }
    return NULL;
}

static enum mylite_column_type_status check_collation_matches_charset(
    const struct mylite_charset_info *charset,
    const struct mylite_collation_info *collation
) {
    if (charset == NULL || collation == NULL) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    if (!ascii_case_equal(
            collation->character_set_name,
            strlen(collation->character_set_name),
            charset->name
        )) {
        return MYLITE_COLUMN_TYPE_COLLATION_CHARACTER_SET_MISMATCH;
    }
    return MYLITE_COLUMN_TYPE_OK;
}

static bool ascii_case_equal(const char *left, size_t left_length, const char *right) {
    size_t right_length = strlen(right);

    if (left == NULL || left_length != right_length) {
        return false;
    }

    for (size_t index = 0U; index < left_length; ++index) {
        if (ascii_lower((unsigned char)left[index]) != ascii_lower((unsigned char)right[index])) {
            return false;
        }
    }
    return true;
}

static char ascii_lower(unsigned char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte + ('a' - 'A'));
    }
    return (char)byte;
}

static void fill_integer_descriptor(
    const struct mylite_integer_type_info *info,
    bool is_unsigned,
    bool is_boolean_alias,
    struct mylite_column_type_attributes attributes,
    struct mylite_column_type_descriptor *out_descriptor
) {
    bool has_effective_display_width = attributes.has_display_width;
    unsigned int effective_display_width = attributes.display_width;
    unsigned int descriptor_display_width = 0U;
    unsigned int numeric_precision = info->signed_precision;
    const char *range_min = info->signed_min;
    const char *range_max = info->signed_max;

    if (is_boolean_alias) {
        has_effective_display_width = true;
        effective_display_width = 1U;
    }
    if (is_unsigned) {
        numeric_precision = info->unsigned_precision;
        range_min = info->unsigned_min;
        range_max = info->unsigned_max;
    }
    if (has_effective_display_width) {
        descriptor_display_width = effective_display_width;
    }

    *out_descriptor = (struct mylite_column_type_descriptor){
        .integer_type = info->integer_type,
        .is_unsigned = is_unsigned,
        .is_boolean_alias = is_boolean_alias,
        .has_display_width = has_effective_display_width,
        .display_width = descriptor_display_width,
        .storage_bytes = info->storage_bytes,
        .numeric_precision = numeric_precision,
        .has_numeric_scale = true,
        .numeric_scale = 0U,
        .canonical_type_name = info->canonical_type_name,
        .data_type = info->canonical_type_name,
        .signed_min = info->signed_min,
        .signed_max = info->signed_max,
        .unsigned_min = info->unsigned_min,
        .unsigned_max = info->unsigned_max,
        .range_min = range_min,
        .range_max = range_max,
    };
    format_integer_column_type(
        info,
        is_unsigned,
        is_boolean_alias,
        attributes,
        out_descriptor->column_type,
        sizeof(out_descriptor->column_type)
    );
}

static void format_integer_column_type(
    const struct mylite_integer_type_info *info,
    bool is_unsigned,
    bool is_boolean_alias,
    struct mylite_column_type_attributes attributes,
    char *out_column_type,
    size_t column_type_size
) {
    if (is_unsigned) {
        (void)snprintf(out_column_type, column_type_size, "%s unsigned", info->canonical_type_name);
        return;
    }
    if (is_boolean_alias || (info->integer_type == MYLITE_COLUMN_INTEGER_TINYINT &&
                             attributes.has_display_width && attributes.display_width == 1U)) {
        (void)snprintf(out_column_type, column_type_size, "tinyint(1)");
        return;
    }

    (void)snprintf(out_column_type, column_type_size, "%s", info->canonical_type_name);
}

static void fill_bit_descriptor(
    unsigned int precision,
    struct mylite_column_type_descriptor *out_descriptor
) {
    *out_descriptor = (struct mylite_column_type_descriptor){
        .is_bit = true,
        .storage_bytes = (precision + 7U) / 8U,
        .numeric_precision = precision,
        .has_numeric_scale = false,
        .canonical_type_name = "bit",
        .data_type = "bit",
    };
    (void)snprintf(
        out_descriptor->column_type,
        sizeof(out_descriptor->column_type),
        "bit(%u)",
        precision
    );
}

static enum mylite_column_type_status describe_decimal(
    struct mylite_column_type_attributes attributes,
    bool is_alias,
    struct mylite_column_type_descriptor *out_descriptor
) {
    uint64_t precision = decimal_default_precision;
    uint64_t scale = 0ULL;
    enum mylite_column_type_status status = MYLITE_COLUMN_TYPE_OK;

    if (attributes.has_precision) {
        precision = attributes.precision;
    }
    if (attributes.has_scale) {
        scale = attributes.scale;
    }
    if (precision == 0ULL && scale == 0ULL) {
        precision = decimal_default_precision;
    }

    status = check_precision_scale(
        (struct mylite_precision_scale){
            .precision = precision,
            .scale = scale,
        },
        (struct mylite_precision_scale_limits){
            .precision_max = decimal_precision_max,
            .scale_max = decimal_scale_max,
        }
    );
    if (status != MYLITE_COLUMN_TYPE_OK) {
        return status;
    }

    fill_numeric_descriptor(
        (struct mylite_numeric_format){
            .type = MYLITE_COLUMN_NUMERIC_DECIMAL,
            .precision = (unsigned int)precision,
            .scale = (unsigned int)scale,
            .has_scale = true,
            .is_unsigned = numeric_attributes_are_unsigned(attributes),
            .is_zerofill = attributes.has_zerofill_attribute,
        },
        is_alias,
        out_descriptor
    );
    return MYLITE_COLUMN_TYPE_OK;
}

static enum mylite_column_type_status describe_float(
    struct mylite_column_type_attributes attributes,
    bool is_alias,
    struct mylite_column_type_descriptor *out_descriptor
) {
    enum mylite_column_numeric_type effective_type = MYLITE_COLUMN_NUMERIC_FLOAT;
    uint64_t precision = float_default_precision;
    uint64_t scale = 0ULL;
    bool has_scale = attributes.has_scale;
    enum mylite_column_type_status status = MYLITE_COLUMN_TYPE_OK;

    if (attributes.has_precision && !attributes.has_scale) {
        if (attributes.precision > float_binary_precision_max) {
            return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
        }
        if (attributes.precision > float_binary_precision_cutover) {
            effective_type = MYLITE_COLUMN_NUMERIC_DOUBLE;
            precision = double_default_precision;
        }
    } else if (attributes.has_precision && attributes.has_scale) {
        precision = attributes.precision;
        scale = attributes.scale;
        if (precision == 0ULL && scale == 0ULL) {
            return MYLITE_COLUMN_TYPE_PRECISION_OUT_OF_RANGE;
        }
        status = check_precision_scale(
            (struct mylite_precision_scale){
                .precision = precision,
                .scale = scale,
            },
            (struct mylite_precision_scale_limits){
                .precision_max = approximate_display_width_max,
                .scale_max = approximate_scale_max,
            }
        );
        if (status != MYLITE_COLUMN_TYPE_OK) {
            return status;
        }
    }

    fill_numeric_descriptor(
        (struct mylite_numeric_format){
            .type = effective_type,
            .precision = (unsigned int)precision,
            .scale = (unsigned int)scale,
            .has_scale = has_scale,
            .is_unsigned = numeric_attributes_are_unsigned(attributes),
            .is_zerofill = attributes.has_zerofill_attribute,
        },
        float_descriptor_is_alias(is_alias, effective_type),
        out_descriptor
    );
    return MYLITE_COLUMN_TYPE_OK;
}

static enum mylite_column_type_status describe_double(
    enum mylite_column_numeric_type requested_type,
    struct mylite_column_type_attributes attributes,
    bool is_alias,
    struct mylite_column_type_descriptor *out_descriptor
) {
    uint64_t precision = double_default_precision;
    uint64_t scale = 0ULL;
    bool has_scale = attributes.has_scale;
    enum mylite_column_type_status status = MYLITE_COLUMN_TYPE_OK;

    (void)requested_type;

    if (attributes.has_precision && !attributes.has_scale) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    if (attributes.has_precision && attributes.has_scale) {
        precision = attributes.precision;
        scale = attributes.scale;
        if (precision == 0ULL && scale == 0ULL) {
            return MYLITE_COLUMN_TYPE_PRECISION_OUT_OF_RANGE;
        }
        status = check_precision_scale(
            (struct mylite_precision_scale){
                .precision = precision,
                .scale = scale,
            },
            (struct mylite_precision_scale_limits){
                .precision_max = approximate_display_width_max,
                .scale_max = approximate_scale_max,
            }
        );
        if (status != MYLITE_COLUMN_TYPE_OK) {
            return status;
        }
    }

    fill_numeric_descriptor(
        (struct mylite_numeric_format){
            .type = MYLITE_COLUMN_NUMERIC_DOUBLE,
            .precision = (unsigned int)precision,
            .scale = (unsigned int)scale,
            .has_scale = has_scale,
            .is_unsigned = numeric_attributes_are_unsigned(attributes),
            .is_zerofill = attributes.has_zerofill_attribute,
        },
        is_alias,
        out_descriptor
    );
    return MYLITE_COLUMN_TYPE_OK;
}

static enum mylite_column_type_status check_precision_scale(
    struct mylite_precision_scale value,
    struct mylite_precision_scale_limits limits
) {
    if (value.precision > limits.precision_max) {
        return MYLITE_COLUMN_TYPE_PRECISION_OUT_OF_RANGE;
    }
    if (value.scale > limits.scale_max) {
        return MYLITE_COLUMN_TYPE_SCALE_OUT_OF_RANGE;
    }
    if (value.scale > value.precision) {
        return MYLITE_COLUMN_TYPE_SCALE_EXCEEDS_PRECISION;
    }
    return MYLITE_COLUMN_TYPE_OK;
}

static bool numeric_attributes_are_unsigned(struct mylite_column_type_attributes attributes) {
    if (attributes.has_unsigned) {
        return true;
    }
    return attributes.has_zerofill_attribute;
}

static bool float_descriptor_is_alias(
    bool is_alias,
    enum mylite_column_numeric_type effective_type
) {
    if (is_alias) {
        return true;
    }
    return effective_type != MYLITE_COLUMN_NUMERIC_FLOAT;
}

static bool numeric_type_is_approximate(enum mylite_column_numeric_type type) {
    if (type == MYLITE_COLUMN_NUMERIC_FLOAT) {
        return true;
    }
    return type == MYLITE_COLUMN_NUMERIC_DOUBLE;
}

static void fill_numeric_descriptor(
    struct mylite_numeric_format format,
    bool is_alias,
    struct mylite_column_type_descriptor *out_descriptor
) {
    const char *data_type = name_for_numeric_type(format.type);

    *out_descriptor = (struct mylite_column_type_descriptor){
        .numeric_type = format.type,
        .is_unsigned = format.is_unsigned,
        .is_exact_numeric = format.type == MYLITE_COLUMN_NUMERIC_DECIMAL,
        .is_approximate_numeric = numeric_type_is_approximate(format.type),
        .is_alias = is_alias,
        .is_zerofill = format.is_zerofill,
        .numeric_precision = format.precision,
        .has_numeric_scale = format.has_scale,
        .numeric_scale = format.scale,
        .canonical_type_name = data_type,
        .data_type = data_type,
    };
    format_numeric_column_type(
        format,
        out_descriptor->column_type,
        sizeof(out_descriptor->column_type)
    );
}

static const char *name_for_numeric_type(enum mylite_column_numeric_type type) {
    switch (type) {
    case MYLITE_COLUMN_NUMERIC_DECIMAL:
        return "decimal";
    case MYLITE_COLUMN_NUMERIC_FLOAT:
        return "float";
    case MYLITE_COLUMN_NUMERIC_DOUBLE:
        return "double";
    case MYLITE_COLUMN_NUMERIC_NONE:
        break;
    }
    return "unknown";
}

static void format_numeric_column_type(
    struct mylite_numeric_format format,
    char *out_column_type,
    size_t column_type_size
) {
    const char *name = name_for_numeric_type(format.type);
    char base[MYLITE_COLUMN_TYPE_TEXT_CAPACITY];

    if (format.type == MYLITE_COLUMN_NUMERIC_DECIMAL || format.has_scale) {
        (void)snprintf(base, sizeof(base), "%s(%u,%u)", name, format.precision, format.scale);
    } else {
        (void)snprintf(base, sizeof(base), "%s", name);
    }

    if (format.is_zerofill) {
        (void)snprintf(out_column_type, column_type_size, "%s unsigned zerofill", base);
    } else if (format.is_unsigned) {
        (void)snprintf(out_column_type, column_type_size, "%s unsigned", base);
    } else {
        (void)snprintf(out_column_type, column_type_size, "%s", base);
    }
}

static void fill_temporal_descriptor(
    const struct mylite_temporal_type_info *info,
    struct mylite_column_type_attributes attributes,
    struct mylite_column_type_descriptor *out_descriptor
) {
    unsigned int datetime_precision = 0U;
    unsigned int storage_bytes = info->base_storage_bytes;
    bool is_alias = false;

    if (attributes.has_precision) {
        datetime_precision = (unsigned int)attributes.precision;
    }
    if (info->permits_fractional_seconds) {
        storage_bytes += fractional_storage_bytes(datetime_precision);
    }
    if (info->type == MYLITE_COLUMN_TEMPORAL_YEAR && attributes.has_precision) {
        is_alias = true;
    }

    *out_descriptor = (struct mylite_column_type_descriptor){
        .temporal_type = info->type,
        .is_alias = is_alias,
        .storage_bytes = storage_bytes,
        .has_datetime_precision = info->has_datetime_precision,
        .datetime_precision = datetime_precision,
        .canonical_type_name = info->canonical_type_name,
        .data_type = info->canonical_type_name,
        .range_min = info->range_min,
        .range_max = info->range_max,
    };
    format_temporal_column_type(
        info,
        attributes,
        out_descriptor->column_type,
        sizeof(out_descriptor->column_type)
    );
}

static unsigned int fractional_storage_bytes(unsigned int precision) {
    if (precision == 0U) {
        return 0U;
    }
    if (precision <= 2U) {
        return 1U;
    }
    if (precision <= 4U) {
        return 2U;
    }
    return 3U;
}

static void format_temporal_column_type(
    const struct mylite_temporal_type_info *info,
    struct mylite_column_type_attributes attributes,
    char *out_column_type,
    size_t column_type_size
) {
    if (info->permits_fractional_seconds && attributes.has_precision && attributes.precision > 0U) {
        (void)snprintf(
            out_column_type,
            column_type_size,
            "%s(%u)",
            info->canonical_type_name,
            (unsigned int)attributes.precision
        );
        return;
    }

    (void)snprintf(out_column_type, column_type_size, "%s", info->canonical_type_name);
}

static enum mylite_column_type_status describe_char_varchar(
    enum mylite_column_string_binary_type requested_type,
    struct mylite_column_type_attributes attributes,
    struct mylite_column_type_descriptor *out_descriptor
) {
    enum mylite_column_type_status status = MYLITE_COLUMN_TYPE_OK;
    const struct mylite_charset_info *charset = effective_charset(attributes, &status);
    const struct mylite_collation_info *collation = NULL;
    uint64_t length = 1ULL;

    if (status != MYLITE_COLUMN_TYPE_OK) {
        return status;
    }
    if (charset == NULL) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    if (attributes.has_length) {
        length = attributes.length;
    }
    if (attributes.has_binary_attribute && attributes.has_byte_attribute) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    if (requested_type == MYLITE_COLUMN_STRING_BINARY_VARCHAR && !attributes.has_length) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    if (requested_type == MYLITE_COLUMN_STRING_BINARY_CHAR && length > char_binary_length_max) {
        return MYLITE_COLUMN_TYPE_LENGTH_OUT_OF_RANGE;
    }
    if (requested_type == MYLITE_COLUMN_STRING_BINARY_VARCHAR &&
        ((!charset->is_binary &&
          length > (regular_capacity - 3ULL) / charset->max_bytes_per_character) ||
         length > varbinary_length_max)) {
        return MYLITE_COLUMN_TYPE_LENGTH_OUT_OF_RANGE;
    }

    if (attributes.has_collation) {
        collation = lookup_collation(attributes.collation, attributes.collation_length);
    }

    if (attributes.has_byte_attribute || charset->is_binary) {
        fill_binary_descriptor(
            requested_type == MYLITE_COLUMN_STRING_BINARY_CHAR
                ? MYLITE_COLUMN_STRING_BINARY_BINARY
                : MYLITE_COLUMN_STRING_BINARY_VARBINARY,
            length,
            true,
            out_descriptor
        );
        return MYLITE_COLUMN_TYPE_OK;
    }

    fill_string_descriptor(
        requested_type,
        length,
        charset,
        collation,
        attributes.has_binary_attribute,
        attributes.is_national,
        out_descriptor
    );
    return MYLITE_COLUMN_TYPE_OK;
}

static enum mylite_column_type_status describe_text_blob(
    enum mylite_column_string_binary_type requested_type,
    struct mylite_column_type_attributes attributes,
    struct mylite_column_type_descriptor *out_descriptor
) {
    enum mylite_column_type_status status = MYLITE_COLUMN_TYPE_OK;
    const struct mylite_charset_info *charset = NULL;
    const struct mylite_collation_info *collation = NULL;
    enum mylite_column_string_binary_type effective_type = requested_type;
    bool requested_blob = requested_type >= MYLITE_COLUMN_STRING_BINARY_TINYBLOB;

    if (requested_type != MYLITE_COLUMN_STRING_BINARY_TEXT &&
        requested_type != MYLITE_COLUMN_STRING_BINARY_BLOB && attributes.has_length) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    if (attributes.has_byte_attribute) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    if (requested_blob && (attributes.has_character_set || attributes.has_collation ||
                           attributes.has_binary_attribute)) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    if (attributes.has_length && attributes.length > blob_text_length_max) {
        return MYLITE_COLUMN_TYPE_LENGTH_OUT_OF_RANGE;
    }

    if (requested_blob) {
        if (requested_type == MYLITE_COLUMN_STRING_BINARY_BLOB && attributes.has_length) {
            effective_type = blob_family_for_capacity(attributes.length);
        }
        fill_binary_descriptor(
            effective_type,
            capacity_for_string_binary_type(effective_type),
            requested_type != effective_type,
            out_descriptor
        );
        return MYLITE_COLUMN_TYPE_OK;
    }

    charset = effective_charset(attributes, &status);
    if (status != MYLITE_COLUMN_TYPE_OK) {
        return status;
    }
    if (charset == NULL) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    if (attributes.has_collation) {
        collation = lookup_collation(attributes.collation, attributes.collation_length);
    }
    if (charset != NULL && charset->is_binary) {
        if (requested_type == MYLITE_COLUMN_STRING_BINARY_TEXT && attributes.has_length) {
            effective_type = blob_family_for_capacity(attributes.length);
        } else {
            effective_type = (enum mylite_column_string_binary_type)(
                requested_type +
                (MYLITE_COLUMN_STRING_BINARY_TINYBLOB - MYLITE_COLUMN_STRING_BINARY_TINYTEXT)
            );
        }
        fill_binary_descriptor(
            effective_type,
            capacity_for_string_binary_type(effective_type),
            true,
            out_descriptor
        );
        return MYLITE_COLUMN_TYPE_OK;
    }
    if (requested_type == MYLITE_COLUMN_STRING_BINARY_TEXT && attributes.has_length) {
        uint64_t capacity = attributes.length * charset->max_bytes_per_character;
        effective_type = text_family_for_capacity(capacity);
    }

    fill_string_descriptor(
        effective_type,
        capacity_for_string_binary_type(effective_type),
        charset,
        collation,
        attributes.has_binary_attribute,
        requested_type != effective_type,
        out_descriptor
    );
    return MYLITE_COLUMN_TYPE_OK;
}

static enum mylite_column_type_status describe_binary_varbinary(
    enum mylite_column_string_binary_type requested_type,
    struct mylite_column_type_attributes attributes,
    struct mylite_column_type_descriptor *out_descriptor
) {
    uint64_t length = 1ULL;

    if (attributes.has_length) {
        length = attributes.length;
    }

    if (attributes.has_character_set || attributes.has_collation ||
        attributes.has_binary_attribute || attributes.has_byte_attribute ||
        attributes.is_national) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    if (requested_type == MYLITE_COLUMN_STRING_BINARY_VARBINARY && !attributes.has_length) {
        return MYLITE_COLUMN_TYPE_INVALID_SYNTAX;
    }
    if ((requested_type == MYLITE_COLUMN_STRING_BINARY_BINARY && length > char_binary_length_max) ||
        (requested_type == MYLITE_COLUMN_STRING_BINARY_VARBINARY &&
         length > varbinary_length_max)) {
        return MYLITE_COLUMN_TYPE_LENGTH_OUT_OF_RANGE;
    }

    fill_binary_descriptor(requested_type, length, false, out_descriptor);
    return MYLITE_COLUMN_TYPE_OK;
}

static void fill_string_descriptor(
    enum mylite_column_string_binary_type type,
    uint64_t length,
    const struct mylite_charset_info *charset,
    const struct mylite_collation_info *collation,
    bool is_deprecated_binary_attribute,
    bool is_alias,
    struct mylite_column_type_descriptor *out_descriptor
) {
    const char *data_type = name_for_string_binary_type(type);
    const char *collation_name = collation == NULL ? charset->default_collation : collation->name;

    if (is_deprecated_binary_attribute) {
        collation_name = charset->binary_collation;
    }

    *out_descriptor = (struct mylite_column_type_descriptor){
        .string_binary_type = type,
        .is_character_string = true,
        .is_deprecated_binary_attribute = is_deprecated_binary_attribute,
        .is_alias = is_alias,
        .character_maximum_length = length,
        .character_octet_length = length * charset->max_bytes_per_character,
        .canonical_type_name = data_type,
        .data_type = data_type,
        .character_set_name = charset->name,
        .collation_name = collation_name,
    };
    format_string_binary_column_type(
        (struct mylite_string_binary_column_type_format){
            .type = type,
            .length = length,
        },
        out_descriptor->column_type,
        sizeof(out_descriptor->column_type)
    );
}

static void fill_binary_descriptor(
    enum mylite_column_string_binary_type type,
    uint64_t length,
    bool is_alias,
    struct mylite_column_type_descriptor *out_descriptor
) {
    const char *data_type = name_for_string_binary_type(type);

    *out_descriptor = (struct mylite_column_type_descriptor){
        .string_binary_type = type,
        .is_binary_string = true,
        .is_alias = is_alias,
        .character_maximum_length = length,
        .character_octet_length = length,
        .canonical_type_name = data_type,
        .data_type = data_type,
    };
    format_string_binary_column_type(
        (struct mylite_string_binary_column_type_format){
            .type = type,
            .length = length,
        },
        out_descriptor->column_type,
        sizeof(out_descriptor->column_type)
    );
}

static enum mylite_column_string_binary_type text_family_for_capacity(uint64_t capacity) {
    if (capacity <= tiny_capacity) {
        return MYLITE_COLUMN_STRING_BINARY_TINYTEXT;
    }
    if (capacity <= regular_capacity) {
        return MYLITE_COLUMN_STRING_BINARY_TEXT;
    }
    if (capacity <= medium_capacity) {
        return MYLITE_COLUMN_STRING_BINARY_MEDIUMTEXT;
    }
    return MYLITE_COLUMN_STRING_BINARY_LONGTEXT;
}

static enum mylite_column_string_binary_type blob_family_for_capacity(uint64_t capacity) {
    if (capacity <= tiny_capacity) {
        return MYLITE_COLUMN_STRING_BINARY_TINYBLOB;
    }
    if (capacity <= regular_capacity) {
        return MYLITE_COLUMN_STRING_BINARY_BLOB;
    }
    if (capacity <= medium_capacity) {
        return MYLITE_COLUMN_STRING_BINARY_MEDIUMBLOB;
    }
    return MYLITE_COLUMN_STRING_BINARY_LONGBLOB;
}

static uint64_t capacity_for_string_binary_type(enum mylite_column_string_binary_type type) {
    switch (type) {
    case MYLITE_COLUMN_STRING_BINARY_TINYTEXT:
    case MYLITE_COLUMN_STRING_BINARY_TINYBLOB:
        return tiny_capacity;
    case MYLITE_COLUMN_STRING_BINARY_TEXT:
    case MYLITE_COLUMN_STRING_BINARY_BLOB:
        return regular_capacity;
    case MYLITE_COLUMN_STRING_BINARY_MEDIUMTEXT:
    case MYLITE_COLUMN_STRING_BINARY_MEDIUMBLOB:
        return medium_capacity;
    case MYLITE_COLUMN_STRING_BINARY_LONGTEXT:
    case MYLITE_COLUMN_STRING_BINARY_LONGBLOB:
        return long_capacity;
    case MYLITE_COLUMN_STRING_BINARY_CHAR:
    case MYLITE_COLUMN_STRING_BINARY_VARCHAR:
    case MYLITE_COLUMN_STRING_BINARY_BINARY:
    case MYLITE_COLUMN_STRING_BINARY_VARBINARY:
    case MYLITE_COLUMN_STRING_BINARY_NONE:
        break;
    }
    return 0ULL;
}

static const char *name_for_string_binary_type(enum mylite_column_string_binary_type type) {
    switch (type) {
    case MYLITE_COLUMN_STRING_BINARY_CHAR:
        return "char";
    case MYLITE_COLUMN_STRING_BINARY_VARCHAR:
        return "varchar";
    case MYLITE_COLUMN_STRING_BINARY_TINYTEXT:
        return "tinytext";
    case MYLITE_COLUMN_STRING_BINARY_TEXT:
        return "text";
    case MYLITE_COLUMN_STRING_BINARY_MEDIUMTEXT:
        return "mediumtext";
    case MYLITE_COLUMN_STRING_BINARY_LONGTEXT:
        return "longtext";
    case MYLITE_COLUMN_STRING_BINARY_BINARY:
        return "binary";
    case MYLITE_COLUMN_STRING_BINARY_VARBINARY:
        return "varbinary";
    case MYLITE_COLUMN_STRING_BINARY_TINYBLOB:
        return "tinyblob";
    case MYLITE_COLUMN_STRING_BINARY_BLOB:
        return "blob";
    case MYLITE_COLUMN_STRING_BINARY_MEDIUMBLOB:
        return "mediumblob";
    case MYLITE_COLUMN_STRING_BINARY_LONGBLOB:
        return "longblob";
    case MYLITE_COLUMN_STRING_BINARY_NONE:
        break;
    }
    return "unknown";
}

static void format_string_binary_column_type(
    struct mylite_string_binary_column_type_format format,
    char *out_column_type,
    size_t column_type_size
) {
    const char *name = name_for_string_binary_type(format.type);

    switch (format.type) {
    case MYLITE_COLUMN_STRING_BINARY_CHAR:
    case MYLITE_COLUMN_STRING_BINARY_VARCHAR:
    case MYLITE_COLUMN_STRING_BINARY_BINARY:
    case MYLITE_COLUMN_STRING_BINARY_VARBINARY:
        (void)snprintf(
            out_column_type,
            column_type_size,
            "%s(%llu)",
            name,
            (unsigned long long)format.length
        );
        return;
    case MYLITE_COLUMN_STRING_BINARY_TINYTEXT:
    case MYLITE_COLUMN_STRING_BINARY_TEXT:
    case MYLITE_COLUMN_STRING_BINARY_MEDIUMTEXT:
    case MYLITE_COLUMN_STRING_BINARY_LONGTEXT:
    case MYLITE_COLUMN_STRING_BINARY_TINYBLOB:
    case MYLITE_COLUMN_STRING_BINARY_BLOB:
    case MYLITE_COLUMN_STRING_BINARY_MEDIUMBLOB:
    case MYLITE_COLUMN_STRING_BINARY_LONGBLOB:
    case MYLITE_COLUMN_STRING_BINARY_NONE:
        break;
    }

    (void)snprintf(out_column_type, column_type_size, "%s", name);
}
