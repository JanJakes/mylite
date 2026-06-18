#include "mylite_numeric_extras.h"

#include "mylite_sqlite_registration.h"

#include <mylite/mylite.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    crc32_bits_per_byte = 8,
    decimal_base = 10,
    exact_decimal_part_capacity = 82,
    format_max_decimals = 30,
    int64_text_capacity = 32,
};

static const uint32_t crc32_polynomial = 0xEDB88320U;

enum mylite_numeric_extra_operation {
    MYLITE_NUMERIC_EXTRA_OPERATION_NONE = 0,
    MYLITE_NUMERIC_EXTRA_OPERATION_CRC32 = 1,
    MYLITE_NUMERIC_EXTRA_OPERATION_FORMAT = 2,
    MYLITE_NUMERIC_EXTRA_OPERATION_TRUNCATE = 3,
    MYLITE_NUMERIC_EXTRA_OPERATION_PI = 4,
};

struct row_exact_decimal {
    bool is_null;
    bool is_negative;
    char integer_digits[exact_decimal_part_capacity + 1U];
    size_t integer_length;
    char fraction_digits[exact_decimal_part_capacity + 1U];
    size_t fraction_length;
};

struct row_decimal_places {
    bool is_null;
    bool is_negative;
    bool overflowed;
    uint64_t magnitude;
};

static void numeric_extra_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static bool numeric_extra_operation_from_context(
    sqlite3_context *context,
    enum mylite_numeric_extra_operation *out_operation
);
static void crc32_result(sqlite3_context *context, sqlite3_value *value);
static void format_result(sqlite3_context *context, sqlite3_value **argv);
static void truncate_result(sqlite3_context *context, sqlite3_value **argv, int argc);
static bool decimal_arguments(
    sqlite3_context *context,
    sqlite3_value **argv,
    const char *function_name,
    struct row_exact_decimal *out_value,
    struct row_decimal_places *out_places
);
static bool exact_decimal_from_sqlite_value(
    sqlite3_context *context,
    sqlite3_value *value,
    const char *function_name,
    struct row_exact_decimal *out_decimal
);
static bool exact_decimal_from_int64(
    sqlite3_context *context,
    int64_t value,
    const char *function_name,
    struct row_exact_decimal *out_decimal
);
static bool exact_decimal_from_text(
    sqlite3_context *context,
    const char *text,
    size_t text_length,
    const char *function_name,
    struct row_exact_decimal *out_decimal
);
static bool assign_exact_decimal_integer(
    sqlite3_context *context,
    const char *text,
    size_t integer_start,
    size_t integer_end,
    const char *function_name,
    struct row_exact_decimal *out_decimal
);
static bool assign_exact_decimal_fraction(
    sqlite3_context *context,
    const char *text,
    size_t text_length,
    size_t dot_index,
    const char *function_name,
    struct row_exact_decimal *out_decimal
);
static bool decimal_places_from_sqlite_value(
    sqlite3_context *context,
    sqlite3_value *value,
    const char *function_name,
    struct row_decimal_places *out_places
);
static bool decimal_places_from_int64(int64_t value, struct row_decimal_places *out_places);
static bool decimal_places_from_text(
    sqlite3_context *context,
    const char *text,
    size_t text_length,
    const char *function_name,
    struct row_decimal_places *out_places
);
static bool truncate_preferred_scale_from_sqlite_value(
    sqlite3_context *context,
    sqlite3_value *value,
    size_t *out_scale
);
static char *format_text_from_decimal(
    sqlite3_context *context,
    const struct row_exact_decimal *value,
    const struct row_decimal_places *places
);
static size_t format_decimal_place_count(const struct row_decimal_places *places);
static void copy_format_fraction_digits(
    const struct row_exact_decimal *value,
    size_t decimal_places,
    char *out_fraction_digits
);
static bool round_format_decimal_digits(
    sqlite3_context *context,
    const struct row_exact_decimal *value,
    size_t decimal_places,
    char *integer_digits,
    size_t *in_out_integer_length,
    char *fraction_digits
);
static char *assign_format_output_text(
    sqlite3_context *context,
    bool is_negative,
    const char *integer_digits,
    size_t integer_length,
    const char *fraction_digits,
    size_t decimal_places
);
static char *truncate_text_from_decimal(
    sqlite3_context *context,
    const struct row_exact_decimal *value,
    const struct row_decimal_places *places,
    bool has_preferred_scale,
    size_t preferred_scale
);
static bool decimal_parts_are_zero(
    const char *integer_digits,
    size_t integer_digit_count,
    const char *fraction_digits,
    size_t fraction_digit_count
);
static bool decimal_digits_are_zero(const char *digits, size_t digit_count);
static uint32_t crc32_checksum(const unsigned char *bytes, size_t byte_count);
static void unsupported_decimal_value_error(sqlite3_context *context, const char *function_name);

