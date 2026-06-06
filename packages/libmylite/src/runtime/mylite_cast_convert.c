#include "mylite_cast_convert.h"

#include <mylite/mylite.h>

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    mysql_warning_truncated_incorrect_integer = 1292,
    mysql_warning_cast_complement = 1105,
    decimal_base = 10,
    utf8_ascii_max = 0x7f,
    big5_lead_byte_min = 0x81,
    big5_lead_byte_max = 0xfe,
};

enum mylite_cast_integer_target {
    MYLITE_CAST_INTEGER_SIGNED = 0,
    MYLITE_CAST_INTEGER_UNSIGNED = 1,
};

enum mylite_cast_text_conversion_mode {
    MYLITE_CAST_TEXT_CONVERSION_TEXT = 0,
    MYLITE_CAST_TEXT_CONVERSION_TEXT_ASCII = 1,
    MYLITE_CAST_TEXT_CONVERSION_BYTES = 2,
    MYLITE_CAST_TEXT_CONVERSION_BYTES_BIG5 = 3,
};

struct row_integer_cast_parse {
    bool is_negative;
    bool saw_digits;
    bool overflowed;
    bool has_truncated_integer_warning;
    uint64_t magnitude;
};

struct integer_cast_numeric_request {
    int64_t value;
    enum mylite_cast_integer_target target;
};

struct integer_cast_text_request {
    const char *value;
    size_t value_length;
    enum mylite_cast_integer_target target;
};

struct row_integer_cast_digit_scan {
    const char *text;
    size_t text_length;
    size_t offset;
    uint64_t limit;
};

static const enum mylite_cast_text_conversion_mode text_conversion_text =
    MYLITE_CAST_TEXT_CONVERSION_TEXT;
static const enum mylite_cast_text_conversion_mode text_conversion_text_ascii =
    MYLITE_CAST_TEXT_CONVERSION_TEXT_ASCII;
static const enum mylite_cast_text_conversion_mode text_conversion_bytes =
    MYLITE_CAST_TEXT_CONVERSION_BYTES;
static const enum mylite_cast_text_conversion_mode text_conversion_bytes_big5 =
    MYLITE_CAST_TEXT_CONVERSION_BYTES_BIG5;
static const enum mylite_cast_integer_target signed_integer_target = MYLITE_CAST_INTEGER_SIGNED;
static const enum mylite_cast_integer_target unsigned_integer_target = MYLITE_CAST_INTEGER_UNSIGNED;

static void text_conversion_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static void left_big5_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void integer_cast_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void text_conversion_value(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_cast_text_conversion_mode mode
);
static void left_big5_value(sqlite3_context *context, sqlite3_value *value, sqlite3_value *length);
static int big5_left_byte_count(const unsigned char *text, int text_length, int64_t char_length);
static int big5_complete_prefix_byte_count(const unsigned char *text, int text_length);
static bool big5_byte_is_lead(unsigned char byte);
static int big5_trim_trailing_partial_character(const unsigned char *text, int text_length);
static void integer_cast_value(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_cast_integer_target target
);
static int append_truncated_integer_warning(
    struct mylite_db *database,
    const char *value,
    size_t value_length
);
static int append_signed_complement_warning(struct mylite_db *database);
static int append_unsigned_complement_warning(struct mylite_db *database);
static void integer_cast_numeric_value(
    sqlite3_context *context,
    const struct integer_cast_numeric_request *request
);
static void integer_cast_text_value(
    sqlite3_context *context,
    const struct integer_cast_text_request *request
);
static void parse_row_integer_cast_text(
    const char *text,
    size_t text_length,
    struct row_integer_cast_parse *out_parse
);
static void parse_row_integer_cast_digits(
    const struct row_integer_cast_digit_scan *scan,
    struct row_integer_cast_parse *inout_parse,
    size_t *out_end_offset
);
static bool row_integer_cast_is_ascii_space(unsigned char byte);
static void finish_signed_integer_cast_result(
    sqlite3_context *context,
    bool is_negative,
    uint64_t magnitude
);
static void finish_unsigned_integer_cast_result(
    sqlite3_context *context,
    bool is_negative,
    uint64_t magnitude
);
static void finish_int64_result(sqlite3_context *context, int64_t value);
static void finish_uint64_result(sqlite3_context *context, uint64_t value);
static void set_warning_append_error(sqlite3_context *context, int rc);

