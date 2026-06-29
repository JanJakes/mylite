#include "mylite_bitwise_aggregate.h"

#include "mylite_sqlite_registration.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum { bitwise_aggregate_bit_count = 64 };

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
    uint64_t non_null_count;
    uint64_t bit_counts[bitwise_aggregate_bit_count];
};

static void bitwise_aggregate_step(sqlite3_context *context, int argc, sqlite3_value **argv);
static void bitwise_aggregate_inverse(sqlite3_context *context, int argc, sqlite3_value **argv);
static void bitwise_aggregate_final(sqlite3_context *context);
static void bitwise_aggregate_value(sqlite3_context *context);
static void uint64_decimal_order_key(sqlite3_context *context, int argc, sqlite3_value **argv);
static const struct mylite_bitwise_aggregate_config *bitwise_aggregate_config(
    sqlite3_context *context
);
static bool read_bitwise_aggregate_argument(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv,
    uint64_t *out_value,
    bool *out_is_null
);
static void add_bitwise_aggregate_value(
    struct mylite_bitwise_aggregate_state *state,
    uint64_t value
);
static bool remove_bitwise_aggregate_value(
    sqlite3_context *context,
    struct mylite_bitwise_aggregate_state *state,
    uint64_t value
);
static uint64_t bitwise_aggregate_result(
    const struct mylite_bitwise_aggregate_config *config,
    const struct mylite_bitwise_aggregate_state *state
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
            .kind = MYLITE_SQLITE_FUNCTION_WINDOW,
            .name = "_mylite_bit_and",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = &and_config,
            .scalar_callback = NULL,
            .step_callback = bitwise_aggregate_step,
            .final_callback = bitwise_aggregate_final,
            .value_callback = bitwise_aggregate_value,
            .inverse_callback = bitwise_aggregate_inverse,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_WINDOW,
            .name = "_mylite_bit_or",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = &or_config,
            .scalar_callback = NULL,
            .step_callback = bitwise_aggregate_step,
            .final_callback = bitwise_aggregate_final,
            .value_callback = bitwise_aggregate_value,
            .inverse_callback = bitwise_aggregate_inverse,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_WINDOW,
            .name = "_mylite_bit_xor",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = &xor_config,
            .scalar_callback = NULL,
            .step_callback = bitwise_aggregate_step,
            .final_callback = bitwise_aggregate_final,
            .value_callback = bitwise_aggregate_value,
            .inverse_callback = bitwise_aggregate_inverse,
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
    uint64_t value = 0U;
    bool is_null = false;

    if (config == NULL) {
        sqlite3_result_error(context, "invalid MyLite bitwise aggregate callback", -1);
        return;
    }
    if (!read_bitwise_aggregate_argument(context, argc, argv, &value, &is_null)) {
        return;
    }
    if (is_null) {
        return;
    }
    state = sqlite3_aggregate_context(context, (int)sizeof(*state));
    if (state == NULL) {
        sqlite3_result_error_nomem(context);
        return;
    }
    add_bitwise_aggregate_value(state, value);
}

static void bitwise_aggregate_inverse(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const struct mylite_bitwise_aggregate_config *config = bitwise_aggregate_config(context);
    struct mylite_bitwise_aggregate_state *state = NULL;
    uint64_t value = 0U;
    bool is_null = false;

    if (config == NULL) {
        sqlite3_result_error(context, "invalid MyLite bitwise aggregate callback", -1);
        return;
    }
    if (!read_bitwise_aggregate_argument(context, argc, argv, &value, &is_null)) {
        return;
    }
    if (is_null) {
        return;
    }
    state = sqlite3_aggregate_context(context, 0);
    if (state == NULL) {
        sqlite3_result_error(context, "invalid MyLite bitwise aggregate window state", -1);
        return;
    }
    if (!remove_bitwise_aggregate_value(context, state, value)) {
        return;
    }
}

