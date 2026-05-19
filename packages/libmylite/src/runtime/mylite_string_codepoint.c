#include "mylite_string_codepoint.h"

#include "mylite_sqlite_registration.h"

#include <stdint.h>

enum {
    utf8_continuation_mask = 0xc0,
    utf8_continuation_tag = 0x80,
    utf8_ascii_byte_limit = 0x80,
    utf8_two_byte_lead_min = 0xc2,
    utf8_two_byte_lead_max = 0xdf,
    utf8_three_byte_lead_min = 0xe0,
    utf8_three_byte_lead_max = 0xef,
    utf8_four_byte_lead_min = 0xf0,
    utf8_four_byte_lead_max = 0xf4,
};

static void string_codepoint_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static bool string_codepoint_kind_from_sqlite_context(
    sqlite3_context *context,
    enum mylite_string_codepoint_kind *out_kind
);
static size_t first_utf8_character_byte_count(const unsigned char *value, size_t value_length);
static bool utf8_continuation_bytes_are_valid(
    const unsigned char *value,
    size_t value_length,
    size_t byte_count
);

int mylite_string_codepoint_value(
    enum mylite_string_codepoint_kind kind,
    const unsigned char *value,
    size_t value_length,
    bool is_binary,
    uint64_t *out_codepoint
) {
    size_t byte_count = 0U;
    uint64_t result = 0U;

    if ((value == NULL && value_length != 0U) || out_codepoint == NULL) {
        return MYLITE_MISUSE;
    }
    if (kind != MYLITE_STRING_CODEPOINT_ASCII && kind != MYLITE_STRING_CODEPOINT_ORD) {
        return MYLITE_MISUSE;
    }

    *out_codepoint = 0U;
    if (value_length == 0U) {
        return MYLITE_OK;
    }
    if (kind == MYLITE_STRING_CODEPOINT_ASCII || is_binary) {
        *out_codepoint = value[0];
        return MYLITE_OK;
    }

    byte_count = first_utf8_character_byte_count(value, value_length);
    for (size_t index = 0U; index < byte_count; ++index) {
        result = (result * UINT64_C(256)) + (uint64_t)value[index];
    }
    *out_codepoint = result;
    return MYLITE_OK;
}

int mylite_sqlite_register_string_codepoint_functions(sqlite3 *sqlite) {
    static const enum mylite_string_codepoint_kind ascii_kind = MYLITE_STRING_CODEPOINT_ASCII;
    static const enum mylite_string_codepoint_kind ord_kind = MYLITE_STRING_CODEPOINT_ORD;
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_ascii",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&ascii_kind,
            .scalar_callback = string_codepoint_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_ord",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&ord_kind,
            .scalar_callback = string_codepoint_sqlite_callback,
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

static void string_codepoint_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    enum mylite_string_codepoint_kind kind = MYLITE_STRING_CODEPOINT_ASCII;
    const unsigned char *value = NULL;
    int value_length = 0;
    uint64_t codepoint = 0U;
    bool is_binary = false;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite string codepoint callback", -1);
        return;
    }
    if (!string_codepoint_kind_from_sqlite_context(context, &kind)) {
        sqlite3_result_error(context, "invalid MyLite string codepoint kind", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    is_binary = sqlite3_value_type(argv[0]) == SQLITE_BLOB;
    if (is_binary) {
        value = (const unsigned char *)sqlite3_value_blob(argv[0]);
    } else {
        value = sqlite3_value_text(argv[0]);
    }
    value_length = sqlite3_value_bytes(argv[0]);
    if ((value == NULL && value_length != 0) || value_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }

    rc = mylite_string_codepoint_value(kind, value, (size_t)value_length, is_binary, &codepoint);
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "MyLite string codepoint conversion failed", -1);
        return;
    }

    sqlite3_result_int64(context, (sqlite3_int64)codepoint);
}

static bool string_codepoint_kind_from_sqlite_context(
    sqlite3_context *context,
    enum mylite_string_codepoint_kind *out_kind
) {
    const enum mylite_string_codepoint_kind *kind = NULL;

    if (context == NULL || out_kind == NULL) {
        return false;
    }
    kind = (const enum mylite_string_codepoint_kind *)sqlite3_user_data(context);
    if (kind == NULL ||
        (*kind != MYLITE_STRING_CODEPOINT_ASCII && *kind != MYLITE_STRING_CODEPOINT_ORD)) {
        return false;
    }
    *out_kind = *kind;
    return true;
}

static size_t first_utf8_character_byte_count(const unsigned char *value, size_t value_length) {
    unsigned char first = 0U;
    size_t byte_count = 1U;

    if (value == NULL || value_length == 0U) {
        return 0U;
    }

    first = value[0];
    if (first < utf8_ascii_byte_limit) {
        return 1U;
    }
    if (first >= utf8_two_byte_lead_min && first <= utf8_two_byte_lead_max) {
        byte_count = 2U;
    } else if (first >= utf8_three_byte_lead_min && first <= utf8_three_byte_lead_max) {
        byte_count = 3U;
    } else if (first >= utf8_four_byte_lead_min && first <= utf8_four_byte_lead_max) {
        byte_count = 4U;
    } else {
        return 1U;
    }
    if (value_length < byte_count ||
        !utf8_continuation_bytes_are_valid(value, value_length, byte_count)) {
        return 1U;
    }
    return byte_count;
}

static bool utf8_continuation_bytes_are_valid(
    const unsigned char *value,
    size_t value_length,
    size_t byte_count
) {
    if (value == NULL || value_length < byte_count || byte_count == 0U) {
        return false;
    }
    for (size_t index = 1U; index < byte_count; ++index) {
        if ((value[index] & utf8_continuation_mask) != utf8_continuation_tag) {
            return false;
        }
    }
    return true;
}