int mylite_sqlite_register_numeric_extra_functions(sqlite3 *sqlite) {
    static const enum mylite_numeric_extra_operation operations[] = {
        MYLITE_NUMERIC_EXTRA_OPERATION_NONE,
        MYLITE_NUMERIC_EXTRA_OPERATION_CRC32,
        MYLITE_NUMERIC_EXTRA_OPERATION_FORMAT,
        MYLITE_NUMERIC_EXTRA_OPERATION_TRUNCATE,
        MYLITE_NUMERIC_EXTRA_OPERATION_PI,
    };
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_crc32",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&operations[MYLITE_NUMERIC_EXTRA_OPERATION_CRC32],
            .scalar_callback = numeric_extra_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_format",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&operations[MYLITE_NUMERIC_EXTRA_OPERATION_FORMAT],
            .scalar_callback = numeric_extra_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_truncate",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&operations[MYLITE_NUMERIC_EXTRA_OPERATION_TRUNCATE],
            .scalar_callback = numeric_extra_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_truncate_scaled",
            .argument_count = 3,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&operations[MYLITE_NUMERIC_EXTRA_OPERATION_TRUNCATE],
            .scalar_callback = numeric_extra_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_pi",
            .argument_count = 0,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&operations[MYLITE_NUMERIC_EXTRA_OPERATION_PI],
            .scalar_callback = numeric_extra_sqlite_callback,
        },
    };

    return mylite_sqlite_register_functions(
        sqlite,
        registrations,
        sizeof(registrations) / sizeof(registrations[0])
    );
}

static void numeric_extra_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    enum mylite_numeric_extra_operation operation = MYLITE_NUMERIC_EXTRA_OPERATION_NONE;

    if (context == NULL || (argc != 0 && argv == NULL)) {
        sqlite3_result_error(context, "invalid MyLite numeric extra callback", -1);
        return;
    }
    if (!numeric_extra_operation_from_context(context, &operation)) {
        sqlite3_result_error(context, "invalid MyLite numeric extra operation", -1);
        return;
    }

    switch (operation) {
    case MYLITE_NUMERIC_EXTRA_OPERATION_CRC32:
        if (argc != 1 || argv[0] == NULL) {
            sqlite3_result_error(context, "invalid MyLite CRC32() callback arity", -1);
            return;
        }
        crc32_result(context, argv[0]);
        return;
    case MYLITE_NUMERIC_EXTRA_OPERATION_FORMAT:
        if (argc != 2 || argv[0] == NULL || argv[1] == NULL) {
            sqlite3_result_error(context, "invalid MyLite FORMAT() callback arity", -1);
            return;
        }
        format_result(context, argv);
        return;
    case MYLITE_NUMERIC_EXTRA_OPERATION_TRUNCATE:
        if ((argc != 2 && argc != 3) || argv[0] == NULL || argv[1] == NULL ||
            (argc == 3 && argv[2] == NULL)) {
            sqlite3_result_error(context, "invalid MyLite TRUNCATE() callback arity", -1);
            return;
        }
        truncate_result(context, argv, argc);
        return;
    case MYLITE_NUMERIC_EXTRA_OPERATION_PI:
        if (argc != 0) {
            sqlite3_result_error(context, "invalid MyLite PI() callback arity", -1);
            return;
        }
        sqlite3_result_text(context, "3.141593", -1, SQLITE_STATIC);
        return;
    case MYLITE_NUMERIC_EXTRA_OPERATION_NONE:
        break;
    }

    sqlite3_result_error(context, "invalid MyLite numeric extra operation", -1);
}

