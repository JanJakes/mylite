#include "mylite_uuid.h"

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
#include <time.h>

static const uint64_t uuid_unix_epoch_offset_100ns = 122192928000000000ULL;

enum {
    mysql_error_incorrect_string_value = 1411,
    uuid_hex_digit_count = 32,
    uuid_dashed_text_size = 36,
    uuid_braced_text_size = 38,
    uuid_compact_text_size = 32,
    uuid_warning_input_capacity = 96,
    sqlite_integer_text_capacity = 32,
    uuid_dash_after_time_low = 8,
    uuid_dash_after_time_mid = 13,
    uuid_dash_after_time_high = 18,
    uuid_dash_after_clock_sequence = 23,
    uuid_time_low_offset = 0,
    uuid_time_mid_offset = 4,
    uuid_time_high_offset = 6,
    uuid_tail_offset = 8,
    uuid_node_offset = 10,
    uuid_time_low_size = 4,
    uuid_time_mid_size = 2,
    uuid_time_high_size = 2,
    uuid_tail_size = 8,
    uuid_hex_digits_per_byte = 2,
    uuid_high_nibble_shift = 4,
    uuid_swapped_time_high_offset = 0,
    uuid_swapped_time_mid_offset = 2,
    uuid_swapped_time_low_offset = 4,
    uuid_hex_alpha_offset = 10,
    uuid_low_nibble_mask = 0x0f,
    uuid_printable_minimum = 0x20,
    uuid_printable_maximum = 0x7e,
    uuid_100ns_per_second = 10000000,
    uuid_clock_sequence_mask = 0x3fff,
    uuid_clock_sequence_high_mask = 0x3f,
    uuid_version_time_based = 0x1000,
    uuid_variant_rfc4122 = 0x80,
    uuid_node_multicast_bit = 0x01,
    uuid_time_mid_shift = 32U,
    uuid_time_high_shift = 48U,
    uuid_time_high_mask = 0x0fffU,
    uuid_clock_sequence_high_shift = 8U,
    uuid_uint32_high_shift = 24U,
    uuid_uint32_mid_shift = 16U,
    uuid_uint32_low_shift = 8U,
    uuid_uint16_high_shift = 8U,
};

struct sqlite_uuid_function_values {
    sqlite3_value *argument;
    sqlite3_value *swap_flag;
};

struct sqlite_value_byte_result {
    const void *bytes;
    size_t size;
    bool is_null;
    bool ok;
};

struct uuid_generation_fields {
    uint64_t timestamp_100ns;
    uint16_t clock_sequence;
};

static void uuid_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void is_uuid_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void uuid_to_bin_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void bin_to_uuid_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void uuid_to_bin_sqlite_value(
    sqlite3_context *context,
    struct sqlite_uuid_function_values values
);
static void bin_to_uuid_sqlite_value(
    sqlite3_context *context,
    struct sqlite_uuid_function_values values
);
static struct sqlite_value_byte_result sqlite_value_bytes(
    sqlite3_context *context,
    sqlite3_value *value,
    char decimal_text[sqlite_integer_text_capacity]
);
static bool sqlite_swap_flag(sqlite3_value *value);
static void initialize_uuid_generation_state(struct mylite_db *database);
static uint64_t next_uuid_timestamp_100ns(struct mylite_db *database);
static uint64_t current_uuid_timestamp_100ns(void);
static void populate_uuid_bytes(
    struct uuid_generation_fields fields,
    const unsigned char node[MYLITE_SESSION_UUID_NODE_SIZE],
    unsigned char out_bytes[MYLITE_UUID_BINARY_SIZE]
);
static void store_uuid_uint32(unsigned char *destination, uint32_t value);
static void store_uuid_uint16(unsigned char *destination, uint16_t value);
static bool parse_uuid_string(
    const void *input,
    size_t input_size,
    unsigned char out_bytes[MYLITE_UUID_BINARY_SIZE]
);
static bool parse_dashed_uuid(
    const unsigned char *bytes,
    size_t input_size,
    unsigned char out_bytes[MYLITE_UUID_BINARY_SIZE]
);
static bool parse_compact_uuid(
    const unsigned char *bytes,
    size_t input_size,
    unsigned char out_bytes[MYLITE_UUID_BINARY_SIZE]
);
static void uuid_to_bin_swap(
    const unsigned char input[MYLITE_UUID_BINARY_SIZE],
    unsigned char output[MYLITE_UUID_BINARY_SIZE]
);
static void bin_to_uuid_swap(
    const unsigned char input[MYLITE_UUID_BINARY_SIZE],
    unsigned char output[MYLITE_UUID_BINARY_SIZE]
);
static void format_uuid_string(
    const unsigned char input[MYLITE_UUID_BINARY_SIZE],
    char out_text[MYLITE_UUID_TEXT_SIZE + 1U]
);
static bool hex_pair_value(unsigned char high, unsigned char low, unsigned char *out_value);
static bool hex_digit_value(unsigned char byte, unsigned char *out_value);
static int format_diagnostic_input(
    const void *input,
    size_t input_size,
    char *destination,
    size_t destination_size
);
static bool diagnostic_input_byte_is_printable(unsigned char byte);

