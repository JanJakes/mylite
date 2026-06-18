#include "mylite_bitwise.h"

#include "mylite_sqlite_registration.h"

#include <mylite/mylite.h>

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

enum mylite_bitwise_operation {
    MYLITE_BITWISE_NONE = 0,
    MYLITE_BITWISE_NOT = 1,
    MYLITE_BITWISE_AND = 2,
    MYLITE_BITWISE_OR = 3,
    MYLITE_BITWISE_XOR = 4,
    MYLITE_BITWISE_LSHIFT = 5,
    MYLITE_BITWISE_RSHIFT = 6,
};

enum {
    mylite_bitwise_unsigned_decimal_capacity = 32,
    mylite_bitwise_word_bits = 64,
    mylite_bitwise_decimal_base = 10,
};

static void bitwise_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static bool bitwise_operation_from_context(
    sqlite3_context *context,
    enum mylite_bitwise_operation *out_operation
);
static bool bitwise_argument_uint64(sqlite3_value *value, uint64_t *out_value, bool *out_is_null);
static bool parse_unsigned_decimal_text(const unsigned char *text, uint64_t *out_value);
static void bitwise_result(sqlite3_context *context, uint64_t value);
static bool bitwise_operation_is_valid(enum mylite_bitwise_operation operation);

int mylite_sqlite_register_bitwise_functions(sqlite3 *sqlite) {
    static const enum mylite_bitwise_operation operations[] = {
        MYLITE_BITWISE_NONE,
        MYLITE_BITWISE_NOT,
        MYLITE_BITWISE_AND,
        MYLITE_BITWISE_OR,
        MYLITE_BITWISE_XOR,
        MYLITE_BITWISE_LSHIFT,
        MYLITE_BITWISE_RSHIFT,
    };
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_bitwise_not",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&operations[MYLITE_BITWISE_NOT],
            .scalar_callback = bitwise_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_bitwise_and",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&operations[MYLITE_BITWISE_AND],
            .scalar_callback = bitwise_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_bitwise_or",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&operations[MYLITE_BITWISE_OR],
            .scalar_callback = bitwise_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_bitwise_xor",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&operations[MYLITE_BITWISE_XOR],
            .scalar_callback = bitwise_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_bitwise_lshift",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&operations[MYLITE_BITWISE_LSHIFT],
            .scalar_callback = bitwise_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_bitwise_rshift",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&operations[MYLITE_BITWISE_RSHIFT],
            .scalar_callback = bitwise_sqlite_callback,
        },
    };

    return mylite_sqlite_register_functions(
        sqlite,
        registrations,
        sizeof(registrations) / sizeof(registrations[0])
    );
}

static void bitwise_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    enum mylite_bitwise_operation operation = MYLITE_BITWISE_NONE;
    uint64_t left = 0U;
    uint64_t right = 0U;
    bool left_is_null = false;
    bool right_is_null = false;

    if (!bitwise_operation_from_context(context, &operation)) {
        sqlite3_result_error(context, "invalid MyLite bitwise operation", -1);
        return;
    }
    if ((operation == MYLITE_BITWISE_NOT && argc != 1) ||
        (operation != MYLITE_BITWISE_NOT && argc != 2)) {
        sqlite3_result_error(context, "invalid MyLite bitwise argument count", -1);
        return;
    }
    if (!bitwise_argument_uint64(argv[0], &left, &left_is_null)) {
        sqlite3_result_error(context, "invalid MyLite bitwise argument", -1);
        return;
    }
    if (left_is_null) {
        sqlite3_result_null(context);
        return;
    }
    if (operation == MYLITE_BITWISE_NOT) {
        bitwise_result(context, ~left);
        return;
    }
    if (!bitwise_argument_uint64(argv[1], &right, &right_is_null)) {
        sqlite3_result_error(context, "invalid MyLite bitwise argument", -1);
        return;
    }
    if (right_is_null) {
        sqlite3_result_null(context);
        return;
    }

    switch (operation) {
    case MYLITE_BITWISE_AND:
        bitwise_result(context, left & right);
        return;
    case MYLITE_BITWISE_OR:
        bitwise_result(context, left | right);
        return;
    case MYLITE_BITWISE_XOR:
        bitwise_result(context, left ^ right);
        return;
    case MYLITE_BITWISE_LSHIFT:
        bitwise_result(context, right >= mylite_bitwise_word_bits ? 0U : left << right);
        return;
    case MYLITE_BITWISE_RSHIFT:
        bitwise_result(context, right >= mylite_bitwise_word_bits ? 0U : left >> right);
        return;
    case MYLITE_BITWISE_NOT:
    case MYLITE_BITWISE_NONE:
        break;
    }
    sqlite3_result_error(context, "invalid MyLite bitwise operation", -1);
}

static bool bitwise_operation_from_context(
    sqlite3_context *context,
    enum mylite_bitwise_operation *out_operation
) {
    const enum mylite_bitwise_operation *operation = NULL;

    if (context == NULL || out_operation == NULL) {
        return false;
    }
    operation = (const enum mylite_bitwise_operation *)sqlite3_user_data(context);
    if (operation == NULL || !bitwise_operation_is_valid(*operation)) {
        return false;
    }
    *out_operation = *operation;
    return true;
}

static bool bitwise_argument_uint64(sqlite3_value *value, uint64_t *out_value, bool *out_is_null) {
    int value_type = SQLITE_NULL;

    if (value == NULL || out_value == NULL || out_is_null == NULL) {
        return false;
    }
    *out_value = 0U;
    *out_is_null = false;
    value_type = sqlite3_value_type(value);
    if (value_type == SQLITE_NULL) {
        *out_is_null = true;
        return true;
    }
    if (value_type == SQLITE_INTEGER) {
        *out_value = (uint64_t)sqlite3_value_int64(value);
        return true;
    }
    if (value_type == SQLITE_TEXT) {
        return parse_unsigned_decimal_text(sqlite3_value_text(value), out_value);
    }
    return false;
}

static bool parse_unsigned_decimal_text(const unsigned char *text, uint64_t *out_value) {
    uint64_t value = 0U;
    size_t index = 0U;

    if (text == NULL || out_value == NULL || text[0] == '\0') {
        return false;
    }
    while (text[index] != '\0') {
        unsigned int digit = 0U;

        if (!isdigit(text[index])) {
            return false;
        }
        digit = (unsigned int)(text[index] - (unsigned char)'0');
        if (value > (UINT64_MAX - digit) / mylite_bitwise_decimal_base) {
            return false;
        }
        value = value * mylite_bitwise_decimal_base + digit;
        ++index;
    }
    *out_value = value;
    return true;
}

static void bitwise_result(sqlite3_context *context, uint64_t value) {
    if (value <= (uint64_t)INT64_MAX) {
        sqlite3_result_int64(context, (sqlite3_int64)value);
        return;
    }

    char buffer[mylite_bitwise_unsigned_decimal_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%" PRIu64, value);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        sqlite3_result_error(context, "failed to format MyLite bitwise result", -1);
        return;
    }
    sqlite3_result_text(context, buffer, written, SQLITE_TRANSIENT);
}

static bool bitwise_operation_is_valid(enum mylite_bitwise_operation operation) {
    switch (operation) {
    case MYLITE_BITWISE_NOT:
    case MYLITE_BITWISE_AND:
    case MYLITE_BITWISE_OR:
    case MYLITE_BITWISE_XOR:
    case MYLITE_BITWISE_LSHIFT:
    case MYLITE_BITWISE_RSHIFT:
        return true;
    case MYLITE_BITWISE_NONE:
        break;
    }
    return false;
}
