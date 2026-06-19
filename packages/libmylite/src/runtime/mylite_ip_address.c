#include "mylite_ip_address.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_warning_truncated_wrong_value = 1292,
    mysql_warning_incorrect_value = 1411,
    ip_address_decimal_text_capacity = 32,
    ip_address_warning_input_capacity = 96,
    ip_address_binary_warning_hex_capacity = 192,
    ip_address_octet_count = 4,
    ip_address_octet_max = 255,
    ip_address_decimal_base = 10,
    ip_address_octet_0_shift = 24,
    ip_address_octet_1_shift = 16,
    ip_address_octet_2_shift = 8,
    ip_address_octet_mask = 0xff,
    ip_address_hex_nibble_shift = 4,
    ip_address_hex_nibble_mask = 0x0f,
    ip_address_printable_ascii_min = 0x20,
    ip_address_printable_ascii_max = 0x7e,
    ip_address_printable_ascii_count =
        ip_address_printable_ascii_max - ip_address_printable_ascii_min + 1,
};

static const double ip_address_real_round_half = 0.5;

static void inet_aton_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void inet_ntoa_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void inet_aton_sqlite_value(sqlite3_context *context, const void *input, size_t input_size);
static void inet_ntoa_sqlite_text_value(
    sqlite3_context *context,
    const void *input,
    size_t input_size
);
static void inet_ntoa_sqlite_real_value(sqlite3_context *context, double value, const char *text);
static int append_inet_aton_warning_message(struct mylite_db *database, const char *input_text);
static int append_inet_ntoa_range_warning_message(
    struct mylite_db *database,
    const char *input_text
);
static int append_inet_ntoa_truncated_integer_warning_message(
    struct mylite_db *database,
    const char *input_text
);
static int append_inet_ntoa_binary_warning_message(
    struct mylite_db *database,
    const char *input_text
);
static int format_printable_warning_input(
    const void *input,
    size_t input_size,
    char *destination,
    size_t destination_size
);
static int format_binary_warning_input(
    const void *input,
    size_t input_size,
    char *destination,
    size_t destination_size
);
static char warning_input_printable_byte(unsigned char byte);
static bool ascii_space(unsigned char byte);

int mylite_ip_address_parse_inet_aton(
    const void *input,
    size_t input_size,
    uint32_t *out_value,
    bool *out_valid
) {
    const unsigned char *bytes = input;
    unsigned int components[ip_address_octet_count] = {0U, 0U, 0U, 0U};
    size_t component_count = 0U;
    unsigned int current = 0U;
    bool has_digit = false;

    if ((input == NULL && input_size != 0U) || out_value == NULL || out_valid == NULL) {
        return MYLITE_MISUSE;
    }

    *out_value = 0U;
    *out_valid = false;
    if (input_size == 0U) {
        return MYLITE_OK;
    }

    for (size_t index = 0U; index < input_size; ++index) {
        unsigned char byte = bytes[index];

        if (byte >= '0' && byte <= '9') {
            current = (current * ip_address_decimal_base) + (unsigned int)(byte - '0');
            if (current > ip_address_octet_max) {
                return MYLITE_OK;
            }
            has_digit = true;
            continue;
        }
        if (byte != '.') {
            return MYLITE_OK;
        }
        if (index + 1U == input_size || component_count >= ip_address_octet_count) {
            return MYLITE_OK;
        }

        components[component_count] = has_digit ? current : 0U;
        ++component_count;
        current = 0U;
        has_digit = false;
    }

    if (!has_digit || component_count >= ip_address_octet_count) {
        return MYLITE_OK;
    }
    components[component_count] = current;
    ++component_count;

    switch (component_count) {
    case 1U:
        *out_value = components[0];
        break;
    case 2U:
        *out_value = (components[0] << ip_address_octet_0_shift) | components[1];
        break;
    case 3U:
        *out_value = (components[0] << ip_address_octet_0_shift) |
                     (components[1] << ip_address_octet_1_shift) | components[2];
        break;
    case 4U:
        *out_value = (components[0] << ip_address_octet_0_shift) |
                     (components[1] << ip_address_octet_1_shift) |
                     (components[2] << ip_address_octet_2_shift) | components[3];
        break;
    default:
        return MYLITE_OK;
    }

    *out_valid = true;
    return MYLITE_OK;
}