static bool numeric_extra_operation_from_context(
    sqlite3_context *context,
    enum mylite_numeric_extra_operation *out_operation
) {
    const enum mylite_numeric_extra_operation *operation = NULL;

    if (context == NULL || out_operation == NULL) {
        return false;
    }
    operation = (const enum mylite_numeric_extra_operation *)sqlite3_user_data(context);
    if (operation == NULL || *operation <= MYLITE_NUMERIC_EXTRA_OPERATION_NONE ||
        *operation > MYLITE_NUMERIC_EXTRA_OPERATION_PI) {
        return false;
    }
    *out_operation = *operation;
    return true;
}

static void crc32_result(sqlite3_context *context, sqlite3_value *value) {
    const unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    uint32_t checksum = 0U;

    if (sqlite3_value_type(value) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (sqlite3_value_type(value) == SQLITE_BLOB) {
        bytes = (const unsigned char *)sqlite3_value_blob(value);
        byte_count = (size_t)sqlite3_value_bytes(value);
    } else {
        bytes = sqlite3_value_text(value);
        byte_count = (size_t)sqlite3_value_bytes(value);
    }
    if (bytes == NULL && byte_count != 0U) {
        sqlite3_result_error_nomem(context);
        return;
    }

    checksum = crc32_checksum(bytes, byte_count);
    sqlite3_result_int64(context, (sqlite3_int64)checksum);
}

static void format_result(sqlite3_context *context, sqlite3_value **argv) {
    struct row_exact_decimal value = {0};
    struct row_decimal_places places = {0};
    char *text = NULL;

    if (!decimal_arguments(context, argv, "FORMAT", &value, &places)) {
        return;
    }
    if (value.is_null || places.is_null) {
        sqlite3_result_null(context);
        return;
    }

    text = format_text_from_decimal(context, &value, &places);
    if (text == NULL) {
        return;
    }
    sqlite3_result_text(context, text, -1, SQLITE_TRANSIENT);
    free(text);
}

static void truncate_result(sqlite3_context *context, sqlite3_value **argv, int argc) {
    struct row_exact_decimal value = {0};
    struct row_decimal_places places = {0};
    size_t preferred_scale = 0U;
    char *text = NULL;

    if (!decimal_arguments(context, argv, "TRUNCATE", &value, &places)) {
        return;
    }
    if (value.is_null || places.is_null) {
        sqlite3_result_null(context);
        return;
    }
    if (argc == 3 &&
        !truncate_preferred_scale_from_sqlite_value(context, argv[2], &preferred_scale)) {
        return;
    }

    text = truncate_text_from_decimal(context, &value, &places, argc == 3, preferred_scale);
    if (text == NULL) {
        return;
    }
    sqlite3_result_text(context, text, -1, SQLITE_TRANSIENT);
    free(text);
}

static bool decimal_arguments(
    sqlite3_context *context,
    sqlite3_value **argv,
    const char *function_name,
    struct row_exact_decimal *out_value,
    struct row_decimal_places *out_places
) {
    if (argv == NULL || function_name == NULL || out_value == NULL || out_places == NULL) {
        sqlite3_result_error(context, "invalid MyLite decimal function callback", -1);
        return false;
    }
    return exact_decimal_from_sqlite_value(context, argv[0], function_name, out_value) &&
           decimal_places_from_sqlite_value(context, argv[1], function_name, out_places);
}

static bool truncate_preferred_scale_from_sqlite_value(
    sqlite3_context *context,
    sqlite3_value *value,
    size_t *out_scale
) {
    int64_t scale = 0;

    if (value == NULL || out_scale == NULL || sqlite3_value_type(value) != SQLITE_INTEGER) {
        sqlite3_result_error(context, "invalid MyLite TRUNCATE() scale", -1);
        return false;
    }

    scale = sqlite3_value_int64(value);
    if (scale < 0 || scale >= (int64_t)exact_decimal_part_capacity) {
        sqlite3_result_error(context, "TRUNCATE() row value is outside the supported range", -1);
        return false;
    }

    *out_scale = (size_t)scale;
    return true;
}

static bool exact_decimal_from_sqlite_value(
    sqlite3_context *context,
    sqlite3_value *value,
    const char *function_name,
    struct row_exact_decimal *out_decimal
) {
    int value_type = SQLITE_NULL;
    const unsigned char *text = NULL;
    int text_length = 0;

    if (value == NULL || function_name == NULL || out_decimal == NULL) {
        sqlite3_result_error(context, "invalid MyLite decimal value", -1);
        return false;
    }
    *out_decimal = (struct row_exact_decimal){0};
    value_type = sqlite3_value_type(value);
    if (value_type == SQLITE_NULL) {
        out_decimal->is_null = true;
        return true;
    }
    if (value_type == SQLITE_INTEGER) {
        return exact_decimal_from_int64(
            context,
            sqlite3_value_int64(value),
            function_name,
            out_decimal
        );
    }
    if (value_type != SQLITE_TEXT) {
        unsupported_decimal_value_error(context, function_name);
        return false;
    }

    text = sqlite3_value_text(value);
    text_length = sqlite3_value_bytes(value);
    if (text == NULL && text_length != 0) {
        sqlite3_result_error_nomem(context);
        return false;
    }
    return exact_decimal_from_text(
        context,
        (const char *)text,
        (size_t)text_length,
        function_name,
        out_decimal
    );
}

static bool exact_decimal_from_int64(
    sqlite3_context *context,
    int64_t value,
    const char *function_name,
    struct row_exact_decimal *out_decimal
) {
    char text[int64_text_capacity];
    int written = snprintf(text, sizeof(text), "%" PRId64, value);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        sqlite3_result_error(context, "failed to format MyLite decimal integer", -1);
        return false;
    }
    return exact_decimal_from_text(context, text, (size_t)written, function_name, out_decimal);
}

