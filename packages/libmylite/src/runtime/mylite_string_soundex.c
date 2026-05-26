#include "mylite_string_soundex.h"

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

struct soundex_slice {
    const char *text;
    size_t length;
};

struct soundex_utf8_location {
    size_t offset;
    size_t width;
};

static void string_soundex_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static int soundex_utf8_text(
    const char *text,
    size_t text_length,
    char *result,
    size_t *out_length
);
static int find_first_soundex_character(
    struct soundex_slice value,
    struct soundex_utf8_location *out_location
);
static int next_utf8_character(
    struct soundex_slice value,
    size_t offset,
    struct soundex_utf8_location *out_location
);
static int utf8_character_width(unsigned char first, size_t *out_width);
static bool utf8_tail_is_valid(struct soundex_slice value, struct soundex_utf8_location location);
static bool utf8_continuation_byte_is_valid(unsigned char byte);
static bool ascii_byte_is_letter(unsigned char byte);
static unsigned char ascii_byte_to_upper(unsigned char byte);
static char ascii_soundex_code(unsigned char byte);

int mylite_string_soundex_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_text_length
) {
    char *result = NULL;
    size_t result_length = 0U;
    int rc = MYLITE_OK;

    (void)database;
    if (text == NULL || out_text == NULL || out_text_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    if (text_length > SIZE_MAX - 4U) {
        return MYLITE_NOMEM;
    }

    result = (char *)malloc(text_length + 4U);
    if (result == NULL) {
        return MYLITE_NOMEM;
    }

    rc = soundex_utf8_text(text, text_length, result, &result_length);
    if (rc != MYLITE_OK) {
        free(result);
        return rc;
    }
    result[result_length] = '\0';

    *out_text = result;
    *out_text_length = result_length;
    return MYLITE_OK;
}

int mylite_sqlite_register_string_soundex_function(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_soundex",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = string_soundex_sqlite_callback,
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

static void string_soundex_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    const unsigned char *text = NULL;
    int text_length = 0;
    char *result = NULL;
    size_t result_length = 0U;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite SOUNDEX callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    text = sqlite3_value_text(argv[0]);
    text_length = sqlite3_value_bytes(argv[0]);
    if (text == NULL || text_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }

    rc = mylite_string_soundex_value(
        NULL,
        (const char *)text,
        (size_t)text_length,
        &result,
        &result_length
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "MyLite SOUNDEX failed", -1);
        }
        free(result);
        return;
    }

    sqlite3_result_text64(context, result, (sqlite3_uint64)result_length, free, SQLITE_UTF8);
}

static int soundex_utf8_text(
    const char *text,
    size_t text_length,
    char *result,
    size_t *out_length
) {
    struct soundex_slice value = {.text = text, .length = text_length};
    struct soundex_utf8_location location = {0};
    size_t offset = 0U;
    size_t result_length = 0U;
    size_t emitted_code_count = 0U;
    char previous_code = '\0';
    int rc = MYLITE_OK;

    if (text == NULL || result == NULL || out_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_length = 0U;

    rc = find_first_soundex_character(value, &location);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (location.width == 0U) {
        return MYLITE_OK;
    }

    if ((unsigned char)text[location.offset] < utf8_ascii_limit) {
        unsigned char prefix = ascii_byte_to_upper((unsigned char)text[location.offset]);

        result[result_length++] = (char)prefix;
        previous_code = ascii_soundex_code(prefix);
    } else {
        memcpy(result, text + location.offset, location.width);
        result_length = location.width;
    }

    offset = location.offset + location.width;
    while (offset < text_length) {
        unsigned char byte = 0U;
        char code = '\0';

        rc = next_utf8_character(value, offset, &location);
        if (rc != MYLITE_OK) {
            return rc;
        }
        byte = (unsigned char)text[location.offset];
        if (byte < utf8_ascii_limit && ascii_byte_is_letter(byte)) {
            code = ascii_soundex_code(ascii_byte_to_upper(byte));
            if (code != '\0') {
                if (code != previous_code) {
                    result[result_length++] = code;
                    ++emitted_code_count;
                }
                previous_code = code;
            }
        }
        offset = location.offset + location.width;
    }

    while (emitted_code_count < 3U) {
        result[result_length++] = '0';
        ++emitted_code_count;
    }
    *out_length = result_length;
    return MYLITE_OK;
}

static int find_first_soundex_character(
    struct soundex_slice value,
    struct soundex_utf8_location *out_location
) {
    size_t offset = 0U;

    if (out_location == NULL) {
        return MYLITE_MISUSE;
    }
    *out_location = (struct soundex_utf8_location){0};

    while (offset < value.length) {
        int rc = next_utf8_character(value, offset, out_location);

        if (rc != MYLITE_OK) {
            return rc;
        }
        if ((unsigned char)value.text[out_location->offset] >= utf8_ascii_limit ||
            ascii_byte_is_letter((unsigned char)value.text[out_location->offset])) {
            return MYLITE_OK;
        }
        offset = out_location->offset + out_location->width;
    }
    *out_location = (struct soundex_utf8_location){0};
    return MYLITE_OK;
}

static int next_utf8_character(
    struct soundex_slice value,
    size_t offset,
    struct soundex_utf8_location *out_location
) {
    size_t width = 0U;
    int rc = MYLITE_OK;

    if (value.text == NULL || out_location == NULL || offset >= value.length) {
        return MYLITE_MISUSE;
    }

    rc = utf8_character_width((unsigned char)value.text[offset], &width);
    if (rc != MYLITE_OK || width == 0U || width > value.length - offset ||
        !utf8_tail_is_valid(
            value,
            (struct soundex_utf8_location){
                .offset = offset,
                .width = width,
            }
        )) {
        return MYLITE_ERROR;
    }

    *out_location = (struct soundex_utf8_location){.offset = offset, .width = width};
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

static bool utf8_tail_is_valid(struct soundex_slice value, struct soundex_utf8_location location) {
    const unsigned char *bytes = (const unsigned char *)value.text;
    unsigned char first = 0U;
    unsigned char second = 0U;

    if (value.text == NULL || location.width == 0U || location.width > value.length ||
        location.offset > value.length - location.width) {
        return false;
    }
    if (location.width == 1U) {
        return true;
    }

    first = bytes[location.offset];
    second = bytes[location.offset + 1U];
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

static bool ascii_byte_is_letter(unsigned char byte) {
    return ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')) != 0;
}

static unsigned char ascii_byte_to_upper(unsigned char byte) {
    if (byte >= 'a' && byte <= 'z') {
        return (unsigned char)(byte - ('a' - 'A'));
    }
    return byte;
}

static char ascii_soundex_code(unsigned char byte) {
    switch (byte) {
    case 'B':
    case 'F':
    case 'P':
    case 'V':
        return '1';
    case 'C':
    case 'G':
    case 'J':
    case 'K':
    case 'Q':
    case 'S':
    case 'X':
    case 'Z':
        return '2';
    case 'D':
    case 'T':
        return '3';
    case 'L':
        return '4';
    case 'M':
    case 'N':
        return '5';
    case 'R':
        return '6';
    default:
        return '\0';
    }
}