void mylite_ip_address_format_inet_ntoa(
    uint32_t value,
    char out_text[mylite_ip_address_text_capacity]
) {
    if (out_text == NULL) {
        return;
    }

    snprintf(
        out_text,
        mylite_ip_address_text_capacity,
        "%u.%u.%u.%u",
        (unsigned int)((value >> ip_address_octet_0_shift) & ip_address_octet_mask),
        (unsigned int)((value >> ip_address_octet_1_shift) & ip_address_octet_mask),
        (unsigned int)((value >> ip_address_octet_2_shift) & ip_address_octet_mask),
        (unsigned int)(value & ip_address_octet_mask)
    );
}

int mylite_ip_address_parse_inet_ntoa_integer_text(
    const void *input,
    size_t input_size,
    uint32_t *out_value,
    bool *out_out_of_range,
    bool *out_truncated
) {
    const unsigned char *bytes = input;
    size_t index = 0U;
    uint64_t value = 0U;
    bool negative = false;
    bool saw_digit = false;
    bool overflow = false;

    if ((input == NULL && input_size != 0U) || out_value == NULL || out_out_of_range == NULL ||
        out_truncated == NULL) {
        return MYLITE_MISUSE;
    }

    *out_value = 0U;
    *out_out_of_range = false;
    *out_truncated = false;

    while (index < input_size && ascii_space(bytes[index])) {
        ++index;
    }
    if (index < input_size && (bytes[index] == '+' || bytes[index] == '-')) {
        negative = bytes[index] == '-';
        ++index;
    }

    while (index < input_size && bytes[index] >= '0' && bytes[index] <= '9') {
        unsigned int digit = (unsigned int)(bytes[index] - '0');

        saw_digit = true;
        if (value > ((uint64_t)UINT32_MAX - digit) / ip_address_decimal_base) {
            overflow = true;
        } else {
            value = (value * ip_address_decimal_base) + digit;
        }
        ++index;
    }

    if (!saw_digit) {
        *out_truncated = true;
        return MYLITE_OK;
    }

    while (index < input_size && ascii_space(bytes[index])) {
        ++index;
    }
    if (index < input_size) {
        *out_truncated = true;
    }
    if (overflow || (negative && value != 0U)) {
        *out_out_of_range = true;
        return MYLITE_OK;
    }

    *out_value = (uint32_t)value;
    return MYLITE_OK;
}

