#include "mylite_bitwise_aggregate.h"

#include "mylite_sqlite_registration.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

enum mylite_bitwise_aggregate_operation {
    MYLITE_BITWISE_AGGREGATE_AND = 0,
    MYLITE_BITWISE_AGGREGATE_OR = 1,
    MYLITE_BITWISE_AGGREGATE_XOR = 2,
};

struct mylite_bitwise_aggregate_config {
    enum mylite_bitwise_aggregate_operation operation;
    uint64_t neutral_value;
};

struct mylite_bitwise_aggregate_state {
    bool initialized;
    uint64_t value;
};

static void bitwise_aggregate_step(sqlite3_context *context, int argc, sqlite3_value **argv);
static void bitwise_aggregate_final(sqlite3_context *context);
static void uint64_decimal_order_key(sqlite3_context *context, int argc, sqlite3_value **argv);
static const struct mylite_bitwise_aggregate_config *bitwise_aggregate_config(
    sqlite3_context *context
);
static void apply_bitwise_aggregate_operation(
    const struct mylite_bitwise_aggregate_config *config,
    struct mylite_bitwise_aggregate_state *state,
    uint64_t value
);
static void set_bitwise_aggregate_result(sqlite3_context *context, uint64_t value);
static bool parse_uint64_decimal_text(
    const unsigned char *text,
    int byte_count,
    uint64_t *out_value
);

int mylite_sqlite_register_bitwise_aggregate_functions(sqlite3 *sqlite) {
    static struct mylite_bitwise_aggregate_config and_config = {
        .operation = MYLITE_BITWISE_AGGREGATE_AND,
        .neutral_value = UINT64_MAX,
    };
    static struct mylite_bitwise_aggregate_config or_config = {
        .operation = MYLITE_BITWISE_AGGREGATE_OR,
        .neutral_value = 0U,
    };
    static struct mylite_bitwise_aggregate_config xor_config = {
        .operation = MYLITE_BITWISE_AGGREGATE_XOR,
        .neutral_value = 0U,
    };
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_AGGREGATE,
            .name = "_mylite_bit_and",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = &and_config,
            .scalar_callback = NULL,
            .step_callback = bitwise_aggregate_step,
            .final_callback = bitwise_aggregate_final,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_AGGREGATE,
            .name = "_mylite_bit_or",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = &or_config,
            .scalar_callback = NULL,
            .step_callback = bitwise_aggregate_step,
            .final_callback = bitwise_aggregate_final,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_AGGREGATE,
            .name = "_mylite_bit_xor",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = &xor_config,
            .scalar_callback = NULL,
            .step_callback = bitwise_aggregate_step,
            .final_callback = bitwise_aggregate_final,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_uint64_decimal_order_key",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = uint64_decimal_order_key,
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

static void bitwise_aggregate_step(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const struct mylite_bitwise_aggregate_config *config = bitwise_aggregate_config(context);
    struct mylite_bitwise_aggregate_state *state = NULL;
    int value_type = SQLITE_NULL;
    uint64_t value = 0U;

    if (config == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite bitwise aggregate callback", -1);
        return;
    }

    value_type = sqlite3_value_type(argv[0]);
    if (value_type == SQLITE_NULL) {
        return;
    }
    if (value_type != SQLITE_INTEGER) {
        sqlite3_result_error(context, "invalid MyLite bitwise aggregate input type", -1);
        return;
    }

    state = sqlite3_aggregate_context(context, (int)sizeof(*state));
    if (state == NULL) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (!state->initialized) {
        state->initialized = true;
        state->value = config->neutral_value;
    }

    value = (uint64_t)(int64_t)sqlite3_value_int64(argv[0]);
    apply_bitwise_aggregate_operation(config, state, value);
}

static void bitwise_aggregate_final(sqlite3_context *context) {
    const struct mylite_bitwise_aggregate_config *config = bitwise_aggregate_config(context);
    struct mylite_bitwise_aggregate_state *state = sqlite3_aggregate_context(context, 0);

    if (config == NULL) {
        sqlite3_result_error(context, "invalid MyLite bitwise aggregate callback", -1);
        return;
    }

    set_bitwise_aggregate_result(
        context,
        state != NULL && state->initialized ? state->value : config->neutral_value
    );
}

static void uint64_decimal_order_key(sqlite3_context *context, int argc, sqlite3_value **argv) {
    enum { uint64_order_key_capacity = 21 };

    char key[uint64_order_key_capacity];
    uint64_t value = 0U;
    int written = 0;

    if (argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite uint64 order-key callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT || !parse_uint64_decimal_text(
                                                          sqlite3_value_text(argv[0]),
                                                          sqlite3_value_bytes(argv[0]),
                                                          &value
                                                      )) {
        sqlite3_result_error(context, "invalid MyLite uint64 order-key input", -1);
        return;
    }

    written = snprintf(key, sizeof(key), "%020" PRIu64, value);
    if (written < 0 || (size_t)written >= sizeof(key)) {
        sqlite3_result_error(context, "failed to format MyLite uint64 order key", -1);
        return;
    }

    sqlite3_result_text(context, key, -1, SQLITE_TRANSIENT);
}

static const struct mylite_bitwise_aggregate_config *bitwise_aggregate_config(
    sqlite3_context *context
) {
    return sqlite3_user_data(context);
}

static void apply_bitwise_aggregate_operation(
    const struct mylite_bitwise_aggregate_config *config,
    struct mylite_bitwise_aggregate_state *state,
    uint64_t value
) {
    if (config == NULL || state == NULL) {
        return;
    }

    switch (config->operation) {
    case MYLITE_BITWISE_AGGREGATE_AND:
        state->value &= value;
        return;
    case MYLITE_BITWISE_AGGREGATE_OR:
        state->value |= value;
        return;
    case MYLITE_BITWISE_AGGREGATE_XOR:
        state->value ^= value;
        return;
    }
}

static void set_bitwise_aggregate_result(sqlite3_context *context, uint64_t value) {
    enum { uint64_text_capacity = 21 };

    char text[uint64_text_capacity];
    int written = snprintf(text, sizeof(text), "%" PRIu64, value);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        sqlite3_result_error(context, "failed to format MyLite bitwise aggregate result", -1);
        return;
    }

    sqlite3_result_text(context, text, -1, SQLITE_TRANSIENT);
}

static bool parse_uint64_decimal_text(
    const unsigned char *text,
    int byte_count,
    uint64_t *out_value
) {
    enum {
        max_uint64_decimal_digits = 20,
        decimal_radix = 10,
    };

    uint64_t value = 0U;

    if (text == NULL || byte_count <= 0 || byte_count > max_uint64_decimal_digits ||
        out_value == NULL) {
        return false;
    }
    for (int index = 0; index < byte_count; ++index) {
        unsigned char character = text[index];
        uint64_t digit = 0U;

        if (character < '0' || character > '9') {
            return false;
        }
        digit = (uint64_t)(character - '0');
        if (value > (UINT64_MAX - digit) / decimal_radix) {
            return false;
        }
        value = (value * decimal_radix) + digit;
    }

    *out_value = value;
    return true;
}