static bool exact_decimal_from_text(
    sqlite3_context *context,
    const char *text,
    size_t text_length,
    const char *function_name,
    struct row_exact_decimal *out_decimal
) {
    size_t digit_start = 0U;
    size_t dot_index = SIZE_MAX;

    if (text == NULL || text_length == 0U || function_name == NULL || out_decimal == NULL) {
        unsupported_decimal_value_error(context, function_name);
        return false;
    }
    *out_decimal = (struct row_exact_decimal){0};
    if (text[0] == '-' || text[0] == '+') {
        out_decimal->is_negative = text[0] == '-';
        digit_start = 1U;
    }
    if (digit_start == text_length) {
        unsupported_decimal_value_error(context, function_name);
        return false;
    }
    for (size_t index = digit_start; index < text_length; ++index) {
        char byte = text[index];

        if (byte == '.') {
            if (dot_index != SIZE_MAX) {
                unsupported_decimal_value_error(context, function_name);
                return false;
            }
            dot_index = index;
        } else if (byte < '0' || byte > '9') {
            unsupported_decimal_value_error(context, function_name);
            return false;
        }
    }
    if (dot_index == digit_start && dot_index + 1U == text_length) {
        unsupported_decimal_value_error(context, function_name);
        return false;
    }
    return assign_exact_decimal_integer(
               context,
               text,
               digit_start,
               dot_index == SIZE_MAX ? text_length : dot_index,
               function_name,
               out_decimal
           ) &&
           assign_exact_decimal_fraction(
               context,
               text,
               text_length,
               dot_index,
               function_name,
               out_decimal
           );
}

