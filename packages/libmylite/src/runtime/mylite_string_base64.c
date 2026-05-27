#include "mylite_string_base64.h"

#include "mylite_sqlite_registration.h"

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    base64_decimal_text_capacity = 32,
    base64_encoded_line_width = 76,
    base64_input_group_size = 3,
    base64_output_group_size = 4,
    base64_low_two_bit_mask = 0x03U,
    base64_low_four_bit_mask = 0x0fU,
    base64_low_six_bit_mask = 0x3fU,
    base64_third_sextet_shift = 6,
    base64_lowercase_value_offset = 26,
    base64_digit_value_offset = 52,
    base64_plus_value = 62,
    base64_slash_value = 63,
};

struct base64_encode_output_state {
    size_t output_index;
    size_t line_width;
};

static void to_base64_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void from_base64_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void to_base64_sqlite_encode(sqlite3_context *context, const void *input, size_t input_size);
static void from_base64_sqlite_decode(
    sqlite3_context *context,
    const void *input,
    size_t input_size
);
static int sqlite_value_bytes_for_base64(
    sqlite3_context *context,
    sqlite3_value *value,
    char *integer_text,
    size_t integer_text_size,
    const void **out_bytes,
    size_t *out_size,
    bool *out_is_null
);
static int base64_encode_empty(char **out_text);
static int base64_encoded_output_size(size_t input_size, size_t *out_size);
static void base64_next_encode_group(
    const unsigned char *bytes,
    size_t input_size,
    size_t *in_out_input_index,
    char group[base64_output_group_size]
);
static int base64_append_encoded_group(
    char *output,
    size_t output_size,
    struct base64_encode_output_state *state,
    const char group[base64_output_group_size]
);
static int base64_decode_normalized(
    const unsigned char *normalized,
    size_t normalized_size,
    unsigned char **out_bytes,
    size_t *out_size,
    bool *out_valid
);
static int base64_decode_empty(unsigned char **out_bytes, size_t *out_size, bool *out_valid);
static int base64_validate_normalized(
    const unsigned char *normalized,
    size_t normalized_size,
    size_t *out_pad_count,
    bool *out_valid
);
static void base64_decode_valid_normalized(
    const unsigned char *normalized,
    size_t normalized_size,
    unsigned char *decoded,
    size_t output_size
);
static int base64_char_value(unsigned char byte, unsigned char *out_value);
static bool base64_ignored_whitespace(unsigned char byte);

int mylite_string_base64_encode(
    const void *input,
    size_t input_size,
    char **out_text,
    size_t *out_size
) {
    const unsigned char *bytes = input;
    size_t output_size = 0U;
    size_t input_index = 0U;
    struct base64_encode_output_state state = {0};
    char *output = NULL;
    int rc = MYLITE_OK;

    if ((input == NULL && input_size != 0U) || out_text == NULL || out_size == NULL) {
        return MYLITE_MISUSE;
    }

    *out_text = NULL;
    *out_size = 0U;

    if (input_size == 0U) {
        return base64_encode_empty(out_text);
    }

    rc = base64_encoded_output_size(input_size, &output_size);
    if (rc != MYLITE_OK) {
        return rc;
    }
    output = (char *)malloc(output_size + 1U);
    if (output == NULL) {
        return MYLITE_NOMEM;
    }

    while (input_index < input_size) {
        char group[base64_output_group_size];

        base64_next_encode_group(bytes, input_size, &input_index, group);
        rc = base64_append_encoded_group(output, output_size, &state, group);
        if (rc != MYLITE_OK) {
            free(output);
            return rc;
        }
    }
    if (state.output_index != output_size) {
        free(output);
        return MYLITE_NOMEM;
    }
    output[state.output_index] = '\0';

    *out_text = output;
    *out_size = output_size;
    return MYLITE_OK;
}

static int base64_encode_empty(char **out_text) {
    char *output = (char *)malloc(1U);

    if (output == NULL) {
        return MYLITE_NOMEM;
    }
    output[0] = '\0';
    *out_text = output;
    return MYLITE_OK;
}

