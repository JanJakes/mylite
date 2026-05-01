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

enum {
    display_width_max = 255U,
};

static const struct mylite_integer_type_info *
lookup_integer_type_info(enum mylite_column_integer_type integer_type);
static bool lookup_integer_alias(const char *type_name, size_t type_name_length,
                                 enum mylite_column_integer_type *out_integer_type,
                                 bool *out_is_boolean_alias);
static bool ascii_case_equal(const char *left, size_t left_length, const char *right);
static char ascii_lower(unsigned char byte);
static void fill_descriptor(const struct mylite_integer_type_info *info, bool is_unsigned,
                            bool is_boolean_alias, struct mylite_column_type_attributes attributes,
                            struct mylite_column_type_descriptor *out_descriptor);
static void format_column_type(const struct mylite_integer_type_info *info, bool is_unsigned,
                               bool is_boolean_alias,
                               struct mylite_column_type_attributes attributes,
                               char *out_column_type, size_t column_type_size);

enum mylite_column_type_status
mylite_column_type_describe_integer(const char *type_name, size_t type_name_length,
                                    struct mylite_column_type_attributes attributes,
                                    struct mylite_column_type_descriptor *out_descriptor)
{
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
    fill_descriptor(info, is_unsigned, is_boolean_alias, attributes, out_descriptor);
    return MYLITE_COLUMN_TYPE_OK;
}

const char *mylite_column_type_status_name(enum mylite_column_type_status status)
{
    switch (status) {
    case MYLITE_COLUMN_TYPE_OK:
        return "ok";
    case MYLITE_COLUMN_TYPE_UNKNOWN:
        return "unknown";
    case MYLITE_COLUMN_TYPE_INVALID_SYNTAX:
        return "invalid_syntax";
    case MYLITE_COLUMN_TYPE_DISPLAY_WIDTH_OUT_OF_RANGE:
        return "display_width_out_of_range";
    }

    return "unknown";
}

static const struct mylite_integer_type_info *
lookup_integer_type_info(enum mylite_column_integer_type integer_type)
{
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

static bool lookup_integer_alias(const char *type_name, size_t type_name_length,
                                 enum mylite_column_integer_type *out_integer_type,
                                 bool *out_is_boolean_alias)
{
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

static bool ascii_case_equal(const char *left, size_t left_length, const char *right)
{
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

static char ascii_lower(unsigned char byte)
{
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte + ('a' - 'A'));
    }
    return (char)byte;
}

static void fill_descriptor(const struct mylite_integer_type_info *info, bool is_unsigned,
                            bool is_boolean_alias, struct mylite_column_type_attributes attributes,
                            struct mylite_column_type_descriptor *out_descriptor)
{
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
    format_column_type(info, is_unsigned, is_boolean_alias, attributes, out_descriptor->column_type,
                       sizeof(out_descriptor->column_type));
}

static void format_column_type(const struct mylite_integer_type_info *info, bool is_unsigned,
                               bool is_boolean_alias,
                               struct mylite_column_type_attributes attributes,
                               char *out_column_type, size_t column_type_size)
{
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