static bool assign_exact_decimal_integer(
    sqlite3_context *context,
    const char *text,
    size_t integer_start,
    size_t integer_end,
    const char *function_name,
    struct row_exact_decimal *out_decimal
) {
    size_t integer_length = 0U;

    if (text == NULL || function_name == NULL || out_decimal == NULL ||
        integer_start > integer_end) {
        sqlite3_result_error(context, "invalid MyLite decimal integer", -1);
        return false;
    }
    while (integer_start < integer_end && text[integer_start] == '0') {
        ++integer_start;
    }
    if (integer_start == integer_end) {
        out_decimal->integer_digits[0] = '0';
        out_decimal->integer_digits[1] = '\0';
        out_decimal->integer_length = 1U;
        return true;
    }

    integer_length = integer_end - integer_start;
    if (integer_length >= exact_decimal_part_capacity) {
        unsupported_decimal_value_error(context, function_name);
        return false;
    }
    memcpy(out_decimal->integer_digits, &text[integer_start], integer_length);
    out_decimal->integer_digits[integer_length] = '\0';
    out_decimal->integer_length = integer_length;
    return true;
}

static bool assign_exact_decimal_fraction(
    sqlite3_context *context,
    const char *text,
    size_t text_length,
    size_t dot_index,
    const char *function_name,
    struct row_exact_decimal *out_decimal
) {
    size_t fraction_length = 0U;

    if (text == NULL || function_name == NULL || out_decimal == NULL) {
        sqlite3_result_error(context, "invalid MyLite decimal fraction", -1);
        return false;
    }
    if (dot_index == SIZE_MAX) {
        return true;
    }

    fraction_length = text_length - dot_index - 1U;
    if (fraction_length >= exact_decimal_part_capacity) {
        unsupported_decimal_value_error(context, function_name);
        return false;
    }
    if (fraction_length != 0U) {
        memcpy(out_decimal->fraction_digits, &text[dot_index + 1U], fraction_length);
    }
    out_decimal->fraction_digits[fraction_length] = '\0';
    out_decimal->fraction_length = fraction_length;
    return true;
}

static bool decimal_places_from_sqlite_value(
    sqlite3_context *context,
    sqlite3_value *value,
    const char *function_name,
    struct row_decimal_places *out_places
) {
    int value_type = SQLITE_NULL;
    const unsigned char *text = NULL;
    int text_length = 0;

    if (value == NULL || function_name == NULL || out_places == NULL) {
        sqlite3_result_error(context, "invalid MyLite decimal-place value", -1);
        return false;
    }
    memset(out_places, 0, sizeof(*out_places));
    value_type = sqlite3_value_type(value);
    if (value_type == SQLITE_NULL) {
        out_places->is_null = true;
        return true;
    }
    if (value_type == SQLITE_INTEGER) {
        return decimal_places_from_int64(sqlite3_value_int64(value), out_places);
    }
    if (value_type != SQLITE_TEXT) {
        unsupported_decimal_value_error(context, function_name);
        return false;
    }

    text = sqlite3_value_text(value);
    text_length = sqlite3_value_bytes(value);
    if (text == NULL && text_length != 0) {
        sqlite3_result_error_nomem(context);
        return false;
    }
    return decimal_places_from_text(
        context,
        (const char *)text,
        (size_t)text_length,
        function_name,
        out_places
    );
}