bool mylite_uuid_string_is_valid(const void *input, size_t input_size) {
    unsigned char ignored[MYLITE_UUID_BINARY_SIZE];

    return parse_uuid_string(input, input_size, ignored);
}

int mylite_uuid_string_to_binary(
    const void *input,
    size_t input_size,
    bool swap_time_parts,
    unsigned char out_bytes[MYLITE_UUID_BINARY_SIZE],
    bool *out_valid
) {
    unsigned char parsed[MYLITE_UUID_BINARY_SIZE];

    if ((input == NULL && input_size != 0U) || out_bytes == NULL || out_valid == NULL) {
        return MYLITE_MISUSE;
    }

    *out_valid = false;
    memset(out_bytes, 0, MYLITE_UUID_BINARY_SIZE);
    if (!parse_uuid_string(input, input_size, parsed)) {
        return MYLITE_OK;
    }

    if (swap_time_parts) {
        uuid_to_bin_swap(parsed, out_bytes);
    } else {
        memcpy(out_bytes, parsed, MYLITE_UUID_BINARY_SIZE);
    }
    *out_valid = true;
    return MYLITE_OK;
}

int mylite_uuid_binary_to_string(
    const void *input,
    size_t input_size,
    bool swap_time_parts,
    char out_text[MYLITE_UUID_TEXT_SIZE + 1U],
    bool *out_valid
) {
    unsigned char normalized[MYLITE_UUID_BINARY_SIZE];

    if ((input == NULL && input_size != 0U) || out_text == NULL || out_valid == NULL) {
        return MYLITE_MISUSE;
    }

    *out_valid = false;
    out_text[0] = '\0';
    if (input_size != MYLITE_UUID_BINARY_SIZE) {
        return MYLITE_OK;
    }

    if (swap_time_parts) {
        bin_to_uuid_swap((const unsigned char *)input, normalized);
    } else {
        memcpy(normalized, input, MYLITE_UUID_BINARY_SIZE);
    }
    format_uuid_string(normalized, out_text);
    *out_valid = true;
    return MYLITE_OK;
}

int mylite_uuid_set_incorrect_string_error(
    struct mylite_db *database,
    const void *input,
    size_t input_size,
    const char *function_name
) {
    char input_text[uuid_warning_input_capacity];
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = 0;
    int rc = MYLITE_OK;

    if (database == NULL || (input == NULL && input_size != 0U) || function_name == NULL) {
        return MYLITE_MISUSE;
    }

    rc = format_diagnostic_input(input, input_size, input_text, sizeof(input_text));
    if (rc != MYLITE_OK) {
        return rc;
    }
    written = snprintf(
        message,
        sizeof(message),
        "Incorrect string value: '%s' for function %s",
        input_text,
        function_name
    );
    if (written < 0 || (size_t)written >= sizeof(message)) {
        return MYLITE_ERROR;
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_string_value,
        "HY000",
        message
    );
    return MYLITE_ERROR;
}

