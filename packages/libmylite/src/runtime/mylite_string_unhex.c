#include "mylite_string_unhex.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    mysql_warning_incorrect_string_value = 1411,
    unhex_alpha_digit_offset = 10,
    unhex_decimal_text_capacity = 32,
    unhex_printable_ascii_min = 0x20,
    unhex_printable_ascii_max = 0x7e,
    unhex_warning_input_capacity = 96,
};

static void unhex_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void unhex_sqlite_decode(sqlite3_context *context, const void *input, size_t input_size);
static int append_incorrect_warning_message(struct mylite_db *database, const char *input_text);
static int hex_digit_value(unsigned char byte, unsigned char *out_value);
static bool warning_input_byte_is_printable(unsigned char byte);

int mylite_string_unhex_decode(
    const void *input,
    size_t input_size,
    unsigned char **out_bytes,
    size_t *out_size,
    bool *out_valid
) {
    const unsigned char *bytes = input;
    unsigned char *decoded = NULL;
    size_t output_size = 0U;
    size_t input_index = 0U;
    size_t output_index = 0U;

    if ((input == NULL && input_size != 0U) || out_bytes == NULL || out_size == NULL ||
        out_valid == NULL) {
        return MYLITE_MISUSE;
    }

    *out_bytes = NULL;
    *out_size = 0U;
    *out_valid = false;

    for (size_t index = 0U; index < input_size; ++index) {
        unsigned char ignored = 0U;

        if (hex_digit_value(bytes[index], &ignored) != MYLITE_OK) {
            return MYLITE_OK;
        }
    }

    if (input_size == SIZE_MAX) {
        return MYLITE_NOMEM;
    }
    output_size = (input_size + 1U) / 2U;
    decoded = (unsigned char *)malloc(output_size + 1U);
    if (decoded == NULL) {
        return MYLITE_NOMEM;
    }

    if ((input_size % 2U) != 0U) {
        unsigned char low = 0U;

        (void)hex_digit_value(bytes[0], &low);
        decoded[0] = low;
        input_index = 1U;
        output_index = 1U;
    }

    while (input_index < input_size) {
        unsigned char high = 0U;
        unsigned char low = 0U;

        (void)hex_digit_value(bytes[input_index], &high);
        (void)hex_digit_value(bytes[input_index + 1U], &low);
        decoded[output_index] = (unsigned char)((high << 4U) | low);
        input_index += 2U;
        ++output_index;
    }
    decoded[output_size] = '\0';

    *out_bytes = decoded;
    *out_size = output_size;
    *out_valid = true;
    return MYLITE_OK;
}

int mylite_string_unhex_append_incorrect_warning(
    struct mylite_db *database,
    const void *input,
    size_t input_size
) {
    char input_text[unhex_warning_input_capacity];
    int rc = MYLITE_OK;

    if (database == NULL || (input == NULL && input_size != 0U)) {
        return MYLITE_MISUSE;
    }

    rc =
        mylite_string_unhex_format_warning_input(input, input_size, input_text, sizeof(input_text));
    if (rc != MYLITE_OK) {
        return rc;
    }
    return append_incorrect_warning_message(database, input_text);
}

int mylite_string_unhex_format_warning_input(
    const void *input,
    size_t input_size,
    char *destination,
    size_t destination_size
) {
    const unsigned char *bytes = input;
    size_t limit = 0U;

    if ((input == NULL && input_size != 0U) || destination == NULL || destination_size == 0U) {
        return MYLITE_MISUSE;
    }

    limit = input_size;
    if (limit > destination_size - 1U) {
        limit = destination_size - 1U;
    }
    for (size_t index = 0U; index < limit; ++index) {
        unsigned char byte = bytes[index];

        if (warning_input_byte_is_printable(byte)) {
            destination[index] = (char)byte;
        } else {
            destination[index] = '?';
        }
    }
    destination[limit] = '\0';

    return MYLITE_OK;
}