static bool decimal_places_from_int64(int64_t value, struct row_decimal_places *out_places) {
    uint64_t magnitude = 0U;

    if (out_places == NULL) {
        return false;
    }
    memset(out_places, 0, sizeof(*out_places));
    out_places->is_negative = value < 0;
    if (value == INT64_MIN) {
        magnitude = (uint64_t)INT64_MAX + 1U;
    } else if (value < 0) {
        magnitude = (uint64_t)(-value);
    } else {
        magnitude = (uint64_t)value;
    }
    out_places->magnitude = magnitude;
    if (magnitude == 0U) {
        out_places->is_negative = false;
    }
    return true;
}

static bool decimal_places_from_text(
    sqlite3_context *context,
    const char *text,
    size_t text_length,
    const char *function_name,
    struct row_decimal_places *out_places
) {
    size_t digit_start = 0U;
    uint64_t magnitude = 0U;

    if (text == NULL || text_length == 0U || function_name == NULL || out_places == NULL) {
        unsupported_decimal_value_error(context, function_name);
        return false;
    }
    memset(out_places, 0, sizeof(*out_places));
    if (text[0] == '-' || text[0] == '+') {
        out_places->is_negative = text[0] == '-';
        digit_start = 1U;
    }
    if (digit_start == text_length) {
        unsupported_decimal_value_error(context, function_name);
        return false;
    }
    for (size_t index = digit_start; index < text_length; ++index) {
        uint64_t digit = 0U;
        char byte = text[index];

        if (byte < '0' || byte > '9') {
            unsupported_decimal_value_error(context, function_name);
            return false;
        }
        digit = (uint64_t)(byte - '0');
        if (!out_places->overflowed) {
            if (magnitude > (UINT64_MAX - digit) / decimal_base) {
                magnitude = UINT64_MAX;
                out_places->overflowed = true;
            } else {
                magnitude = (magnitude * decimal_base) + digit;
            }
        }
    }
    out_places->magnitude = magnitude;
    if (magnitude == 0U && !out_places->overflowed) {
        out_places->is_negative = false;
    }
    return true;
}

static char *format_text_from_decimal(
    sqlite3_context *context,
    const struct row_exact_decimal *value,
    const struct row_decimal_places *places
) {
    char integer_digits[exact_decimal_part_capacity + 2U];
    char fraction_digits[format_max_decimals + 1U];
    size_t integer_length = 0U;
    size_t decimal_places = 0U;

    if (value == NULL || places == NULL) {
        sqlite3_result_error(context, "invalid MyLite FORMAT() decimal state", -1);
        return NULL;
    }

    decimal_places = format_decimal_place_count(places);
    integer_length = value->integer_length;
    memcpy(integer_digits, value->integer_digits, integer_length + 1U);
    copy_format_fraction_digits(value, decimal_places, fraction_digits);
    if (!round_format_decimal_digits(
            context,
            value,
            decimal_places,
            integer_digits,
            &integer_length,
            fraction_digits
        )) {
        return NULL;
    }
    return assign_format_output_text(
        context,
        value->is_negative,
        integer_digits,
        integer_length,
        fraction_digits,
        decimal_places
    );
}

static size_t format_decimal_place_count(const struct row_decimal_places *places) {
    if (places == NULL || places->is_negative) {
        return 0U;
    }
    if (places->overflowed || places->magnitude > format_max_decimals) {
        return format_max_decimals;
    }
    return (size_t)places->magnitude;
}

static void copy_format_fraction_digits(
    const struct row_exact_decimal *value,
    size_t decimal_places,
    char *out_fraction_digits
) {
    if (value == NULL || out_fraction_digits == NULL) {
        return;
    }
    for (size_t index = 0U; index < decimal_places; ++index) {
        if (index < value->fraction_length) {
            out_fraction_digits[index] = value->fraction_digits[index];
        } else {
            out_fraction_digits[index] = '0';
        }
    }
    out_fraction_digits[decimal_places] = '\0';
}