int mylite_uuid_generate(struct mylite_db *database, char out_text[MYLITE_UUID_TEXT_SIZE + 1U]) {
    unsigned char bytes[MYLITE_UUID_BINARY_SIZE];
    uint64_t timestamp = 0U;

    if (database == NULL || out_text == NULL) {
        return MYLITE_MISUSE;
    }

    if (!database->session.uuid_state_initialized) {
        initialize_uuid_generation_state(database);
    }

    timestamp = next_uuid_timestamp_100ns(database);
    populate_uuid_bytes(
        (struct uuid_generation_fields){
            .timestamp_100ns = timestamp,
            .clock_sequence = database->session.uuid_clock_sequence,
        },
        database->session.uuid_node,
        bytes
    );
    format_uuid_string(bytes, out_text);
    return MYLITE_OK;
}

int mylite_sqlite_register_uuid_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_uuid",
            .argument_count = 0,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = uuid_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_is_uuid",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = is_uuid_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_uuid_to_bin",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = uuid_to_bin_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_uuid_to_bin",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = uuid_to_bin_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_bin_to_uuid",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = bin_to_uuid_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_bin_to_uuid",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = bin_to_uuid_sqlite_callback,
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

static void uuid_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_db *database = NULL;
    char output[MYLITE_UUID_TEXT_SIZE + 1U];
    int rc = MYLITE_OK;

    (void)argv;
    if (context == NULL || argc != 0) {
        sqlite3_result_error(context, "invalid MyLite UUID callback", -1);
        return;
    }

    database = mylite_sqlite_bootstrap_owner_from_context(context);
    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite UUID owner", -1);
        return;
    }

    rc = mylite_uuid_generate(database, output);
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "MyLite UUID failed", -1);
        return;
    }

    sqlite3_result_text(context, output, MYLITE_UUID_TEXT_SIZE, SQLITE_TRANSIENT);
}

static void is_uuid_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    char decimal_text[sqlite_integer_text_capacity];
    struct sqlite_value_byte_result value = {0};

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite IS_UUID callback", -1);
        return;
    }

    value = sqlite_value_bytes(context, argv[0], decimal_text);
    if (!value.ok) {
        return;
    }
    if (value.is_null) {
        sqlite3_result_null(context);
        return;
    }

    sqlite3_result_int(context, (int)mylite_uuid_string_is_valid(value.bytes, value.size));
}

static void uuid_to_bin_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct sqlite_uuid_function_values values = {0};

    if (context == NULL || argc < 1 || argc > 2 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite UUID_TO_BIN callback", -1);
        return;
    }

    values.argument = argv[0];
    values.swap_flag = argc == 2 ? argv[1] : NULL;
    uuid_to_bin_sqlite_value(context, values);
}

static void bin_to_uuid_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct sqlite_uuid_function_values values = {0};

    if (context == NULL || argc < 1 || argc > 2 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite BIN_TO_UUID callback", -1);
        return;
    }

    values.argument = argv[0];
    values.swap_flag = argc == 2 ? argv[1] : NULL;
    bin_to_uuid_sqlite_value(context, values);
}

static void uuid_to_bin_sqlite_value(
    sqlite3_context *context,
    struct sqlite_uuid_function_values values
) {
    struct mylite_db *database = NULL;
    unsigned char output[MYLITE_UUID_BINARY_SIZE];
    char decimal_text[sqlite_integer_text_capacity];
    struct sqlite_value_byte_result value = {0};
    bool valid = false;
    int rc = MYLITE_OK;

    database = mylite_sqlite_bootstrap_owner_from_context(context);
    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite UUID_TO_BIN owner", -1);
        return;
    }

    value = sqlite_value_bytes(context, values.argument, decimal_text);
    if (!value.ok) {
        return;
    }
    if (value.is_null) {
        sqlite3_result_null(context);
        return;
    }

    rc = mylite_uuid_string_to_binary(
        value.bytes,
        value.size,
        sqlite_swap_flag(values.swap_flag),
        output,
        &valid
    );
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "MyLite UUID_TO_BIN failed", -1);
        return;
    }
    if (!valid) {
        (void
        )mylite_uuid_set_incorrect_string_error(database, value.bytes, value.size, "uuid_to_bin");
        sqlite3_result_error(context, "Incorrect string value for UUID_TO_BIN()", -1);
        return;
    }

    sqlite3_result_blob(context, output, MYLITE_UUID_BINARY_SIZE, SQLITE_TRANSIENT);
}

