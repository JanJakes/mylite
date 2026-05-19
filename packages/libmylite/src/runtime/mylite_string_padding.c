#include "mylite_string_padding.h"

#include "mylite_sqlite_registration.h"

#include <stdbool.h>
#include <stdint.h>
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

struct string_padding_result_lengths {
    size_t value_length;
    size_t pad_length;
    size_t full_pad_repeats;
    size_t pad_remainder_length;
};

struct string_padding_write_plan {
    const char *pad;
    size_t pad_length;
    size_t full_pad_repeats;
    size_t pad_remainder_length;
};

struct utf8_tail_location {
    size_t offset;
    size_t width;
};

static void string_lpad_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void string_rpad_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void string_repeat_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void string_space_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void string_pad_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv,
    enum mylite_string_padding_side side
);
static int sqlite_value_text_argument(
    sqlite3_value *value,
    const char **out_text,
    size_t *out_length
);
static void sqlite_result_from_padding_value(
    sqlite3_context *context,
    int rc,
    char *result,
    size_t result_length,
    bool is_null,
    const char *error_message
);
static int utf8_character_count(const char *value, size_t value_length, size_t *out_count);
static int utf8_prefix_length(
    struct mylite_string_padding_slice value,
    size_t character_count,
    size_t *out_length
);
static int next_utf8_character_end(
    struct mylite_string_padding_slice value,
    size_t offset,
    size_t *out_end
);
static int utf8_character_width(unsigned char first, size_t *out_width);
static bool utf8_tail_is_valid(
    struct mylite_string_padding_slice value,
    struct utf8_tail_location location
);
static bool utf8_continuation_byte_is_valid(unsigned char byte);
static int copy_string_padding_prefix(const char *value, size_t value_length, char **out_text);
static int pad_result_length(
    struct string_padding_result_lengths lengths,
    size_t *out_result_length
);
static int repeat_result_length(
    size_t value_length,
    size_t repeat_count,
    size_t *out_result_length
);
static void write_repeated_pad(char *result, size_t *offset, struct string_padding_write_plan plan);

