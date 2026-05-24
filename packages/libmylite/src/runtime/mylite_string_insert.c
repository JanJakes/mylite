#include "mylite_string_insert.h"

#include "mylite_sqlite_registration.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

enum {
    utf8_continuation_mask = 0xc0,
    utf8_continuation_tag = 0x80,
    utf8_ascii_limit = 0x80,
    utf8_two_byte_lead_min = 0xc2,
    utf8_two_byte_lead_max = 0xdf,
    utf8_three_byte_lead_min = 0xe0,
    utf8_three_byte_lead_max = 0xef,
    utf8_four_byte_lead_min = 0xf0,
    utf8_four_byte_lead_max = 0xf4,
    utf8_three_byte_second_min_after_e0 = 0xa0,
    utf8_three_byte_lead_before_surrogate = 0xed,
    utf8_three_byte_second_max_before_surrogate = 0x9f,
    utf8_four_byte_second_min_after_f0 = 0x90,
    utf8_four_byte_second_max_after_f4 = 0x8f,
};

struct insert_slice {
    const char *text;
    size_t length;
};

static void string_insert_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int sqlite_value_text_argument(
    sqlite3_value *value,
    const char **out_text,
    size_t *out_length
);
static int copy_insert_slice(struct insert_slice slice, char **out_text, size_t *out_text_length);
static int build_insert_result(
    struct insert_slice value,
    size_t prefix_length,
    size_t suffix_offset,
    struct insert_slice replacement,
    char **out_text,
    size_t *out_text_length
);
static int insert_result_length(
    struct insert_slice value,
    size_t prefix_length,
    size_t suffix_offset,
    struct insert_slice replacement,
    size_t *out_result_length
);
static int utf8_character_count(struct insert_slice value, size_t *out_count);
static int utf8_offset_for_character(
    struct insert_slice value,
    size_t character_index,
    size_t *out_offset
);
static int utf8_character_width(unsigned char first, size_t *out_width);
static bool utf8_tail_is_valid(struct insert_slice value, size_t offset, size_t width);
static bool utf8_continuation_byte_is_valid(unsigned char byte);