static void bin_to_uuid_sqlite_value(
    sqlite3_context *context,
    struct sqlite_uuid_function_values values
) {
    struct mylite_db *database = NULL;
    char decimal_text[sqlite_integer_text_capacity];
    char output[MYLITE_UUID_TEXT_SIZE + 1U];
    struct sqlite_value_byte_result value = {0};
    bool valid = false;
    int rc = MYLITE_OK;

    database = mylite_sqlite_bootstrap_owner_from_context(context);
    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite BIN_TO_UUID owner", -1);
        return;
    }

    value = sqlite_value_bytes(context, values.argument, decimal_text);
    if (!value.ok) {
        return;
    }
    if (value.is_null) {
        sqlite3_result_null(context);
        return;
    }

    rc = mylite_uuid_binary_to_string(
        value.bytes,
        value.size,
        sqlite_swap_flag(values.swap_flag),
        output,
        &valid
    );
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "MyLite BIN_TO_UUID failed", -1);
        return;
    }
    if (!valid) {
        (void
        )mylite_uuid_set_incorrect_string_error(database, value.bytes, value.size, "bin_to_uuid");
        sqlite3_result_error(context, "Incorrect string value for BIN_TO_UUID()", -1);
        return;
    }

    sqlite3_result_text(context, output, MYLITE_UUID_TEXT_SIZE, SQLITE_TRANSIENT);
}

static struct sqlite_value_byte_result sqlite_value_bytes(
    sqlite3_context *context,
    sqlite3_value *value,
    char decimal_text[sqlite_integer_text_capacity]
) {
    struct sqlite_value_byte_result result = {0};
    int value_type = SQLITE_NULL;

    value_type = sqlite3_value_type(value);
    if (value_type == SQLITE_NULL) {
        result.is_null = true;
        result.ok = true;
        return result;
    }
    if (value_type == SQLITE_INTEGER) {
        int written = snprintf(
            decimal_text,
            sqlite_integer_text_capacity,
            "%" PRId64,
            (int64_t)sqlite3_value_int64(value)
        );

        if (written < 0 || written >= sqlite_integer_text_capacity) {
            sqlite3_result_error(context, "failed to format MyLite UUID integer", -1);
            return result;
        }
        result.bytes = decimal_text;
        result.size = (size_t)written;
        result.ok = true;
        return result;
    }
    if (value_type == SQLITE_TEXT || value_type == SQLITE_BLOB) {
        int byte_count = sqlite3_value_bytes(value);
        const void *bytes = sqlite3_value_blob(value);

        if ((bytes == NULL && byte_count != 0) || byte_count < 0) {
            sqlite3_result_error_nomem(context);
            return result;
        }
        result.bytes = bytes;
        result.size = (size_t)byte_count;
        result.ok = true;
        return result;
    }

    result.bytes = "";
    result.size = 0U;
    result.ok = true;
    return result;
}

static bool sqlite_swap_flag(sqlite3_value *value) {
    if (value == NULL || sqlite3_value_type(value) == SQLITE_NULL) {
        return false;
    }
    return sqlite3_value_int64(value) != 0;
}

static void initialize_uuid_generation_state(struct mylite_db *database) {
    uint16_t sequence = 0U;

    sqlite3_randomness((int)sizeof(sequence), &sequence);
    sqlite3_randomness((int)sizeof(database->session.uuid_node), database->session.uuid_node);
    database->session.uuid_clock_sequence = (uint16_t)(sequence & uuid_clock_sequence_mask);
    database->session.uuid_node[0] |= uuid_node_multicast_bit;
    database->session.uuid_state_initialized = true;
}

static uint64_t next_uuid_timestamp_100ns(struct mylite_db *database) {
    uint64_t timestamp = current_uuid_timestamp_100ns();

    if (timestamp <= database->session.uuid_last_timestamp_100ns) {
        timestamp = database->session.uuid_last_timestamp_100ns + 1U;
    }
    database->session.uuid_last_timestamp_100ns = timestamp;
    return timestamp;
}

static uint64_t current_uuid_timestamp_100ns(void) {
    time_t seconds = time(NULL);

    if (seconds < (time_t)0) {
        seconds = (time_t)0;
    }
    return uuid_unix_epoch_offset_100ns + ((uint64_t)seconds * uuid_100ns_per_second);
}