int mylite_ip_address_round_inet_ntoa_real(
    double value,
    uint32_t *out_value,
    bool *out_out_of_range
) {
    uint64_t rounded = 0U;

    if (out_value == NULL || out_out_of_range == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0U;
    *out_out_of_range = false;

    if (value != value || value <= -ip_address_real_round_half ||
        value >= ((double)UINT32_MAX + ip_address_real_round_half)) {
        *out_out_of_range = true;
        return MYLITE_OK;
    }
    if (value < 0.0) {
        return MYLITE_OK;
    }

    rounded = (uint64_t)(value + ip_address_real_round_half);
    if (rounded > UINT32_MAX) {
        *out_out_of_range = true;
        return MYLITE_OK;
    }

    *out_value = (uint32_t)rounded;
    return MYLITE_OK;
}

int mylite_ip_address_append_inet_aton_warning(
    struct mylite_db *database,
    const void *input,
    size_t input_size
) {
    char input_text[ip_address_warning_input_capacity];
    int rc = MYLITE_OK;

    if (database == NULL || (input == NULL && input_size != 0U)) {
        return MYLITE_MISUSE;
    }

    rc = format_printable_warning_input(input, input_size, input_text, sizeof(input_text));
    if (rc != MYLITE_OK) {
        return rc;
    }
    return append_inet_aton_warning_message(database, input_text);
}

int mylite_ip_address_append_inet_ntoa_range_warning(
    struct mylite_db *database,
    const void *input,
    size_t input_size
) {
    char input_text[ip_address_warning_input_capacity];
    int rc = MYLITE_OK;

    if (database == NULL || (input == NULL && input_size != 0U)) {
        return MYLITE_MISUSE;
    }

    rc = format_printable_warning_input(input, input_size, input_text, sizeof(input_text));
    if (rc != MYLITE_OK) {
        return rc;
    }
    return append_inet_ntoa_range_warning_message(database, input_text);
}

int mylite_ip_address_append_inet_ntoa_truncated_integer_warning(
    struct mylite_db *database,
    const void *input,
    size_t input_size
) {
    char input_text[ip_address_warning_input_capacity];
    int rc = MYLITE_OK;

    if (database == NULL || (input == NULL && input_size != 0U)) {
        return MYLITE_MISUSE;
    }

    rc = format_printable_warning_input(input, input_size, input_text, sizeof(input_text));
    if (rc != MYLITE_OK) {
        return rc;
    }
    return append_inet_ntoa_truncated_integer_warning_message(database, input_text);
}

int mylite_ip_address_append_inet_ntoa_binary_warning(
    struct mylite_db *database,
    const void *input,
    size_t input_size
) {
    char input_text[ip_address_binary_warning_hex_capacity + sizeof("x''")];
    int rc = MYLITE_OK;

    if (database == NULL || (input == NULL && input_size != 0U)) {
        return MYLITE_MISUSE;
    }

    rc = format_binary_warning_input(input, input_size, input_text, sizeof(input_text));
    if (rc != MYLITE_OK) {
        return rc;
    }
    return append_inet_ntoa_binary_warning_message(database, input_text);
}

int mylite_sqlite_register_ip_address_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_inet_aton",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = inet_aton_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_inet_ntoa",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = inet_ntoa_sqlite_callback,
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

static void inet_aton_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    int value_type = SQLITE_NULL;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite INET_ATON callback", -1);
        return;
    }

    value_type = sqlite3_value_type(argv[0]);
    if (value_type == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (value_type == SQLITE_INTEGER) {
        char decimal_text[ip_address_decimal_text_capacity];
        int written = snprintf(
            decimal_text,
            sizeof(decimal_text),
            "%" PRId64,
            (int64_t)sqlite3_value_int64(argv[0])
        );

        if (written < 0 || (size_t)written >= sizeof(decimal_text)) {
            sqlite3_result_error(context, "failed to format MyLite INET_ATON integer", -1);
            return;
        }
        inet_aton_sqlite_value(context, decimal_text, (size_t)written);
        return;
    }
    if (value_type == SQLITE_FLOAT) {
        const unsigned char *text = sqlite3_value_text(argv[0]);
        int byte_count = sqlite3_value_bytes(argv[0]);

        if ((text == NULL && byte_count != 0) || byte_count < 0) {
            sqlite3_result_error_nomem(context);
            return;
        }
        inet_aton_sqlite_value(context, text, (size_t)byte_count);
        return;
    }
    if (value_type == SQLITE_TEXT || value_type == SQLITE_BLOB) {
        const void *bytes = sqlite3_value_blob(argv[0]);
        int byte_count = sqlite3_value_bytes(argv[0]);

        if ((bytes == NULL && byte_count != 0) || byte_count < 0) {
            sqlite3_result_error_nomem(context);
            return;
        }
        inet_aton_sqlite_value(context, bytes, (size_t)byte_count);
        return;
    }

    sqlite3_result_null(context);
}

static void inet_aton_sqlite_value(sqlite3_context *context, const void *input, size_t input_size) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    uint32_t value = 0U;
    bool valid = false;
    int rc = MYLITE_OK;

    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite INET_ATON owner", -1);
        return;
    }

    rc = mylite_ip_address_parse_inet_aton(input, input_size, &value, &valid);
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "MyLite INET_ATON parse failed", -1);
        return;
    }
    if (!valid) {
        rc = mylite_ip_address_append_inet_aton_warning(database, input, input_size);
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
            return;
        }
        if (rc != MYLITE_OK) {
            sqlite3_result_error(context, "MyLite INET_ATON warning failed", -1);
            return;
        }
        sqlite3_result_null(context);
        return;
    }

    sqlite3_result_int64(context, (sqlite3_int64)value);
}