int mylite_string_insert_value(
    struct mylite_db *database,
    const struct mylite_string_insert_arguments *arguments,
    char **out_text,
    size_t *out_text_length
) {
    struct insert_slice value_slice = {0};
    struct insert_slice replacement_slice = {0};
    int64_t position = 0;
    int64_t length = 0;
    size_t character_count = 0U;
    size_t replacement_character_count = 0U;
    size_t prefix_character_count = 0U;
    size_t suffix_character_index = 0U;
    size_t prefix_length = 0U;
    size_t suffix_offset = 0U;
    int rc = MYLITE_OK;

    (void)database;
    if (arguments == NULL || out_text == NULL || out_text_length == NULL) {
        return MYLITE_MISUSE;
    }

    value_slice = (struct insert_slice){
        .text = arguments->value.text,
        .length = arguments->value.length,
    };
    replacement_slice = (struct insert_slice){
        .text = arguments->replacement.text,
        .length = arguments->replacement.length,
    };
    position = arguments->position;
    length = arguments->length;

    if ((value_slice.text == NULL && value_slice.length != 0U) ||
        (replacement_slice.text == NULL && replacement_slice.length != 0U)) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;

    rc = utf8_character_count(value_slice, &character_count);
    if (rc == MYLITE_OK) {
        rc = utf8_character_count(replacement_slice, &replacement_character_count);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (position <= 0 || character_count == 0U || (uint64_t)position > character_count) {
        return copy_insert_slice(value_slice, out_text, out_text_length);
    }

    prefix_character_count = (size_t)((uint64_t)position - 1U);
    rc = utf8_offset_for_character(value_slice, prefix_character_count, &prefix_length);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (length < 0 || (uint64_t)length >= (uint64_t)(character_count - prefix_character_count)) {
        suffix_offset = value_slice.length;
    } else {
        suffix_character_index = prefix_character_count + (size_t)(uint64_t)length;
        rc = utf8_offset_for_character(value_slice, suffix_character_index, &suffix_offset);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return build_insert_result(
        value_slice,
        prefix_length,
        suffix_offset,
        replacement_slice,
        out_text,
        out_text_length
    );
}

int mylite_sqlite_register_string_insert_function(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_insert_string",
            .argument_count = 4,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = string_insert_sqlite_callback,
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

static void string_insert_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    const char *value = NULL;
    const char *replacement = NULL;
    size_t value_length = 0U;
    size_t replacement_length = 0U;
    char *result = NULL;
    size_t result_length = 0U;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 4 || argv == NULL || argv[0] == NULL || argv[1] == NULL ||
        argv[2] == NULL || argv[3] == NULL) {
        sqlite3_result_error(context, "invalid MyLite INSERT callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL ||
        sqlite3_value_type(argv[2]) == SQLITE_NULL || sqlite3_value_type(argv[3]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    rc = sqlite_value_text_argument(argv[0], &value, &value_length);
    if (rc == MYLITE_OK) {
        rc = sqlite_value_text_argument(argv[3], &replacement, &replacement_length);
    }
    if (rc != MYLITE_OK) {
        sqlite3_result_error_nomem(context);
        return;
    }

    rc = mylite_string_insert_value(
        NULL,
        &(struct mylite_string_insert_arguments){
            .value = {.text = value, .length = value_length},
            .position = (int64_t)sqlite3_value_int64(argv[1]),
            .length = (int64_t)sqlite3_value_int64(argv[2]),
            .replacement = {.text = replacement, .length = replacement_length},
        },
        &result,
        &result_length
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "MyLite INSERT string function failed", -1);
        }
        free(result);
        return;
    }

    sqlite3_result_text64(
        context,
        result,
        (sqlite3_uint64)result_length,
        SQLITE_TRANSIENT,
        SQLITE_UTF8
    );
    free(result);
}

static int sqlite_value_text_argument(
    sqlite3_value *value,
    const char **out_text,
    size_t *out_length
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (value == NULL || out_text == NULL || out_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_length = 0U;

    text = sqlite3_value_text(value);
    byte_count = sqlite3_value_bytes(value);
    if (text == NULL || byte_count < 0) {
        return MYLITE_NOMEM;
    }

    *out_text = (const char *)text;
    *out_length = (size_t)byte_count;
    return MYLITE_OK;
}

static int copy_insert_slice(struct insert_slice slice, char **out_text, size_t *out_text_length) {
    char *result = NULL;

    if ((slice.text == NULL && slice.length != 0U) || out_text == NULL || out_text_length == NULL) {
        return MYLITE_MISUSE;
    }
    if (slice.length == SIZE_MAX) {
        return MYLITE_NOMEM;
    }

    result = (char *)malloc(slice.length + 1U);
    if (result == NULL) {
        return MYLITE_NOMEM;
    }
    if (slice.length != 0U) {
        memcpy(result, slice.text, slice.length);
    }
    result[slice.length] = '\0';

    *out_text = result;
    *out_text_length = slice.length;
    return MYLITE_OK;
}

static int build_insert_result(
    struct insert_slice value,
    size_t prefix_length,
    size_t suffix_offset,
    struct insert_slice replacement,
    char **out_text,
    size_t *out_text_length
) {
    char *result = NULL;
    size_t result_length = 0U;
    size_t offset = 0U;
    int rc = MYLITE_OK;

    rc = insert_result_length(value, prefix_length, suffix_offset, replacement, &result_length);
    if (rc != MYLITE_OK) {
        return rc;
    }

    result = (char *)malloc(result_length + 1U);
    if (result == NULL) {
        return MYLITE_NOMEM;
    }
    if (prefix_length != 0U) {
        memcpy(&result[offset], value.text, prefix_length);
        offset += prefix_length;
    }
    if (replacement.length != 0U) {
        memcpy(&result[offset], replacement.text, replacement.length);
        offset += replacement.length;
    }
    if (suffix_offset < value.length) {
        memcpy(&result[offset], &value.text[suffix_offset], value.length - suffix_offset);
        offset += value.length - suffix_offset;
    }
    result[offset] = '\0';

    *out_text = result;
    *out_text_length = result_length;
    return MYLITE_OK;
}

static int insert_result_length(
    struct insert_slice value,
    size_t prefix_length,
    size_t suffix_offset,
    struct insert_slice replacement,
    size_t *out_result_length
) {
    size_t suffix_length = 0U;
    size_t result_length = 0U;

    if ((value.text == NULL && value.length != 0U) ||
        (replacement.text == NULL && replacement.length != 0U) || prefix_length > value.length ||
        suffix_offset > value.length || out_result_length == NULL) {
        return MYLITE_MISUSE;
    }

    suffix_length = value.length - suffix_offset;
    if (prefix_length > SIZE_MAX - replacement.length) {
        return MYLITE_NOMEM;
    }
    result_length = prefix_length + replacement.length;
    if (result_length > SIZE_MAX - suffix_length || result_length + suffix_length == SIZE_MAX) {
        return MYLITE_NOMEM;
    }
    *out_result_length = result_length + suffix_length;
    return MYLITE_OK;
}

static int utf8_character_count(struct insert_slice value, size_t *out_count) {
    size_t index = 0U;
    size_t count = 0U;
    int rc = MYLITE_OK;

    if ((value.text == NULL && value.length != 0U) || out_count == NULL) {
        return MYLITE_MISUSE;
    }

    while (index < value.length) {
        size_t width = 0U;

        rc = utf8_character_width((unsigned char)value.text[index], &width);
        if (rc != MYLITE_OK || !utf8_tail_is_valid(value, index, width)) {
            return MYLITE_ERROR;
        }
        index += width;
        ++count;
    }

    *out_count = count;
    return MYLITE_OK;
}

static int utf8_offset_for_character(
    struct insert_slice value,
    size_t character_index,
    size_t *out_offset
) {
    size_t index = 0U;
    size_t current_character = 0U;
    int rc = MYLITE_OK;

    if ((value.text == NULL && value.length != 0U) || out_offset == NULL) {
        return MYLITE_MISUSE;
    }
    while (index < value.length && current_character < character_index) {
        size_t width = 0U;

        rc = utf8_character_width((unsigned char)value.text[index], &width);
        if (rc != MYLITE_OK || !utf8_tail_is_valid(value, index, width)) {
            return MYLITE_ERROR;
        }
        index += width;
        ++current_character;
    }
    if (current_character != character_index) {
        return MYLITE_ERROR;
    }

    *out_offset = index;
    return MYLITE_OK;
}

static int utf8_character_width(unsigned char first, size_t *out_width) {
    if (out_width == NULL) {
        return MYLITE_MISUSE;
    }
    if (first < utf8_ascii_limit) {
        *out_width = 1U;
        return MYLITE_OK;
    }
    if (first >= utf8_two_byte_lead_min && first <= utf8_two_byte_lead_max) {
        *out_width = 2U;
        return MYLITE_OK;
    }
    if (first >= utf8_three_byte_lead_min && first <= utf8_three_byte_lead_max) {
        *out_width = 3U;
        return MYLITE_OK;
    }
    if (first >= utf8_four_byte_lead_min && first <= utf8_four_byte_lead_max) {
        *out_width = 4U;
        return MYLITE_OK;
    }
    return MYLITE_ERROR;
}

static bool utf8_tail_is_valid(struct insert_slice value, size_t offset, size_t width) {
    unsigned char first = 0U;

    if (value.text == NULL || width == 0U || offset > value.length ||
        width > value.length - offset) {
        return false;
    }
    first = (unsigned char)value.text[offset];
    for (size_t index = 1U; index < width; ++index) {
        if (!utf8_continuation_byte_is_valid((unsigned char)value.text[offset + index])) {
            return false;
        }
    }
    if (width == 3U && first == utf8_three_byte_lead_min &&
        (unsigned char)value.text[offset + 1U] < utf8_three_byte_second_min_after_e0) {
        return false;
    }
    if (width == 3U && first == utf8_three_byte_lead_before_surrogate &&
        (unsigned char)value.text[offset + 1U] > utf8_three_byte_second_max_before_surrogate) {
        return false;
    }
    if (width == 4U && first == utf8_four_byte_lead_min &&
        (unsigned char)value.text[offset + 1U] < utf8_four_byte_second_min_after_f0) {
        return false;
    }
    if (width == 4U && first == utf8_four_byte_lead_max &&
        (unsigned char)value.text[offset + 1U] > utf8_four_byte_second_max_after_f4) {
        return false;
    }
    return true;
}

static bool utf8_continuation_byte_is_valid(unsigned char byte) {
    return (byte & utf8_continuation_mask) == utf8_continuation_tag;
}