static void populate_uuid_bytes(
    struct uuid_generation_fields fields,
    const unsigned char node[MYLITE_SESSION_UUID_NODE_SIZE],
    unsigned char out_bytes[MYLITE_UUID_BINARY_SIZE]
) {
    uint32_t time_low = (uint32_t)(fields.timestamp_100ns & UINT32_MAX);
    uint16_t time_mid = (uint16_t)((fields.timestamp_100ns >> uuid_time_mid_shift) & UINT16_MAX);
    uint16_t time_high =
        (uint16_t)((fields.timestamp_100ns >> uuid_time_high_shift) & uuid_time_high_mask);

    memset(out_bytes, 0, MYLITE_UUID_BINARY_SIZE);
    store_uuid_uint32(out_bytes + uuid_time_low_offset, time_low);
    store_uuid_uint16(out_bytes + uuid_time_mid_offset, time_mid);
    store_uuid_uint16(
        out_bytes + uuid_time_high_offset,
        (uint16_t)(time_high | uuid_version_time_based)
    );
    out_bytes[uuid_tail_offset] =
        (unsigned char)(uuid_variant_rfc4122 |
                        ((fields.clock_sequence >> uuid_clock_sequence_high_shift) &
                         uuid_clock_sequence_high_mask));
    out_bytes[uuid_tail_offset + 1U] = (unsigned char)(fields.clock_sequence & UINT8_MAX);
    memcpy(out_bytes + uuid_node_offset, node, MYLITE_SESSION_UUID_NODE_SIZE);
}

static void store_uuid_uint32(unsigned char *destination, uint32_t value) {
    destination[0] = (unsigned char)((value >> uuid_uint32_high_shift) & UINT8_MAX);
    destination[1] = (unsigned char)((value >> uuid_uint32_mid_shift) & UINT8_MAX);
    destination[2] = (unsigned char)((value >> uuid_uint32_low_shift) & UINT8_MAX);
    destination[3] = (unsigned char)(value & UINT8_MAX);
}

static void store_uuid_uint16(unsigned char *destination, uint16_t value) {
    destination[0] = (unsigned char)((value >> uuid_uint16_high_shift) & UINT8_MAX);
    destination[1] = (unsigned char)(value & UINT8_MAX);
}

static bool parse_uuid_string(
    const void *input,
    size_t input_size,
    unsigned char out_bytes[MYLITE_UUID_BINARY_SIZE]
) {
    const unsigned char *bytes = input;

    if ((input == NULL && input_size != 0U) || out_bytes == NULL) {
        return false;
    }
    memset(out_bytes, 0, MYLITE_UUID_BINARY_SIZE);
    if (input_size == uuid_compact_text_size) {
        return parse_compact_uuid(bytes, input_size, out_bytes);
    }
    if (input_size == uuid_dashed_text_size) {
        return parse_dashed_uuid(bytes, input_size, out_bytes);
    }
    if (input_size == uuid_braced_text_size && bytes[0] == '{' && bytes[input_size - 1U] == '}') {
        return parse_dashed_uuid(bytes + 1U, input_size - 2U, out_bytes);
    }
    return false;
}

static bool parse_dashed_uuid(
    const unsigned char *bytes,
    size_t input_size,
    unsigned char out_bytes[MYLITE_UUID_BINARY_SIZE]
) {
    unsigned char compact[uuid_hex_digit_count];
    size_t compact_index = 0U;

    if (bytes == NULL || input_size != uuid_dashed_text_size || out_bytes == NULL) {
        return false;
    }
    for (size_t index = 0U; index < input_size; ++index) {
        if (index == uuid_dash_after_time_low || index == uuid_dash_after_time_mid ||
            index == uuid_dash_after_time_high || index == uuid_dash_after_clock_sequence) {
            if (bytes[index] != '-') {
                return false;
            }
            continue;
        }
        if (compact_index >= sizeof(compact)) {
            return false;
        }
        compact[compact_index] = bytes[index];
        ++compact_index;
    }
    if (compact_index != sizeof(compact)) {
        return false;
    }
    return parse_compact_uuid(compact, sizeof(compact), out_bytes);
}