static int base64_encoded_output_size(size_t input_size, size_t *out_size) {
    size_t encoded_size = 0U;
    size_t newline_count = 0U;

    if (input_size > (SIZE_MAX / base64_output_group_size) * base64_input_group_size) {
        return MYLITE_NOMEM;
    }
    if (input_size == 0U) {
        *out_size = 0U;
        return MYLITE_OK;
    }

    encoded_size = ((input_size + 2U) / base64_input_group_size) * base64_output_group_size;
    newline_count = (encoded_size - 1U) / base64_encoded_line_width;
    if (encoded_size > SIZE_MAX - newline_count - 1U) {
        return MYLITE_NOMEM;
    }

    *out_size = encoded_size + newline_count;
    return MYLITE_OK;
}

static void base64_next_encode_group(
    const unsigned char *bytes,
    size_t input_size,
    size_t *in_out_input_index,
    char group[base64_output_group_size]
) {
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    static const char padding_character = '=';
    size_t group_start = *in_out_input_index;
    unsigned int first_byte = bytes[(*in_out_input_index)++];
    unsigned int second_byte = 0U;
    unsigned int third_byte = 0U;
    size_t remaining = 0U;
    size_t group_input_size = 0U;

    if (*in_out_input_index < input_size) {
        second_byte = bytes[(*in_out_input_index)++];
    }
    if (*in_out_input_index < input_size) {
        third_byte = bytes[(*in_out_input_index)++];
    }

    remaining = input_size - group_start;
    group_input_size = remaining >= base64_input_group_size ? base64_input_group_size : remaining;
    group[0] = alphabet[(first_byte >> 2U) & base64_low_six_bit_mask];
    group[1] = alphabet
        [((first_byte & base64_low_two_bit_mask) << 4U) |
         ((second_byte >> 4U) & base64_low_four_bit_mask)];
    if (group_input_size > 1U) {
        group[2] = alphabet
            [((second_byte & base64_low_four_bit_mask) << 2U) |
             ((third_byte >> base64_third_sextet_shift) & base64_low_two_bit_mask)];
    } else {
        group[2] = padding_character;
    }
    if (group_input_size > 2U) {
        group[3] = alphabet[third_byte & base64_low_six_bit_mask];
    } else {
        group[3] = padding_character;
    }
}

static int base64_append_encoded_group(
    char *output,
    size_t output_size,
    struct base64_encode_output_state *state,
    const char group[base64_output_group_size]
) {
    for (size_t group_index = 0U; group_index < base64_output_group_size; ++group_index) {
        if (state->line_width == base64_encoded_line_width) {
            if (state->output_index >= output_size) {
                return MYLITE_NOMEM;
            }
            output[state->output_index++] = '\n';
            state->line_width = 0U;
        }
        if (state->output_index >= output_size) {
            return MYLITE_NOMEM;
        }
        output[state->output_index++] = group[group_index];
        ++state->line_width;
    }
    return MYLITE_OK;
}

int mylite_string_base64_decode(
    const void *input,
    size_t input_size,
    unsigned char **out_bytes,
    size_t *out_size,
    bool *out_valid
) {
    const unsigned char *bytes = input;
    unsigned char *normalized = NULL;
    size_t normalized_size = 0U;
    int rc = MYLITE_OK;

    if ((input == NULL && input_size != 0U) || out_bytes == NULL || out_size == NULL ||
        out_valid == NULL) {
        return MYLITE_MISUSE;
    }

    *out_bytes = NULL;
    *out_size = 0U;
    *out_valid = false;

    if (input_size == SIZE_MAX) {
        return MYLITE_NOMEM;
    }
    normalized = (unsigned char *)malloc(input_size + 1U);
    if (normalized == NULL) {
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < input_size; ++index) {
        if (!base64_ignored_whitespace(bytes[index])) {
            normalized[normalized_size++] = bytes[index];
        }
    }
    normalized[normalized_size] = '\0';

    rc = base64_decode_normalized(normalized, normalized_size, out_bytes, out_size, out_valid);
    free(normalized);
    return rc;
}

int mylite_sqlite_register_string_base64_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_to_base64",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = to_base64_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_from_base64",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = from_base64_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
    };

    return mylite_sqlite_register_functions(
        sqlite,
        registrations,
        sizeof(registrations) / sizeof(registrations[0])
    );
}

static void to_base64_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    char integer_text[base64_decimal_text_capacity];
    const void *bytes = NULL;
    size_t byte_count = 0U;
    bool is_null = false;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite TO_BASE64 callback", -1);
        return;
    }

    if (sqlite_value_bytes_for_base64(
            context,
            argv[0],
            integer_text,
            sizeof(integer_text),
            &bytes,
            &byte_count,
            &is_null
        ) != MYLITE_OK) {
        return;
    }
    if (is_null) {
        sqlite3_result_null(context);
        return;
    }
    to_base64_sqlite_encode(context, bytes, byte_count);
}