static void inet_ntoa_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    int value_type = SQLITE_NULL;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite INET_NTOA callback", -1);
        return;
    }

    value_type = sqlite3_value_type(argv[0]);
    if (value_type == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (value_type == SQLITE_INTEGER) {
        int64_t integer = sqlite3_value_int64(argv[0]);
        char text[ip_address_decimal_text_capacity];
        uint32_t value = 0U;
        int written = snprintf(text, sizeof(text), "%" PRId64, integer);

        if (written < 0 || (size_t)written >= sizeof(text)) {
            sqlite3_result_error(context, "failed to format MyLite INET_NTOA integer", -1);
            return;
        }
        if (integer < 0 || integer > (int64_t)UINT32_MAX) {
            struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
            int rc = MYLITE_OK;

            if (database == NULL) {
                sqlite3_result_error(context, "missing MyLite INET_NTOA owner", -1);
                return;
            }
            rc = mylite_ip_address_append_inet_ntoa_range_warning(database, text, (size_t)written);
            if (rc == MYLITE_NOMEM) {
                sqlite3_result_error_nomem(context);
                return;
            }
            if (rc != MYLITE_OK) {
                sqlite3_result_error(context, "MyLite INET_NTOA warning failed", -1);
                return;
            }
            sqlite3_result_null(context);
            return;
        }

        value = (uint32_t)integer;
        mylite_ip_address_format_inet_ntoa(value, text);
        sqlite3_result_text(context, text, -1, SQLITE_TRANSIENT);
        return;
    }
    if (value_type == SQLITE_FLOAT) {
        const unsigned char *text = sqlite3_value_text(argv[0]);

        inet_ntoa_sqlite_real_value(
            context,
            sqlite3_value_double(argv[0]),
            text == NULL ? "" : (const char *)text
        );
        return;
    }
    if (value_type == SQLITE_TEXT) {
        const unsigned char *text = sqlite3_value_text(argv[0]);
        int byte_count = sqlite3_value_bytes(argv[0]);

        if ((text == NULL && byte_count != 0) || byte_count < 0) {
            sqlite3_result_error_nomem(context);
            return;
        }
        inet_ntoa_sqlite_text_value(context, text, (size_t)byte_count);
        return;
    }
    if (value_type == SQLITE_BLOB) {
        struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
        const void *bytes = sqlite3_value_blob(argv[0]);
        int byte_count = sqlite3_value_bytes(argv[0]);
        char text[mylite_ip_address_text_capacity];
        int rc = MYLITE_OK;

        if (database == NULL) {
            sqlite3_result_error(context, "missing MyLite INET_NTOA owner", -1);
            return;
        }
        if ((bytes == NULL && byte_count != 0) || byte_count < 0) {
            sqlite3_result_error_nomem(context);
            return;
        }
        rc = mylite_ip_address_append_inet_ntoa_binary_warning(database, bytes, (size_t)byte_count);
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
            return;
        }
        if (rc != MYLITE_OK) {
            sqlite3_result_error(context, "MyLite INET_NTOA warning failed", -1);
            return;
        }
        mylite_ip_address_format_inet_ntoa(0U, text);
        sqlite3_result_text(context, text, -1, SQLITE_TRANSIENT);
        return;
    }

    sqlite3_result_null(context);
}

static void inet_ntoa_sqlite_text_value(
    sqlite3_context *context,
    const void *input,
    size_t input_size
) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    char text[mylite_ip_address_text_capacity];
    uint32_t value = 0U;
    bool out_of_range = false;
    bool truncated = false;
    int rc = MYLITE_OK;

    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite INET_NTOA owner", -1);
        return;
    }

    rc = mylite_ip_address_parse_inet_ntoa_integer_text(
        input,
        input_size,
        &value,
        &out_of_range,
        &truncated
    );
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "MyLite INET_NTOA parse failed", -1);
        return;
    }
    if (truncated) {
        rc = mylite_ip_address_append_inet_ntoa_truncated_integer_warning(
            database,
            input,
            input_size
        );
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
            return;
        }
        if (rc != MYLITE_OK) {
            sqlite3_result_error(context, "MyLite INET_NTOA warning failed", -1);
            return;
        }
    }
    if (out_of_range) {
        rc = mylite_ip_address_append_inet_ntoa_range_warning(database, input, input_size);
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
            return;
        }
        if (rc != MYLITE_OK) {
            sqlite3_result_error(context, "MyLite INET_NTOA warning failed", -1);
            return;
        }
        sqlite3_result_null(context);
        return;
    }

    mylite_ip_address_format_inet_ntoa(value, text);
    sqlite3_result_text(context, text, -1, SQLITE_TRANSIENT);
}

static void inet_ntoa_sqlite_real_value(sqlite3_context *context, double value, const char *text) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    char output[mylite_ip_address_text_capacity];
    uint32_t rounded = 0U;
    bool out_of_range = false;
    int rc = MYLITE_OK;

    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite INET_NTOA owner", -1);
        return;
    }

    rc = mylite_ip_address_round_inet_ntoa_real(value, &rounded, &out_of_range);
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "MyLite INET_NTOA parse failed", -1);
        return;
    }
    if (out_of_range) {
        rc = mylite_ip_address_append_inet_ntoa_range_warning(
            database,
            text,
            text == NULL ? 0U : strlen(text)
        );
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
            return;
        }
        if (rc != MYLITE_OK) {
            sqlite3_result_error(context, "MyLite INET_NTOA warning failed", -1);
            return;
        }
        sqlite3_result_null(context);
        return;
    }

    mylite_ip_address_format_inet_ntoa(rounded, output);
    sqlite3_result_text(context, output, -1, SQLITE_TRANSIENT);
}