static bool parse_compact_uuid(
    const unsigned char *bytes,
    size_t input_size,
    unsigned char out_bytes[MYLITE_UUID_BINARY_SIZE]
) {
    if (bytes == NULL || input_size != uuid_compact_text_size || out_bytes == NULL) {
        return false;
    }
    for (size_t index = 0U; index < MYLITE_UUID_BINARY_SIZE; ++index) {
        size_t high_index = index * uuid_hex_digits_per_byte;
        size_t low_index = high_index + 1U;

        if (!hex_pair_value(bytes[high_index], bytes[low_index], &out_bytes[index])) {
            return false;
        }
    }
    return true;
}

static void uuid_to_bin_swap(
    const unsigned char input[MYLITE_UUID_BINARY_SIZE],
    unsigned char output[MYLITE_UUID_BINARY_SIZE]
) {
    memcpy(
        output + uuid_swapped_time_high_offset,
        input + uuid_time_high_offset,
        uuid_time_high_size
    );
    memcpy(output + uuid_swapped_time_mid_offset, input + uuid_time_mid_offset, uuid_time_mid_size);
    memcpy(output + uuid_swapped_time_low_offset, input + uuid_time_low_offset, uuid_time_low_size);
    memcpy(output + uuid_tail_offset, input + uuid_tail_offset, uuid_tail_size);
}

static void bin_to_uuid_swap(
    const unsigned char input[MYLITE_UUID_BINARY_SIZE],
    unsigned char output[MYLITE_UUID_BINARY_SIZE]
) {
    memcpy(output + uuid_time_low_offset, input + uuid_swapped_time_low_offset, uuid_time_low_size);
    memcpy(output + uuid_time_mid_offset, input + uuid_swapped_time_mid_offset, uuid_time_mid_size);
    memcpy(
        output + uuid_time_high_offset,
        input + uuid_swapped_time_high_offset,
        uuid_time_high_size
    );
    memcpy(output + uuid_tail_offset, input + uuid_tail_offset, uuid_tail_size);
}

static void format_uuid_string(
    const unsigned char input[MYLITE_UUID_BINARY_SIZE],
    char out_text[MYLITE_UUID_TEXT_SIZE + 1U]
) {
    static const char digits[] = "0123456789abcdef";
    size_t output_index = 0U;

    for (size_t input_index = 0U; input_index < MYLITE_UUID_BINARY_SIZE; ++input_index) {
        if (input_index == uuid_time_mid_offset || input_index == uuid_time_high_offset ||
            input_index == uuid_tail_offset || input_index == uuid_node_offset) {
            out_text[output_index] = '-';
            ++output_index;
        }
        out_text[output_index] = digits[input[input_index] >> uuid_high_nibble_shift];
        out_text[output_index + 1U] = digits[input[input_index] & uuid_low_nibble_mask];
        output_index += 2U;
    }
    out_text[output_index] = '\0';
}

static bool hex_pair_value(unsigned char high, unsigned char low, unsigned char *out_value) {
    unsigned char high_value = 0U;
    unsigned char low_value = 0U;

    if (out_value == NULL) {
        return false;
    }
    if (!hex_digit_value(high, &high_value) || !hex_digit_value(low, &low_value)) {
        return false;
    }
    *out_value = (unsigned char)((high_value << 4U) | low_value);
    return true;
}

static bool hex_digit_value(unsigned char byte, unsigned char *out_value) {
    if (out_value == NULL) {
        return false;
    }
    if (byte >= '0' && byte <= '9') {
        *out_value = (unsigned char)(byte - '0');
        return true;
    }
    if (byte >= 'A' && byte <= 'F') {
        *out_value = (unsigned char)(byte - 'A' + uuid_hex_alpha_offset);
        return true;
    }
    if (byte >= 'a' && byte <= 'f') {
        *out_value = (unsigned char)(byte - 'a' + uuid_hex_alpha_offset);
        return true;
    }
    return false;
}

static int format_diagnostic_input(
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

        if (diagnostic_input_byte_is_printable(byte)) {
            destination[index] = (char)byte;
        } else {
            destination[index] = '?';
        }
    }
    destination[limit] = '\0';

    return MYLITE_OK;
}

static bool diagnostic_input_byte_is_printable(unsigned char byte) {
    if (byte < uuid_printable_minimum) {
        return false;
    }
    if (byte > uuid_printable_maximum) {
        return false;
    }
    return true;
}