int mylite_sqlite_register_cast_convert_functions(sqlite3 *sqlite) {
    static const struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_cast_convert_text",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&text_conversion_text,
            .scalar_callback = text_conversion_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_cast_convert_text_ascii",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&text_conversion_text_ascii,
            .scalar_callback = text_conversion_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_cast_convert_bytes",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&text_conversion_bytes,
            .scalar_callback = text_conversion_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_cast_convert_bytes_big5",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&text_conversion_bytes_big5,
            .scalar_callback = text_conversion_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_left_big5",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = left_big5_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_cast_signed",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&signed_integer_target,
            .scalar_callback = integer_cast_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_cast_unsigned",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&unsigned_integer_target,
            .scalar_callback = integer_cast_sqlite_callback,
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

static void text_conversion_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    const enum mylite_cast_text_conversion_mode *mode = NULL;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite CAST/CONVERT text callback", -1);
        return;
    }

    mode = (const enum mylite_cast_text_conversion_mode *)sqlite3_user_data(context);
    if (mode == NULL) {
        sqlite3_result_error(context, "invalid MyLite CAST/CONVERT text configuration", -1);
        return;
    }
    text_conversion_value(context, argv[0], *mode);
}

static void left_big5_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    if (context == NULL || argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        sqlite3_result_error(context, "invalid MyLite Big5 LEFT callback", -1);
        return;
    }
    left_big5_value(context, argv[0], argv[1]);
}

static void integer_cast_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const enum mylite_cast_integer_target *target = NULL;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite CAST/CONVERT integer callback", -1);
        return;
    }

    target = (const enum mylite_cast_integer_target *)sqlite3_user_data(context);
    if (target == NULL) {
        sqlite3_result_error(context, "invalid MyLite CAST/CONVERT integer configuration", -1);
        return;
    }
    integer_cast_value(context, argv[0], *target);
}