int mylite_sqlite_register_string_unhex_function(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_unhex",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = unhex_sqlite_callback,
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

static void unhex_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    int value_type = SQLITE_NULL;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite UNHEX callback", -1);
        return;
    }

    value_type = sqlite3_value_type(argv[0]);
    if (value_type == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (value_type == SQLITE_INTEGER) {
        char decimal_text[unhex_decimal_text_capacity];
        int written = snprintf(
            decimal_text,
            sizeof(decimal_text),
            "%" PRId64,
            (int64_t)sqlite3_value_int64(argv[0])
        );

        if (written < 0 || (size_t)written >= sizeof(decimal_text)) {
            sqlite3_result_error(context, "failed to format MyLite UNHEX integer", -1);
            return;
        }
        unhex_sqlite_decode(context, decimal_text, (size_t)written);
        return;
    }
    if (value_type == SQLITE_TEXT || value_type == SQLITE_BLOB) {
        const void *bytes = sqlite3_value_blob(argv[0]);
        int byte_count = sqlite3_value_bytes(argv[0]);

        if ((bytes == NULL && byte_count != 0) || byte_count < 0) {
            sqlite3_result_error_nomem(context);
            return;
        }
        unhex_sqlite_decode(context, bytes, (size_t)byte_count);
        return;
    }

    sqlite3_result_null(context);
}

static void unhex_sqlite_decode(sqlite3_context *context, const void *input, size_t input_size) {
    struct mylite_db *database = NULL;
    unsigned char *decoded = NULL;
    size_t decoded_size = 0U;
    bool valid = false;
    int rc = MYLITE_OK;

    database = mylite_sqlite_bootstrap_owner_from_context(context);
    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite UNHEX owner", -1);
        return;
    }

    rc = mylite_string_unhex_decode(input, input_size, &decoded, &decoded_size, &valid);
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "MyLite UNHEX decode failed", -1);
        return;
    }
    if (!valid) {
        rc = mylite_string_unhex_append_incorrect_warning(database, input, input_size);
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
            return;
        }
        if (rc != MYLITE_OK) {
            sqlite3_result_error(context, "MyLite UNHEX warning failed", -1);
            return;
        }
        sqlite3_result_null(context);
        return;
    }
    if (decoded_size > (size_t)INT_MAX) {
        free(decoded);
        sqlite3_result_error(context, "MyLite UNHEX result is too large", -1);
        return;
    }

    sqlite3_result_blob(context, decoded, (int)decoded_size, SQLITE_TRANSIENT);
    free(decoded);
}

static int append_incorrect_warning_message(struct mylite_db *database, const char *input_text) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = 0;
    int rc = MYLITE_OK;

    if (database == NULL || input_text == NULL) {
        return MYLITE_MISUSE;
    }

    written = snprintf(
        message,
        sizeof(message),
        "Incorrect string value: ''%s'' for function unhex",
        input_text
    );
    if (written < 0 || (size_t)written >= sizeof(message)) {
        return MYLITE_ERROR;
    }

    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_incorrect_string_value,
        "HY000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_NOMEM,
            "HY001",
            "out of memory while recording UNHEX() warning"
        );
    }
    return rc;
}

static int hex_digit_value(unsigned char byte, unsigned char *out_value) {
    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (byte >= '0' && byte <= '9') {
        *out_value = (unsigned char)(byte - '0');
        return MYLITE_OK;
    }
    if (byte >= 'A' && byte <= 'F') {
        *out_value = (unsigned char)(byte - 'A' + unhex_alpha_digit_offset);
        return MYLITE_OK;
    }
    if (byte >= 'a' && byte <= 'f') {
        *out_value = (unsigned char)(byte - 'a' + unhex_alpha_digit_offset);
        return MYLITE_OK;
    }
    return MYLITE_ERROR;
}

static bool warning_input_byte_is_printable(unsigned char byte) {
    if (byte < unhex_printable_ascii_min) {
        return false;
    }
    if (byte > unhex_printable_ascii_max) {
        return false;
    }
    return true;
}