static void from_base64_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    char integer_text[base64_decimal_text_capacity];
    const void *bytes = NULL;
    size_t byte_count = 0U;
    bool is_null = false;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite FROM_BASE64 callback", -1);
        return;
    }

    if (sqlite_value_bytes_for_base64(
            context,
            argv[0],
            integer_text,
            sizeof(integer_text),
            &bytes,
            &byte_count,
            &is_null
        ) != MYLITE_OK) {
        return;
    }
    if (is_null) {
        sqlite3_result_null(context);
        return;
    }
    from_base64_sqlite_decode(context, bytes, byte_count);
}

static void to_base64_sqlite_encode(
    sqlite3_context *context,
    const void *input,
    size_t input_size
) {
    char *encoded = NULL;
    size_t encoded_size = 0U;
    int rc = mylite_string_base64_encode(input, input_size, &encoded, &encoded_size);

    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK || encoded_size > (size_t)INT_MAX) {
        free(encoded);
        sqlite3_result_error(context, "MyLite TO_BASE64 encode failed", -1);
        return;
    }

    sqlite3_result_text(context, encoded, (int)encoded_size, SQLITE_TRANSIENT);
    free(encoded);
}

static void from_base64_sqlite_decode(
    sqlite3_context *context,
    const void *input,
    size_t input_size
) {
    unsigned char *decoded = NULL;
    size_t decoded_size = 0U;
    bool valid = false;
    int rc = mylite_string_base64_decode(input, input_size, &decoded, &decoded_size, &valid);

    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "MyLite FROM_BASE64 decode failed", -1);
        return;
    }
    if (!valid) {
        sqlite3_result_null(context);
        return;
    }
    if (decoded_size > (size_t)INT_MAX) {
        free(decoded);
        sqlite3_result_error(context, "MyLite FROM_BASE64 result is too large", -1);
        return;
    }

    sqlite3_result_blob(context, decoded, (int)decoded_size, SQLITE_TRANSIENT);
    free(decoded);
}