static void text_conversion_value(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_cast_text_conversion_mode mode
) {
    const unsigned char *text = NULL;
    int text_length = 0;

    if (sqlite3_value_type(value) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    text = sqlite3_value_blob(value);
    text_length = sqlite3_value_bytes(value);
    if ((text == NULL && text_length != 0) || text_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (mode == MYLITE_CAST_TEXT_CONVERSION_TEXT_ASCII) {
        for (int index = 0; index < text_length; ++index) {
            if (text[index] > utf8_ascii_max) {
                sqlite3_result_error(
                    context,
                    "CONVERT USING latin1 supports only ASCII scalar values",
                    -1
                );
                return;
            }
        }
    }

    if (mode == MYLITE_CAST_TEXT_CONVERSION_BYTES ||
        mode == MYLITE_CAST_TEXT_CONVERSION_BYTES_BIG5) {
        const unsigned char *result = text == NULL ? (const unsigned char *)"" : text;
        int result_length = text_length;

        if (mode == MYLITE_CAST_TEXT_CONVERSION_BYTES_BIG5) {
            result_length = big5_trim_trailing_partial_character(text, text_length);
        }
        sqlite3_result_blob(context, result, result_length, SQLITE_TRANSIENT);
        return;
    }
    sqlite3_result_text(
        context,
        text == NULL ? "" : (const char *)text,
        text_length,
        SQLITE_TRANSIENT
    );
}

static void left_big5_value(sqlite3_context *context, sqlite3_value *value, sqlite3_value *length) {
    const unsigned char *text = NULL;
    int text_length = 0;
    sqlite3_int64 char_length = 0;
    int result_length = 0;

    if (sqlite3_value_type(value) == SQLITE_NULL || sqlite3_value_type(length) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    text = sqlite3_value_blob(value);
    text_length = sqlite3_value_bytes(value);
    if ((text == NULL && text_length != 0) || text_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }

    char_length = sqlite3_value_int64(length);
    if (char_length <= 0) {
        sqlite3_result_blob(context, "", 0, SQLITE_STATIC);
        return;
    }
    result_length = big5_left_byte_count(text, text_length, char_length);
    sqlite3_result_blob(
        context,
        text == NULL ? "" : (const char *)text,
        result_length,
        SQLITE_TRANSIENT
    );
}

static int big5_left_byte_count(const unsigned char *text, int text_length, int64_t char_length) {
    int offset = 0;
    int64_t characters = 0;

    while (offset < text_length && characters < char_length) {
        if (big5_byte_is_lead(text[offset]) && offset + 1 < text_length) {
            offset += 2;
        } else {
            ++offset;
        }
        ++characters;
    }
    return offset;
}

static bool big5_byte_is_lead(unsigned char byte) {
    return byte >= big5_lead_byte_min && byte <= big5_lead_byte_max;
}

static int big5_complete_prefix_byte_count(const unsigned char *text, int text_length) {
    int offset = 0;

    while (offset < text_length) {
        if (big5_byte_is_lead(text[offset])) {
            if (offset + 1 >= text_length) {
                return offset;
            }
            offset += 2;
        } else {
            ++offset;
        }
    }
    return offset;
}

static int big5_trim_trailing_partial_character(const unsigned char *text, int text_length) {
    if (text == NULL || text_length <= 0) {
        return text_length;
    }
    return big5_complete_prefix_byte_count(text, text_length);
}

static void integer_cast_value(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_cast_integer_target target
) {
    const unsigned char *text = NULL;
    int text_length = 0;

    if (sqlite3_value_type(value) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (sqlite3_value_type(value) == SQLITE_INTEGER) {
        integer_cast_numeric_value(
            context,
            &(const struct integer_cast_numeric_request){
                .value = sqlite3_value_int64(value),
                .target = target,
            }
        );
        return;
    }

    text = sqlite3_value_text(value);
    text_length = sqlite3_value_bytes(value);
    if (text == NULL || text_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }
    integer_cast_text_value(
        context,
        &(const struct integer_cast_text_request){
            .value = (const char *)text,
            .value_length = (size_t)text_length,
            .target = target,
        }
    );
}

static int append_truncated_integer_warning(
    struct mylite_db *database,
    const char *value,
    size_t value_length
) {
    static const char prefix[] = "Truncated incorrect INTEGER value: '";
    static const char suffix[] = "'";
    char *message = NULL;
    size_t message_length = 0U;
    int rc = MYLITE_OK;

    if (database == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (value_length > SIZE_MAX - sizeof(prefix) - sizeof(suffix)) {
        return MYLITE_NOMEM;
    }
    message_length = (sizeof(prefix) - 1U) + value_length + (sizeof(suffix) - 1U);
    message = (char *)malloc(message_length + 1U);
    if (message == NULL) {
        return MYLITE_NOMEM;
    }

    memcpy(message, prefix, sizeof(prefix) - 1U);
    memcpy(message + sizeof(prefix) - 1U, value, value_length);
    memcpy(message + sizeof(prefix) - 1U + value_length, suffix, sizeof(suffix));

    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_incorrect_integer,
        "22007",
        message
    );
    free(message);
    return rc;
}

static int append_signed_complement_warning(struct mylite_db *database) {
    if (database == NULL) {
        return MYLITE_MISUSE;
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_cast_complement,
        "HY000",
        "Cast to signed converted positive out-of-range integer to its negative complement"
    );
}

static int append_unsigned_complement_warning(struct mylite_db *database) {
    if (database == NULL) {
        return MYLITE_MISUSE;
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_cast_complement,
        "HY000",
        "Cast to unsigned converted negative integer to its positive complement"
    );
}

static void integer_cast_numeric_value(
    sqlite3_context *context,
    const struct integer_cast_numeric_request *request
) {
    if (request->target == MYLITE_CAST_INTEGER_SIGNED) {
        finish_int64_result(context, request->value);
        return;
    }
    finish_uint64_result(context, (uint64_t)request->value);
}

static void integer_cast_text_value(
    sqlite3_context *context,
    const struct integer_cast_text_request *request
) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    struct row_integer_cast_parse parse = {
        .is_negative = false,
        .saw_digits = false,
        .overflowed = false,
        .has_truncated_integer_warning = false,
        .magnitude = 0U,
    };
    int rc = MYLITE_OK;

    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite connection for CAST/CONVERT", -1);
        return;
    }

    parse_row_integer_cast_text(request->value, request->value_length, &parse);
    if (parse.has_truncated_integer_warning) {
        rc = append_truncated_integer_warning(database, request->value, request->value_length);
        if (rc != MYLITE_OK) {
            set_warning_append_error(context, rc);
            return;
        }
    }

    if (request->target == MYLITE_CAST_INTEGER_SIGNED) {
        if (!parse.is_negative && parse.magnitude > (uint64_t)INT64_MAX && !parse.overflowed) {
            rc = append_signed_complement_warning(database);
            if (rc != MYLITE_OK) {
                set_warning_append_error(context, rc);
                return;
            }
        }
        finish_signed_integer_cast_result(context, parse.is_negative, parse.magnitude);
        return;
    }

    if (parse.is_negative && parse.saw_digits && !parse.overflowed) {
        rc = append_unsigned_complement_warning(database);
        if (rc != MYLITE_OK) {
            set_warning_append_error(context, rc);
            return;
        }
    }
    finish_unsigned_integer_cast_result(context, parse.is_negative, parse.magnitude);
}

static void parse_row_integer_cast_text(
    const char *text,
    size_t text_length,
    struct row_integer_cast_parse *out_parse
) {
    size_t offset = 0U;
    size_t digit_end = 0U;
    uint64_t limit = UINT64_MAX;
    const uint64_t int64_min_magnitude = (uint64_t)INT64_MAX + 1U;

    *out_parse = (struct row_integer_cast_parse){
        .is_negative = false,
        .saw_digits = false,
        .overflowed = false,
        .has_truncated_integer_warning = false,
        .magnitude = 0U,
    };
    while (offset < text_length && row_integer_cast_is_ascii_space((unsigned char)text[offset])) {
        ++offset;
    }
    if (offset < text_length && (text[offset] == '+' || text[offset] == '-')) {
        out_parse->is_negative = text[offset] == '-';
        ++offset;
    }
    if (out_parse->is_negative) {
        limit = int64_min_magnitude;
    }
    parse_row_integer_cast_digits(
        &(const struct row_integer_cast_digit_scan){
            .text = text,
            .text_length = text_length,
            .offset = offset,
            .limit = limit,
        },
        out_parse,
        &digit_end
    );
    if (!out_parse->saw_digits) {
        out_parse->has_truncated_integer_warning = true;
        out_parse->magnitude = 0U;
        return;
    }

    offset = digit_end;
    while (offset < text_length && row_integer_cast_is_ascii_space((unsigned char)text[offset])) {
        ++offset;
    }
    if (offset != text_length) {
        out_parse->has_truncated_integer_warning = true;
    }
}

static void parse_row_integer_cast_digits(
    const struct row_integer_cast_digit_scan *scan,
    struct row_integer_cast_parse *inout_parse,
    size_t *out_end_offset
) {
    size_t offset = scan->offset;

    while (offset < scan->text_length && scan->text[offset] >= '0' && scan->text[offset] <= '9') {
        uint64_t digit = (uint64_t)(scan->text[offset] - '0');

        inout_parse->saw_digits = true;
        if (!inout_parse->overflowed &&
            inout_parse->magnitude > (scan->limit - digit) / decimal_base) {
            inout_parse->overflowed = true;
            inout_parse->has_truncated_integer_warning = true;
            inout_parse->magnitude = scan->limit;
        } else if (!inout_parse->overflowed) {
            inout_parse->magnitude = (inout_parse->magnitude * decimal_base) + digit;
        }
        ++offset;
    }

    if (out_end_offset != NULL) {
        *out_end_offset = offset;
    }
}

static bool row_integer_cast_is_ascii_space(unsigned char byte) {
    return isspace(byte) != 0;
}

static void finish_signed_integer_cast_result(
    sqlite3_context *context,
    bool is_negative,
    uint64_t magnitude
) {
    const uint64_t int64_min_magnitude = (uint64_t)INT64_MAX + 1U;

    if (is_negative) {
        if (magnitude >= int64_min_magnitude) {
            finish_int64_result(context, INT64_MIN);
            return;
        }
        finish_int64_result(context, -(int64_t)magnitude);
        return;
    }
    if (magnitude <= (uint64_t)INT64_MAX) {
        finish_int64_result(context, (int64_t)magnitude);
        return;
    }
    {
        uint64_t complement = UINT64_MAX - magnitude + 1U;

        if (complement >= int64_min_magnitude) {
            finish_int64_result(context, INT64_MIN);
            return;
        }
        finish_int64_result(context, -(int64_t)complement);
    }
}

static void finish_unsigned_integer_cast_result(
    sqlite3_context *context,
    bool is_negative,
    uint64_t magnitude
) {
    if (is_negative) {
        if (magnitude == 0U) {
            finish_uint64_result(context, 0U);
            return;
        }
        finish_uint64_result(context, UINT64_MAX - magnitude + 1U);
        return;
    }
    finish_uint64_result(context, magnitude);
}

static void finish_int64_result(sqlite3_context *context, int64_t value) {
    sqlite3_result_int64(context, value);
}

static void finish_uint64_result(sqlite3_context *context, uint64_t value) {
    char buffer[sizeof("18446744073709551615")];
    int written = 0;

    if (value <= (uint64_t)INT64_MAX) {
        sqlite3_result_int64(context, (int64_t)value);
        return;
    }

    written = snprintf(buffer, sizeof(buffer), "%" PRIu64, value);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        sqlite3_result_error(context, "failed to format MyLite unsigned CAST/CONVERT result", -1);
        return;
    }
    sqlite3_result_text(context, buffer, written, SQLITE_TRANSIENT);
}

static void set_warning_append_error(sqlite3_context *context, int rc) {
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    sqlite3_result_error(context, "failed to append MyLite CAST/CONVERT warning", -1);
}
