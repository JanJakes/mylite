#include "mylite_dml_binary_literal.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    mylite_dml_binary_literal_bits_per_byte = 8,
};

static bool binary_literal_digits(
    enum mylite_dml_binary_literal_kind kind,
    const char *text,
    size_t text_length,
    const char **out_digits,
    size_t *out_digit_count
);
static int decode_hex_literal(
    const char *digits,
    size_t digit_count,
    char **out_text,
    size_t *out_length
);
static int decode_bit_literal(
    const char *digits,
    size_t digit_count,
    char **out_text,
    size_t *out_length
);
static bool parse_hex_literal_uint64(const char *digits, size_t digit_count, uint64_t *out_value);
static bool parse_bit_literal_uint64(const char *digits, size_t digit_count, uint64_t *out_value);
static int hex_digit_value(unsigned char digit);

enum mylite_dml_binary_literal_kind mylite_dml_binary_literal_kind_for_ast(
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_DML_BINARY_LITERAL_NONE;
    }
    if (node->literal_kind == MYLITE_SQL_AST_LITERAL_HEX) {
        return MYLITE_DML_BINARY_LITERAL_HEX;
    }
    if (node->literal_kind == MYLITE_SQL_AST_LITERAL_BIT) {
        return MYLITE_DML_BINARY_LITERAL_BIT;
    }
    return MYLITE_DML_BINARY_LITERAL_NONE;
}

enum mylite_dml_binary_literal_kind mylite_dml_binary_literal_kind_for_insert_value(
    enum mylite_insert_value_kind kind
) {
    if (kind == MYLITE_INSERT_VALUE_HEX_LITERAL) {
        return MYLITE_DML_BINARY_LITERAL_HEX;
    }
    if (kind == MYLITE_INSERT_VALUE_BIT_LITERAL) {
        return MYLITE_DML_BINARY_LITERAL_BIT;
    }
    return MYLITE_DML_BINARY_LITERAL_NONE;
}

int mylite_dml_binary_literal_decode(
    enum mylite_dml_binary_literal_kind kind,
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_length
) {
    const char *digits = NULL;
    size_t digit_count = 0U;

    if (out_text == NULL || out_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_length = 0U;
    if (!binary_literal_digits(kind, text, text_length, &digits, &digit_count)) {
        return MYLITE_UNSUPPORTED;
    }
    if (kind == MYLITE_DML_BINARY_LITERAL_HEX) {
        return decode_hex_literal(digits, digit_count, out_text, out_length);
    }
    if (kind == MYLITE_DML_BINARY_LITERAL_BIT) {
        return decode_bit_literal(digits, digit_count, out_text, out_length);
    }
    return MYLITE_UNSUPPORTED;
}

bool mylite_dml_binary_literal_uint64(
    enum mylite_dml_binary_literal_kind kind,
    const char *text,
    size_t text_length,
    uint64_t *out_value
) {
    const char *digits = NULL;
    size_t digit_count = 0U;

    if (out_value == NULL ||
        !binary_literal_digits(kind, text, text_length, &digits, &digit_count)) {
        return false;
    }
    if (kind == MYLITE_DML_BINARY_LITERAL_HEX) {
        return parse_hex_literal_uint64(digits, digit_count, out_value);
    }
    if (kind == MYLITE_DML_BINARY_LITERAL_BIT) {
        return parse_bit_literal_uint64(digits, digit_count, out_value);
    }
    return false;
}

int mylite_dml_binary_literal_decimal_text(uint64_t value, char **out_text, size_t *out_length) {
    char buffer[32];
    int length = 0;

    if (out_text == NULL || out_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_length = 0U;
    length = snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value);
    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return MYLITE_NOMEM;
    }
    *out_text = malloc((size_t)length + 1U);
    if (*out_text == NULL) {
        return MYLITE_NOMEM;
    }
    memcpy(*out_text, buffer, (size_t)length + 1U);
    *out_length = (size_t)length;
    return MYLITE_OK;
}