static bool round_format_decimal_digits(
    sqlite3_context *context,
    const struct row_exact_decimal *value,
    size_t decimal_places,
    char *integer_digits,
    size_t *in_out_integer_length,
    char *fraction_digits
) {
    size_t integer_length = 0U;
    bool carry = false;

    if (value == NULL || integer_digits == NULL || in_out_integer_length == NULL ||
        fraction_digits == NULL) {
        sqlite3_result_error(context, "invalid MyLite FORMAT() rounding state", -1);
        return false;
    }
    if (value->fraction_length <= decimal_places || value->fraction_digits[decimal_places] < '5') {
        return true;
    }

    integer_length = *in_out_integer_length;
    carry = true;
    for (size_t index = decimal_places; carry && index > 0U; --index) {
        if (fraction_digits[index - 1U] == '9') {
            fraction_digits[index - 1U] = '0';
        } else {
            ++fraction_digits[index - 1U];
            carry = false;
        }
    }
    for (size_t index = integer_length; carry && index > 0U; --index) {
        if (integer_digits[index - 1U] == '9') {
            integer_digits[index - 1U] = '0';
        } else {
            ++integer_digits[index - 1U];
            carry = false;
        }
    }
    if (carry) {
        if (integer_length + 1U >= exact_decimal_part_capacity + 2U) {
            sqlite3_result_error(context, "FORMAT() row value is outside the supported range", -1);
            return false;
        }
        memmove(integer_digits + 1U, integer_digits, integer_length + 1U);
        integer_digits[0] = '1';
        ++integer_length;
    }
    *in_out_integer_length = integer_length;
    return true;
}

static char *assign_format_output_text(
    sqlite3_context *context,
    bool is_negative,
    const char *integer_digits,
    size_t integer_length,
    const char *fraction_digits,
    size_t decimal_places
) {
    size_t output_length = 0U;
    size_t output_index = 0U;
    char *output = NULL;
    bool is_zero = false;

    if (integer_digits == NULL || fraction_digits == NULL) {
        sqlite3_result_error(context, "invalid MyLite FORMAT() output state", -1);
        return NULL;
    }

    is_zero =
        decimal_parts_are_zero(integer_digits, integer_length, fraction_digits, decimal_places);
    output_length = integer_length + ((integer_length - 1U) / 3U);
    if (is_negative && !is_zero) {
        ++output_length;
    }
    if (decimal_places != 0U) {
        output_length += 1U + decimal_places;
    }
    output = (char *)malloc(output_length + 1U);
    if (output == NULL) {
        sqlite3_result_error_nomem(context);
        return NULL;
    }
    if (is_negative && !is_zero) {
        output[output_index] = '-';
        ++output_index;
    }
    for (size_t index = 0U; index < integer_length; ++index) {
        if (index != 0U && ((integer_length - index) % 3U) == 0U) {
            output[output_index] = ',';
            ++output_index;
        }
        output[output_index] = integer_digits[index];
        ++output_index;
    }
    if (decimal_places != 0U) {
        output[output_index] = '.';
        ++output_index;
        memcpy(output + output_index, fraction_digits, decimal_places);
        output_index += decimal_places;
    }
    output[output_index] = '\0';
    return output;
}

