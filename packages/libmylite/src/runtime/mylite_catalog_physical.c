#include "mylite_catalog.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool hex_encoded_text_length(size_t text_length, size_t *out_length);

static char *append_hex_encoded_text(char *target, const char *source);

char *mylite_catalog_physical_table_name(const char *schema_name, const char *table_name) {
    static const char prefix[] = "__mylite_user_";
    static const char separator[] = "__";
    size_t prefix_length = sizeof(prefix) - 1U;
    size_t separator_length = sizeof(separator) - 1U;
    size_t schema_length = 0U;
    size_t table_length = 0U;
    size_t schema_hex_length = 0U;
    size_t table_hex_length = 0U;
    size_t output_length = 0U;
    char *output = NULL;
    char *cursor = NULL;

    if (schema_name == NULL || table_name == NULL || schema_name[0] == '\0' ||
        table_name[0] == '\0') {
        return NULL;
    }
    schema_length = strlen(schema_name);
    table_length = strlen(table_name);

    if (!hex_encoded_text_length(schema_length, &schema_hex_length) ||
        !hex_encoded_text_length(table_length, &table_hex_length)) {
        return NULL;
    }
    if (prefix_length > SIZE_MAX - schema_hex_length ||
        prefix_length + schema_hex_length > SIZE_MAX - separator_length ||
        prefix_length + schema_hex_length + separator_length > SIZE_MAX - table_hex_length) {
        return NULL;
    }
    output_length = prefix_length + schema_hex_length + separator_length + table_hex_length;
    if (output_length == SIZE_MAX) {
        return NULL;
    }
    output = malloc(output_length + 1U);
    if (output == NULL) {
        return NULL;
    }

    cursor = output;
    memcpy(cursor, prefix, prefix_length);
    cursor += prefix_length;
    cursor = append_hex_encoded_text(cursor, schema_name);
    memcpy(cursor, separator, separator_length);
    cursor += separator_length;
    cursor = append_hex_encoded_text(cursor, table_name);
    *cursor = '\0';
    return output;
}

static bool hex_encoded_text_length(size_t text_length, size_t *out_length) {
    enum {
        hex_encoded_byte_width = 2U,
    };

    if (text_length > SIZE_MAX / hex_encoded_byte_width) {
        return false;
    }
    *out_length = text_length * hex_encoded_byte_width;
    return true;
}

static char *append_hex_encoded_text(char *target, const char *source) {
    static const char hex_digits[] = "0123456789ABCDEF";

    enum {
        hex_digit_high_index = 0U,
        hex_digit_low_index = 1U,
        hex_encoded_byte_width = 2U,
        hex_high_shift = 4U,
        hex_low_mask = 0x0FU,
    };

    while (*source != '\0') {
        unsigned char byte = (unsigned char)*source;

        target[hex_digit_high_index] = hex_digits[byte >> hex_high_shift];
        target[hex_digit_low_index] = hex_digits[byte & hex_low_mask];
        target += hex_encoded_byte_width;
        ++source;
    }
    return target;
}