static bool binary_literal_digits(
    enum mylite_dml_binary_literal_kind kind,
    const char *text,
    size_t text_length,
    const char **out_digits,
    size_t *out_digit_count
) {
    if (text == NULL || out_digits == NULL || out_digit_count == NULL) {
        return false;
    }
    if (kind == MYLITE_DML_BINARY_LITERAL_HEX) {
        if (text_length >= 2U && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
            *out_digits = text + 2U;
            *out_digit_count = text_length - 2U;
            return true;
        }
        if (text_length >= 3U && (text[0] == 'x' || text[0] == 'X') && text[1] == '\'' &&
            text[text_length - 1U] == '\'') {
            *out_digits = text + 2U;
            *out_digit_count = text_length - 3U;
            return true;
        }
        return false;
    }
    if (kind == MYLITE_DML_BINARY_LITERAL_BIT) {
        if (text_length >= 2U && text[0] == '0' && (text[1] == 'b' || text[1] == 'B')) {
            *out_digits = text + 2U;
            *out_digit_count = text_length - 2U;
            return true;
        }
        if (text_length >= 3U && (text[0] == 'b' || text[0] == 'B') && text[1] == '\'' &&
            text[text_length - 1U] == '\'') {
            *out_digits = text + 2U;
            *out_digit_count = text_length - 3U;
            return true;
        }
    }
    return false;
}

static int decode_hex_literal(
    const char *digits,
    size_t digit_count,
    char **out_text,
    size_t *out_length
) {
    size_t result_length = (digit_count / 2U) + (digit_count % 2U);
    size_t input = 0U;
    size_t output = 0U;
    char *result = malloc(result_length + 1U);
    unsigned char *bytes = (unsigned char *)result;

    if (result == NULL) {
        return MYLITE_NOMEM;
    }
    if ((digit_count % 2U) != 0U) {
        int digit = hex_digit_value((unsigned char)digits[input++]);

        if (digit < 0) {
            free(result);
            return MYLITE_UNSUPPORTED;
        }
        bytes[output++] = (unsigned char)digit;
    }
    while (input < digit_count) {
        int high = hex_digit_value((unsigned char)digits[input]);
        int low = hex_digit_value((unsigned char)digits[input + 1U]);

        if (high < 0 || low < 0) {
            free(result);
            return MYLITE_UNSUPPORTED;
        }
        bytes[output++] = (unsigned char)((high << 4U) | low);
        input += 2U;
    }
    result[result_length] = '\0';
    *out_text = result;
    *out_length = result_length;
    return MYLITE_OK;
}

static int decode_bit_literal(
    const char *digits,
    size_t digit_count,
    char **out_text,
    size_t *out_length
) {
    size_t result_length = (digit_count + (mylite_dml_binary_literal_bits_per_byte - 1U)) /
                           mylite_dml_binary_literal_bits_per_byte;
    size_t leading_pad = (result_length * mylite_dml_binary_literal_bits_per_byte) - digit_count;
    char *result = calloc(result_length + 1U, sizeof(*result));
    unsigned char *bytes = (unsigned char *)result;

    if (result == NULL) {
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < digit_count; ++index) {
        size_t bit_position = leading_pad + index;
        unsigned char mask = (unsigned char)(1U
                                             << ((mylite_dml_binary_literal_bits_per_byte - 1U) -
                                                 (bit_position %
                                                  mylite_dml_binary_literal_bits_per_byte)));
        size_t byte_index = bit_position / mylite_dml_binary_literal_bits_per_byte;

        if (digits[index] == '0') {
            continue;
        }
        if (digits[index] != '1') {
            free(result);
            return MYLITE_UNSUPPORTED;
        }
        bytes[byte_index] = (unsigned char)(bytes[byte_index] | mask);
    }
    *out_text = result;
    *out_length = result_length;
    return MYLITE_OK;
}

static bool parse_hex_literal_uint64(const char *digits, size_t digit_count, uint64_t *out_value) {
    uint64_t value = 0U;

    for (size_t index = 0U; index < digit_count; ++index) {
        int digit = hex_digit_value((unsigned char)digits[index]);

        if (digit < 0 || value > (UINT64_MAX - (uint64_t)digit) / 16U) {
            return false;
        }
        value = (value * 16U) + (uint64_t)digit;
    }
    *out_value = value;
    return true;
}

static bool parse_bit_literal_uint64(const char *digits, size_t digit_count, uint64_t *out_value) {
    uint64_t value = 0U;

    for (size_t index = 0U; index < digit_count; ++index) {
        unsigned int bit = 0U;

        if (digits[index] == '0') {
            bit = 0U;
        } else if (digits[index] == '1') {
            bit = 1U;
        } else {
            return false;
        }
        if (value > (UINT64_MAX - bit) / 2U) {
            return false;
        }
        value = (value * 2U) + bit;
    }
    *out_value = value;
    return true;
}

static int hex_digit_value(unsigned char digit) {
    if (digit >= '0' && digit <= '9') {
        return (int)(digit - '0');
    }
    if (digit >= 'a' && digit <= 'f') {
        return (int)(digit - 'a' + 10U);
    }
    if (digit >= 'A' && digit <= 'F') {
        return (int)(digit - 'A' + 10U);
    }
    return -1;
}