static int sqlite_value_bytes_for_base64(
    sqlite3_context *context,
    sqlite3_value *value,
    char *integer_text,
    size_t integer_text_size,
    const void **out_bytes,
    size_t *out_size,
    bool *out_is_null
) {
    int value_type = SQLITE_NULL;

    if (context == NULL || value == NULL || integer_text == NULL || integer_text_size == 0U ||
        out_bytes == NULL || out_size == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }

    *out_bytes = NULL;
    *out_size = 0U;
    *out_is_null = false;

    value_type = sqlite3_value_type(value);
    if (value_type == SQLITE_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (value_type == SQLITE_INTEGER) {
        int written = snprintf(
            integer_text,
            integer_text_size,
            "%" PRId64,
            (int64_t)sqlite3_value_int64(value)
        );

        if (written < 0 || (size_t)written >= integer_text_size) {
            sqlite3_result_error(context, "failed to format MyLite Base64 integer", -1);
            return MYLITE_ERROR;
        }
        *out_bytes = integer_text;
        *out_size = (size_t)written;
        return MYLITE_OK;
    }
    if (value_type == SQLITE_TEXT || value_type == SQLITE_BLOB) {
        const void *bytes = sqlite3_value_blob(value);
        int byte_count = sqlite3_value_bytes(value);

        if ((bytes == NULL && byte_count != 0) || byte_count < 0) {
            sqlite3_result_error_nomem(context);
            return MYLITE_ERROR;
        }
        *out_bytes = bytes;
        *out_size = (size_t)byte_count;
        return MYLITE_OK;
    }

    *out_is_null = true;
    return MYLITE_OK;
}

static int base64_decode_normalized(
    const unsigned char *normalized,
    size_t normalized_size,
    unsigned char **out_bytes,
    size_t *out_size,
    bool *out_valid
) {
    unsigned char *decoded = NULL;
    size_t pad_count = 0U;
    size_t output_size = 0U;
    bool valid = false;
    int rc = MYLITE_OK;

    if (normalized == NULL || out_bytes == NULL || out_size == NULL || out_valid == NULL) {
        return MYLITE_MISUSE;
    }

    if (normalized_size == 0U) {
        return base64_decode_empty(out_bytes, out_size, out_valid);
    }

    rc = base64_validate_normalized(normalized, normalized_size, &pad_count, &valid);
    if (rc != MYLITE_OK || !valid) {
        return rc;
    }

    output_size = (normalized_size / base64_output_group_size) * base64_input_group_size;
    output_size -= pad_count;
    decoded = (unsigned char *)malloc(output_size + 1U);
    if (decoded == NULL) {
        return MYLITE_NOMEM;
    }

    base64_decode_valid_normalized(normalized, normalized_size, decoded, output_size);
    decoded[output_size] = '\0';

    *out_bytes = decoded;
    *out_size = output_size;
    *out_valid = true;
    return MYLITE_OK;
}

static int base64_decode_empty(unsigned char **out_bytes, size_t *out_size, bool *out_valid) {
    unsigned char *decoded = (unsigned char *)malloc(1U);

    if (decoded == NULL) {
        return MYLITE_NOMEM;
    }
    decoded[0] = '\0';
    *out_bytes = decoded;
    *out_size = 0U;
    *out_valid = true;
    return MYLITE_OK;
}

static int base64_validate_normalized(
    const unsigned char *normalized,
    size_t normalized_size,
    size_t *out_pad_count,
    bool *out_valid
) {
    size_t pad_count = 0U;

    *out_valid = false;
    if ((normalized_size % base64_output_group_size) != 0U) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; index < normalized_size; ++index) {
        unsigned char value = 0U;

        if (normalized[index] == '=') {
            if (index < normalized_size - 2U) {
                return MYLITE_OK;
            }
            ++pad_count;
            continue;
        }
        if (pad_count != 0U || base64_char_value(normalized[index], &value) != MYLITE_OK) {
            return MYLITE_OK;
        }
    }
    if (pad_count > 2U) {
        return MYLITE_OK;
    }
    if ((normalized_size / base64_output_group_size) > SIZE_MAX / base64_input_group_size) {
        return MYLITE_NOMEM;
    }
    if (((normalized_size / base64_output_group_size) * base64_input_group_size) < pad_count) {
        return MYLITE_OK;
    }

    *out_pad_count = pad_count;
    *out_valid = true;
    return MYLITE_OK;
}

static void base64_decode_valid_normalized(
    const unsigned char *normalized,
    size_t normalized_size,
    unsigned char *decoded,
    size_t output_size
) {
    size_t output_index = 0U;

    for (size_t input_index = 0U; input_index < normalized_size;
         input_index += base64_output_group_size) {
        unsigned char sextets[base64_output_group_size] = {0U, 0U, 0U, 0U};
        size_t group_pad_count = 0U;

        for (size_t group_index = 0U; group_index < base64_output_group_size; ++group_index) {
            unsigned char byte = normalized[input_index + group_index];

            if (byte == '=') {
                ++group_pad_count;
                continue;
            }
            (void)base64_char_value(byte, &sextets[group_index]);
        }

        decoded[output_index++] = (unsigned char)((sextets[0] << 2U) | (sextets[1] >> 4U));
        if (group_pad_count < 2U && output_index < output_size) {
            decoded[output_index++] = (unsigned char)((sextets[1] << 4U) | (sextets[2] >> 2U));
        }
        if (group_pad_count == 0U && output_index < output_size) {
            decoded[output_index++] =
                (unsigned char)((sextets[2] << base64_third_sextet_shift) | sextets[3]);
        }
    }
}

static int base64_char_value(unsigned char byte, unsigned char *out_value) {
    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (byte >= 'A' && byte <= 'Z') {
        *out_value = (unsigned char)(byte - 'A');
        return MYLITE_OK;
    }
    if (byte >= 'a' && byte <= 'z') {
        *out_value = (unsigned char)(byte - 'a' + base64_lowercase_value_offset);
        return MYLITE_OK;
    }
    if (byte >= '0' && byte <= '9') {
        *out_value = (unsigned char)(byte - '0' + base64_digit_value_offset);
        return MYLITE_OK;
    }
    if (byte == '+') {
        *out_value = base64_plus_value;
        return MYLITE_OK;
    }
    if (byte == '/') {
        *out_value = base64_slash_value;
        return MYLITE_OK;
    }
    return MYLITE_ERROR;
}

static bool base64_ignored_whitespace(unsigned char byte) {
    if (byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n') {
        return true;
    }
    return false;
}