static void bitwise_aggregate_final(sqlite3_context *context) {
    const struct mylite_bitwise_aggregate_config *config = bitwise_aggregate_config(context);
    struct mylite_bitwise_aggregate_state *state = sqlite3_aggregate_context(context, 0);

    if (config == NULL) {
        sqlite3_result_error(context, "invalid MyLite bitwise aggregate callback", -1);
        return;
    }

    set_bitwise_aggregate_result(context, bitwise_aggregate_result(config, state));
}

static void bitwise_aggregate_value(sqlite3_context *context) {
    const struct mylite_bitwise_aggregate_config *config = bitwise_aggregate_config(context);
    struct mylite_bitwise_aggregate_state *state = sqlite3_aggregate_context(context, 0);

    if (config == NULL) {
        sqlite3_result_error(context, "invalid MyLite bitwise aggregate callback", -1);
        return;
    }

    set_bitwise_aggregate_result(context, bitwise_aggregate_result(config, state));
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

static bool read_bitwise_aggregate_argument(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv,
    uint64_t *out_value,
    bool *out_is_null
) {
    int value_type = SQLITE_NULL;

    if (argc != 1 || argv == NULL || argv[0] == NULL || out_value == NULL || out_is_null == NULL) {
        sqlite3_result_error(context, "invalid MyLite bitwise aggregate callback", -1);
        return false;
    }

    *out_value = 0U;
    *out_is_null = false;
    value_type = sqlite3_value_type(argv[0]);
    if (value_type == SQLITE_NULL) {
        *out_is_null = true;
        return true;
    }
    if (value_type != SQLITE_INTEGER) {
        sqlite3_result_error(context, "invalid MyLite bitwise aggregate input type", -1);
        return false;
    }

    *out_value = (uint64_t)(int64_t)sqlite3_value_int64(argv[0]);
    return true;
}

static void add_bitwise_aggregate_value(
    struct mylite_bitwise_aggregate_state *state,
    uint64_t value
) {
    if (state == NULL) {
        return;
    }

    ++state->non_null_count;
    for (size_t bit_index = 0U; bit_index < bitwise_aggregate_bit_count; ++bit_index) {
        uint64_t mask = UINT64_C(1) << bit_index;
        if ((value & mask) != 0U) {
            ++state->bit_counts[bit_index];
        }
    }
}

static bool remove_bitwise_aggregate_value(
    sqlite3_context *context,
    struct mylite_bitwise_aggregate_state *state,
    uint64_t value
) {
    if (state == NULL || state->non_null_count == 0U) {
        sqlite3_result_error(context, "invalid MyLite bitwise aggregate window state", -1);
        return false;
    }

    --state->non_null_count;
    for (size_t bit_index = 0U; bit_index < bitwise_aggregate_bit_count; ++bit_index) {
        uint64_t mask = UINT64_C(1) << bit_index;
        if ((value & mask) == 0U) {
            continue;
        }
        if (state->bit_counts[bit_index] == 0U) {
            sqlite3_result_error(context, "invalid MyLite bitwise aggregate window state", -1);
            return false;
        }
        --state->bit_counts[bit_index];
    }
    return true;
}

static uint64_t bitwise_aggregate_result(
    const struct mylite_bitwise_aggregate_config *config,
    const struct mylite_bitwise_aggregate_state *state
) {
    uint64_t result = 0U;

    if (config == NULL || state == NULL || state->non_null_count == 0U) {
        return config == NULL ? 0U : config->neutral_value;
    }

    for (size_t bit_index = 0U; bit_index < bitwise_aggregate_bit_count; ++bit_index) {
        uint64_t bit_count = state->bit_counts[bit_index];
        uint64_t mask = UINT64_C(1) << bit_index;
        bool set_bit = false;

        switch (config->operation) {
        case MYLITE_BITWISE_AGGREGATE_AND:
            set_bit = bit_count == state->non_null_count;
            break;
        case MYLITE_BITWISE_AGGREGATE_OR:
            set_bit = bit_count != 0U;
            break;
        case MYLITE_BITWISE_AGGREGATE_XOR:
            set_bit = (bit_count % 2U) != 0U;
            break;
        }
        if (set_bit) {
            result |= mask;
        }
    }
    return result;
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