static int append_inet_aton_warning_message(struct mylite_db *database, const char *input_text) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = 0;

    if (database == NULL || input_text == NULL) {
        return MYLITE_MISUSE;
    }

    written = snprintf(
        message,
        sizeof(message),
        "Incorrect string value: ''%s'' for function inet_aton",
        input_text
    );
    if (written < 0 || (size_t)written >= sizeof(message)) {
        return MYLITE_ERROR;
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_incorrect_value,
        "HY000",
        message
    );
}

static int append_inet_ntoa_range_warning_message(
    struct mylite_db *database,
    const char *input_text
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = 0;

    if (database == NULL || input_text == NULL) {
        return MYLITE_MISUSE;
    }

    written = snprintf(
        message,
        sizeof(message),
        "Incorrect integer value: '%s' for function inet_ntoa",
        input_text
    );
    if (written < 0 || (size_t)written >= sizeof(message)) {
        return MYLITE_ERROR;
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_incorrect_value,
        "HY000",
        message
    );
}

static int append_inet_ntoa_truncated_integer_warning_message(
    struct mylite_db *database,
    const char *input_text
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = 0;

    if (database == NULL || input_text == NULL) {
        return MYLITE_MISUSE;
    }

    written =
        snprintf(message, sizeof(message), "Truncated incorrect INTEGER value: '%s'", input_text);
    if (written < 0 || (size_t)written >= sizeof(message)) {
        return MYLITE_ERROR;
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_wrong_value,
        "22007",
        message
    );
}

static int append_inet_ntoa_binary_warning_message(
    struct mylite_db *database,
    const char *input_text
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = 0;

    if (database == NULL || input_text == NULL) {
        return MYLITE_MISUSE;
    }

    written =
        snprintf(message, sizeof(message), "Truncated incorrect BINARY value: '%s'", input_text);
    if (written < 0 || (size_t)written >= sizeof(message)) {
        return MYLITE_ERROR;
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_wrong_value,
        "22007",
        message
    );
}

static int format_printable_warning_input(
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

        destination[index] = warning_input_printable_byte(byte);
    }
    destination[limit] = '\0';
    return MYLITE_OK;
}

static char warning_input_printable_byte(unsigned char byte) {
    static const char printable_ascii[ip_address_printable_ascii_count + 1] =
        " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
        "abcdefghijklmnopqrstuvwxyz{|}~";
    size_t offset = 0U;

    if (byte < ip_address_printable_ascii_min || byte > ip_address_printable_ascii_max) {
        return '?';
    }
    offset = (size_t)byte - ip_address_printable_ascii_min;
    if (offset >= ip_address_printable_ascii_count) {
        return '?';
    }
    return printable_ascii[offset];
}

static int format_binary_warning_input(
    const void *input,
    size_t input_size,
    char *destination,
    size_t destination_size
) {
    static const char hex[] = "0123456789ABCDEF";
    const unsigned char *bytes = input;
    size_t output_index = 0U;
    size_t byte_limit = 0U;

    if ((input == NULL && input_size != 0U) || destination == NULL || destination_size < 4U) {
        return MYLITE_MISUSE;
    }

    destination[output_index++] = 'x';
    destination[output_index++] = '\'';
    byte_limit = input_size;
    if (byte_limit > (destination_size - output_index - 2U) / 2U) {
        byte_limit = (destination_size - output_index - 2U) / 2U;
    }
    for (size_t index = 0U; index < byte_limit; ++index) {
        unsigned char byte = bytes[index];

        destination[output_index++] =
            hex[(byte >> ip_address_hex_nibble_shift) & ip_address_hex_nibble_mask];
        destination[output_index++] = hex[byte & ip_address_hex_nibble_mask];
    }
    destination[output_index++] = '\'';
    destination[output_index] = '\0';
    return MYLITE_OK;
}

static bool ascii_space(unsigned char byte) {
    return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' || byte == '\f' ||
           byte == '\v';
}