int mylite_string_pad_value(
    struct mylite_db *database,
    enum mylite_string_padding_side side,
    struct mylite_string_padding_slice value,
    int64_t target_length,
    struct mylite_string_padding_slice pad,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    char *result = NULL;
    size_t value_characters = 0U;
    size_t pad_characters = 0U;
    size_t requested_characters = 0U;
    size_t value_prefix_length = 0U;
    size_t pad_characters_needed = 0U;
    size_t full_pad_repeats = 0U;
    size_t pad_remainder_characters = 0U;
    size_t pad_remainder_length = 0U;
    size_t result_length = 0U;
    size_t offset = 0U;
    int rc = MYLITE_OK;

    (void)database;
    if ((value.text == NULL && value.length != 0U) || (pad.text == NULL && pad.length != 0U) ||
        out_text == NULL || out_text_length == NULL || out_is_null == NULL ||
        (side != MYLITE_STRING_PADDING_LEFT && side != MYLITE_STRING_PADDING_RIGHT)) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    if (target_length < 0) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if ((uint64_t)target_length > (uint64_t)SIZE_MAX) {
        return MYLITE_NOMEM;
    }
    requested_characters = (size_t)target_length;
    if (requested_characters == 0U) {
        return copy_string_padding_prefix("", 0U, out_text);
    }

    rc = utf8_character_count(value.text, value.length, &value_characters);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = utf8_character_count(pad.text, pad.length, &pad_characters);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (value_characters >= requested_characters) {
        rc = utf8_prefix_length(value, requested_characters, &value_prefix_length);
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_text_length = value_prefix_length;
        return copy_string_padding_prefix(value.text, value_prefix_length, out_text);
    }

    if (pad_characters == 0U) {
        return copy_string_padding_prefix("", 0U, out_text);
    }

    pad_characters_needed = requested_characters - value_characters;
    full_pad_repeats = pad_characters_needed / pad_characters;
    pad_remainder_characters = pad_characters_needed % pad_characters;
    rc = utf8_prefix_length(pad, pad_remainder_characters, &pad_remainder_length);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = pad_result_length(
        (struct string_padding_result_lengths){
            .value_length = value.length,
            .pad_length = pad.length,
            .full_pad_repeats = full_pad_repeats,
            .pad_remainder_length = pad_remainder_length,
        },
        &result_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    result = (char *)malloc(result_length + 1U);
    if (result == NULL) {
        return MYLITE_NOMEM;
    }
    if (side == MYLITE_STRING_PADDING_LEFT) {
        write_repeated_pad(
            result,
            &offset,
            (struct string_padding_write_plan){
                .pad = pad.text,
                .pad_length = pad.length,
                .full_pad_repeats = full_pad_repeats,
                .pad_remainder_length = pad_remainder_length,
            }
        );
    }
    if (value.length != 0U) {
        memcpy(&result[offset], value.text, value.length);
        offset += value.length;
    }
    if (side == MYLITE_STRING_PADDING_RIGHT) {
        write_repeated_pad(
            result,
            &offset,
            (struct string_padding_write_plan){
                .pad = pad.text,
                .pad_length = pad.length,
                .full_pad_repeats = full_pad_repeats,
                .pad_remainder_length = pad_remainder_length,
            }
        );
    }
    result[offset] = '\0';
    *out_text = result;
    *out_text_length = result_length;
    return MYLITE_OK;
}

int mylite_string_repeat_value(
    struct mylite_db *database,
    struct mylite_string_padding_slice value,
    int64_t count,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    char *result = NULL;
    size_t result_length = 0U;
    size_t repeat_count = 0U;
    size_t offset = 0U;
    int rc = MYLITE_OK;

    (void)database;
    if ((value.text == NULL && value.length != 0U) || out_text == NULL || out_text_length == NULL ||
        out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    rc = utf8_character_count(value.text, value.length, &repeat_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (count <= 0 || value.length == 0U) {
        return copy_string_padding_prefix("", 0U, out_text);
    }
    if ((uint64_t)count > (uint64_t)SIZE_MAX) {
        return MYLITE_NOMEM;
    }
    repeat_count = (size_t)count;
    rc = repeat_result_length(value.length, repeat_count, &result_length);
    if (rc != MYLITE_OK) {
        return rc;
    }

    result = (char *)malloc(result_length + 1U);
    if (result == NULL) {
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < repeat_count; ++index) {
        memcpy(&result[offset], value.text, value.length);
        offset += value.length;
    }
    result[offset] = '\0';
    *out_text = result;
    *out_text_length = result_length;
    return MYLITE_OK;
}

int mylite_string_space_value(
    int64_t count,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    char *result = NULL;
    size_t result_length = 0U;

    if (out_text == NULL || out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    if (count <= 0) {
        return copy_string_padding_prefix("", 0U, out_text);
    }
    if ((uint64_t)count >= (uint64_t)SIZE_MAX) {
        return MYLITE_NOMEM;
    }
    result_length = (size_t)count;
    result = (char *)malloc(result_length + 1U);
    if (result == NULL) {
        return MYLITE_NOMEM;
    }
    memset(result, ' ', result_length);
    result[result_length] = '\0';
    *out_text = result;
    *out_text_length = result_length;
    return MYLITE_OK;
}

int mylite_sqlite_register_string_padding_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_lpad",
            .argument_count = 3,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = string_lpad_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_rpad",
            .argument_count = 3,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = string_rpad_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_repeat",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = string_repeat_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_space",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = string_space_sqlite_callback,
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

static void string_lpad_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    string_pad_sqlite_callback(context, argc, argv, MYLITE_STRING_PADDING_LEFT);
}

static void string_rpad_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    string_pad_sqlite_callback(context, argc, argv, MYLITE_STRING_PADDING_RIGHT);
}

static void string_repeat_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    const char *value = NULL;
    size_t value_length = 0U;
    int64_t count = 0;
    char *result = NULL;
    size_t result_length = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        sqlite3_result_error(context, "invalid MyLite REPEAT callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    rc = sqlite_value_text_argument(argv[0], &value, &value_length);
    if (rc == MYLITE_OK) {
        count = sqlite3_value_int64(argv[1]);
        rc = mylite_string_repeat_value(
            NULL,
            (struct mylite_string_padding_slice){
                .text = value,
                .length = value_length,
            },
            count,
            &result,
            &result_length,
            &is_null
        );
    }
    sqlite_result_from_padding_value(
        context,
        rc,
        result,
        result_length,
        is_null,
        "MyLite REPEAT failed"
    );
}

static void string_space_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    int64_t count = 0;
    char *result = NULL;
    size_t result_length = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite SPACE callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    count = sqlite3_value_int64(argv[0]);
    rc = mylite_string_space_value(count, &result, &result_length, &is_null);
    sqlite_result_from_padding_value(
        context,
        rc,
        result,
        result_length,
        is_null,
        "MyLite SPACE failed"
    );
}

static void string_pad_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv,
    enum mylite_string_padding_side side
) {
    const char *value = NULL;
    const char *pad = NULL;
    size_t value_length = 0U;
    size_t pad_length = 0U;
    int64_t target_length = 0;
    char *result = NULL;
    size_t result_length = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 3 || argv == NULL || argv[0] == NULL || argv[1] == NULL ||
        argv[2] == NULL) {
        sqlite3_result_error(context, "invalid MyLite string padding callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL ||
        sqlite3_value_type(argv[2]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    rc = sqlite_value_text_argument(argv[0], &value, &value_length);
    if (rc == MYLITE_OK) {
        rc = sqlite_value_text_argument(argv[2], &pad, &pad_length);
    }
    if (rc == MYLITE_OK) {
        target_length = sqlite3_value_int64(argv[1]);
        rc = mylite_string_pad_value(
            NULL,
            side,
            (struct mylite_string_padding_slice){
                .text = value,
                .length = value_length,
            },
            target_length,
            (struct mylite_string_padding_slice){
                .text = pad,
                .length = pad_length,
            },
            &result,
            &result_length,
            &is_null
        );
    }
    sqlite_result_from_padding_value(
        context,
        rc,
        result,
        result_length,
        is_null,
        "MyLite string padding failed"
    );
}

static int sqlite_value_text_argument(
    sqlite3_value *value,
    const char **out_text,
    size_t *out_length
) {
    const unsigned char *text = NULL;
    int text_length = 0;

    if (value == NULL || out_text == NULL || out_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_length = 0U;

    text = sqlite3_value_text(value);
    text_length = sqlite3_value_bytes(value);
    if ((text == NULL && text_length != 0) || text_length < 0) {
        return MYLITE_NOMEM;
    }

    *out_text = (const char *)text;
    *out_length = (size_t)text_length;
    return MYLITE_OK;
}

static void sqlite_result_from_padding_value(
    sqlite3_context *context,
    int rc,
    char *result,
    size_t result_length,
    bool is_null,
    const char *error_message
) {
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        free(result);
        return;
    }
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, error_message, -1);
        free(result);
        return;
    }
    if (is_null) {
        sqlite3_result_null(context);
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

static int utf8_character_count(const char *value, size_t value_length, size_t *out_count) {
    struct mylite_string_padding_slice text = {
        .text = value,
        .length = value_length,
    };
    size_t offset = 0U;
    size_t count = 0U;

    if ((value == NULL && value_length != 0U) || out_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_count = 0U;

    while (offset < value_length) {
        size_t next_offset = offset;

        if (next_utf8_character_end(text, offset, &next_offset) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        offset = next_offset;
        ++count;
    }

    *out_count = count;
    return MYLITE_OK;
}

static int utf8_prefix_length(
    struct mylite_string_padding_slice value,
    size_t character_count,
    size_t *out_length
) {
    size_t offset = 0U;

    if ((value.text == NULL && value.length != 0U) || out_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_length = 0U;

    for (size_t index = 0U; index < character_count; ++index) {
        if (offset >= value.length ||
            next_utf8_character_end(value, offset, &offset) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
    }

    *out_length = offset;
    return MYLITE_OK;
}

static int next_utf8_character_end(
    struct mylite_string_padding_slice value,
    size_t offset,
    size_t *out_end
) {
    unsigned char first = 0U;
    size_t width = 1U;

    if (value.text == NULL || offset >= value.length || out_end == NULL) {
        return MYLITE_MISUSE;
    }
    first = (unsigned char)value.text[offset];
    if (first < utf8_ascii_limit) {
        *out_end = offset + 1U;
        return MYLITE_OK;
    }
    if (utf8_character_width(first, &width) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    if (width > value.length - offset || !utf8_tail_is_valid(
                                             value,
                                             (struct utf8_tail_location){
                                                 .offset = offset,
                                                 .width = width,
                                             }
                                         )) {
        return MYLITE_ERROR;
    }
    *out_end = offset + width;
    return MYLITE_OK;
}

static int utf8_character_width(unsigned char first, size_t *out_width) {
    if (out_width == NULL) {
        return MYLITE_MISUSE;
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

static bool utf8_tail_is_valid(
    struct mylite_string_padding_slice value,
    struct utf8_tail_location location
) {
    const unsigned char *bytes = (const unsigned char *)value.text;
    unsigned char first = bytes[location.offset];
    unsigned char second = bytes[location.offset + 1U];

    if (!utf8_continuation_byte_is_valid(second)) {
        return false;
    }
    if (location.width == 2U) {
        return true;
    }
    if ((first == utf8_three_byte_lead_min && second < utf8_three_byte_second_min_after_e0) ||
        (first == utf8_three_byte_lead_before_surrogate &&
         second > utf8_three_byte_second_max_before_surrogate) ||
        !utf8_continuation_byte_is_valid(bytes[location.offset + 2U])) {
        return false;
    }
    if (location.width == 3U) {
        return true;
    }
    if (first < utf8_four_byte_lead_min || first > utf8_four_byte_lead_max ||
        (first == utf8_four_byte_lead_min && second < utf8_four_byte_second_min_after_f0) ||
        (first == utf8_four_byte_lead_max && second > utf8_four_byte_second_max_after_f4)) {
        return false;
    }
    return utf8_continuation_byte_is_valid(bytes[location.offset + 3U]);
}

static bool utf8_continuation_byte_is_valid(unsigned char byte) {
    return (byte & utf8_continuation_mask) == utf8_continuation_tag;
}

static int copy_string_padding_prefix(const char *value, size_t value_length, char **out_text) {
    char *result = NULL;

    if ((value == NULL && value_length != 0U) || out_text == NULL) {
        return MYLITE_MISUSE;
    }
    if (value_length == SIZE_MAX) {
        return MYLITE_NOMEM;
    }

    result = (char *)malloc(value_length + 1U);
    if (result == NULL) {
        return MYLITE_NOMEM;
    }
    if (value_length != 0U) {
        memcpy(result, value, value_length);
    }
    result[value_length] = '\0';
    *out_text = result;
    return MYLITE_OK;
}

static int pad_result_length(
    struct string_padding_result_lengths lengths,
    size_t *out_result_length
) {
    size_t repeated_pad_length = 0U;
    size_t result_length = 0U;

    if (out_result_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result_length = 0U;

    if (lengths.full_pad_repeats != 0U &&
        lengths.pad_length > SIZE_MAX / lengths.full_pad_repeats) {
        return MYLITE_NOMEM;
    }
    repeated_pad_length = lengths.full_pad_repeats * lengths.pad_length;
    if (repeated_pad_length > SIZE_MAX - lengths.pad_remainder_length) {
        return MYLITE_NOMEM;
    }
    repeated_pad_length += lengths.pad_remainder_length;
    if (repeated_pad_length > SIZE_MAX - lengths.value_length) {
        return MYLITE_NOMEM;
    }
    result_length = repeated_pad_length + lengths.value_length;
    if (result_length == SIZE_MAX) {
        return MYLITE_NOMEM;
    }
    *out_result_length = result_length;
    return MYLITE_OK;
}

static int repeat_result_length(
    size_t value_length,
    size_t repeat_count,
    size_t *out_result_length
) {
    size_t result_length = 0U;

    if (out_result_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result_length = 0U;
    if (repeat_count != 0U && value_length > SIZE_MAX / repeat_count) {
        return MYLITE_NOMEM;
    }
    result_length = value_length * repeat_count;
    if (result_length == SIZE_MAX) {
        return MYLITE_NOMEM;
    }
    *out_result_length = result_length;
    return MYLITE_OK;
}

static void write_repeated_pad(
    char *result,
    size_t *offset,
    struct string_padding_write_plan plan
) {
    if (result == NULL || offset == NULL || (plan.pad == NULL && plan.pad_length != 0U)) {
        return;
    }
    for (size_t index = 0U; index < plan.full_pad_repeats; ++index) {
        if (plan.pad_length != 0U) {
            memcpy(&result[*offset], plan.pad, plan.pad_length);
            *offset += plan.pad_length;
        }
    }
    if (plan.pad_remainder_length != 0U) {
        memcpy(&result[*offset], plan.pad, plan.pad_remainder_length);
        *offset += plan.pad_remainder_length;
    }
}