static char *truncate_text_from_decimal(
    sqlite3_context *context,
    const struct row_exact_decimal *value,
    const struct row_decimal_places *places,
    bool has_preferred_scale,
    size_t preferred_scale
) {
    char integer_digits[exact_decimal_part_capacity + 1U];
    size_t integer_length = 0U;
    size_t fraction_length = 0U;
    size_t source_fraction_length = 0U;
    size_t zero_test_fraction_length = 0U;
    size_t output_length = 0U;
    size_t output_index = 0U;
    char *output = NULL;
    bool is_zero = false;

    if (value == NULL || places == NULL) {
        sqlite3_result_error(context, "invalid MyLite TRUNCATE() decimal state", -1);
        return NULL;
    }

    integer_length = value->integer_length;
    memcpy(integer_digits, value->integer_digits, integer_length + 1U);
    if (places->is_negative) {
        if (places->overflowed || places->magnitude >= integer_length) {
            integer_digits[0] = '0';
            integer_digits[1] = '\0';
            integer_length = 1U;
        } else if (places->magnitude != 0U) {
            size_t zero_count = (size_t)places->magnitude;

            memset(integer_digits + integer_length - zero_count, '0', zero_count);
            if (decimal_digits_are_zero(integer_digits, integer_length)) {
                integer_digits[0] = '0';
                integer_digits[1] = '\0';
                integer_length = 1U;
            }
        }
    } else if (places->magnitude != 0U) {
        if (places->overflowed || places->magnitude >= value->fraction_length) {
            fraction_length = value->fraction_length;
        } else {
            fraction_length = (size_t)places->magnitude;
        }
    }
    source_fraction_length = fraction_length;
    if (has_preferred_scale) {
        fraction_length = preferred_scale;
        if (source_fraction_length > fraction_length) {
            source_fraction_length = fraction_length;
        }
    }

    zero_test_fraction_length = source_fraction_length < value->fraction_length
                                    ? source_fraction_length
                                    : value->fraction_length;
    is_zero = decimal_parts_are_zero(
        integer_digits,
        integer_length,
        value->fraction_digits,
        zero_test_fraction_length
    );
    output_length = integer_length;
    if (value->is_negative && !is_zero) {
        ++output_length;
    }
    if (fraction_length != 0U) {
        output_length += 1U + fraction_length;
    }
    output = (char *)malloc(output_length + 1U);
    if (output == NULL) {
        sqlite3_result_error_nomem(context);
        return NULL;
    }
    if (value->is_negative && !is_zero) {
        output[output_index] = '-';
        ++output_index;
    }
    memcpy(output + output_index, integer_digits, integer_length);
    output_index += integer_length;
    if (fraction_length != 0U) {
        output[output_index] = '.';
        ++output_index;
        for (size_t index = 0U; index < fraction_length; ++index) {
            if (index < source_fraction_length && index < value->fraction_length) {
                output[output_index] = value->fraction_digits[index];
            } else {
                output[output_index] = '0';
            }
            ++output_index;
        }
    }
    output[output_index] = '\0';
    return output;
}

static bool decimal_parts_are_zero(
    const char *integer_digits,
    size_t integer_digit_count,
    const char *fraction_digits,
    size_t fraction_digit_count
) {
    if (!decimal_digits_are_zero(integer_digits, integer_digit_count)) {
        return false;
    }
    return decimal_digits_are_zero(fraction_digits, fraction_digit_count);
}

static bool decimal_digits_are_zero(const char *digits, size_t digit_count) {
    if (digits == NULL) {
        return true;
    }
    for (size_t index = 0U; index < digit_count; ++index) {
        if (digits[index] != '0') {
            return false;
        }
    }
    return true;
}

static uint32_t crc32_checksum(const unsigned char *bytes, size_t byte_count) {
    uint32_t checksum = UINT32_MAX;

    for (size_t byte_index = 0U; byte_index < byte_count; ++byte_index) {
        checksum ^= bytes[byte_index];
        for (unsigned int bit = 0U; bit < crc32_bits_per_byte; ++bit) {
            checksum =
                (checksum >> 1U) ^ (crc32_polynomial & (uint32_t)(-(int32_t)(checksum & 1U)));
        }
    }
    return checksum ^ UINT32_MAX;
}

static void unsupported_decimal_value_error(sqlite3_context *context, const char *function_name) {
    if (function_name != NULL && strcmp(function_name, "FORMAT") == 0) {
        sqlite3_result_error(context, "FORMAT() row value is outside the supported subset", -1);
        return;
    }
    sqlite3_result_error(context, "TRUNCATE() row value is outside the supported subset", -1);
}
