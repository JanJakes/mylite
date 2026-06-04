#include "mylite_rand.h"

#include "mylite_sqlite_registration.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

enum {
    scalar_bitwise_integer_bits = 64,
    rand_double_value_bits = 53,
    rand_double_discard_bits = scalar_bitwise_integer_bits - rand_double_value_bits,
    rand_seed_state_modulus = 0x3fffffff,
    rand_seed_first_multiplier = 0x10001,
    rand_seed_second_multiplier = 0x10000001,
    rand_seed_first_addend = 55555555,
    rand_seed_step_multiplier = 3,
    rand_seed_step_addend = 33,
    rand_decimal_base = 10,
};

static void rand_unseeded_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void rand_seeded_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void rand_seeded_once_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static bool rand_seed_from_sqlite_value(
    sqlite3_context *context,
    sqlite3_value *value,
    uint32_t *out_seed
);
static bool rand_seed_from_decimal_text(
    sqlite3_context *context,
    const unsigned char *text,
    int text_length,
    uint32_t *out_seed
);
static struct mylite_rand_state *rand_seeded_sqlite_state(
    sqlite3_context *context,
    sqlite3_value *seed_value
);
static void rand_seeded_sqlite_state_free(void *state);
static uint32_t rand_seed_initial_word(uint32_t seed, uint32_t multiplier, uint32_t addend);

int mylite_sqlite_register_rand_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_rand",
            .argument_count = 0,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = rand_unseeded_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_rand_seeded",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = rand_seeded_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_rand_seeded_once",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = rand_seeded_once_sqlite_callback,
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

double mylite_rand_unseeded_unit_double(void) {
    uint64_t random_bits = 0U;

    sqlite3_randomness((int)sizeof(random_bits), &random_bits);
    random_bits >>= rand_double_discard_bits;
    return ldexp((double)random_bits, -rand_double_value_bits);
}

double mylite_rand_seeded_unit_double(uint32_t seed) {
    struct mylite_rand_state state = {0};

    mylite_rand_state_init(&state, seed);
    return mylite_rand_state_next_unit_double(&state);
}

void mylite_rand_state_init(struct mylite_rand_state *state, uint32_t seed) {
    if (state == NULL) {
        return;
    }

    state->seed1 = rand_seed_initial_word(seed, rand_seed_first_multiplier, rand_seed_first_addend);
    state->seed2 = rand_seed_initial_word(seed, rand_seed_second_multiplier, 0U);
}

double mylite_rand_state_next_unit_double(struct mylite_rand_state *state) {
    uint64_t next_seed1 = 0U;
    uint64_t next_seed2 = 0U;

    if (state == NULL) {
        return 0.0;
    }

    next_seed1 = ((uint64_t)state->seed1 * rand_seed_step_multiplier) + state->seed2;
    state->seed1 = (uint32_t)(next_seed1 % rand_seed_state_modulus);
    next_seed2 = (uint64_t)state->seed1 + state->seed2 + rand_seed_step_addend;
    state->seed2 = (uint32_t)(next_seed2 % rand_seed_state_modulus);

    return (double)state->seed1 / (double)rand_seed_state_modulus;
}

static void rand_unseeded_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    (void)argv;

    if (context == NULL) {
        return;
    }
    if (argc != 0) {
        sqlite3_result_error(context, "invalid MyLite RAND callback", -1);
        return;
    }

    sqlite3_result_double(context, mylite_rand_unseeded_unit_double());
}

static void rand_seeded_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_rand_state *state = NULL;

    if (context == NULL) {
        return;
    }
    if (argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite seeded RAND callback", -1);
        return;
    }

    state = rand_seeded_sqlite_state(context, argv[0]);
    if (state == NULL) {
        return;
    }

    sqlite3_result_double(context, mylite_rand_state_next_unit_double(state));
}

static void rand_seeded_once_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    uint32_t seed = 0U;

    if (context == NULL) {
        return;
    }
    if (argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite seeded RAND callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_double(context, mylite_rand_seeded_unit_double(seed));
        return;
    }
    if (!rand_seed_from_sqlite_value(context, argv[0], &seed)) {
        return;
    }

    sqlite3_result_double(context, mylite_rand_seeded_unit_double(seed));
}

static bool rand_seed_from_sqlite_value(
    sqlite3_context *context,
    sqlite3_value *value,
    uint32_t *out_seed
) {
    const unsigned char *text = NULL;
    int text_length = 0;

    if (value == NULL || out_seed == NULL) {
        sqlite3_result_error(context, "invalid MyLite seeded RAND argument", -1);
        return false;
    }
    if (sqlite3_value_type(value) == SQLITE_INTEGER) {
        *out_seed = (uint32_t)sqlite3_value_int64(value);
        return true;
    }
    if (sqlite3_value_type(value) != SQLITE_TEXT) {
        sqlite3_result_error(context, "invalid MyLite seeded RAND argument", -1);
        return false;
    }

    text = sqlite3_value_text(value);
    text_length = sqlite3_value_bytes(value);
    if (text == NULL || text_length < 0) {
        sqlite3_result_error_nomem(context);
        return false;
    }
    return rand_seed_from_decimal_text(context, text, text_length, out_seed);
}

static bool rand_seed_from_decimal_text(
    sqlite3_context *context,
    const unsigned char *text,
    int text_length,
    uint32_t *out_seed
) {
    bool is_negative = false;
    uint32_t magnitude = 0U;
    int offset = 0;

    if (text == NULL || text_length <= 0 || out_seed == NULL) {
        sqlite3_result_error(context, "invalid MyLite seeded RAND argument", -1);
        return false;
    }
    if (text[offset] == '-') {
        is_negative = true;
        ++offset;
        if (offset == text_length) {
            sqlite3_result_error(context, "invalid MyLite seeded RAND argument", -1);
            return false;
        }
    }
    for (; offset < text_length; ++offset) {
        if (text[offset] < '0' || text[offset] > '9') {
            sqlite3_result_error(context, "invalid MyLite seeded RAND argument", -1);
            return false;
        }
        magnitude = (magnitude * rand_decimal_base) + (uint32_t)(text[offset] - '0');
    }

    *out_seed = is_negative ? (uint32_t)(0U - magnitude) : magnitude;
    return true;
}

static struct mylite_rand_state *rand_seeded_sqlite_state(
    sqlite3_context *context,
    sqlite3_value *seed_value
) {
    struct mylite_rand_state *state = sqlite3_get_auxdata(context, 0);
    uint32_t seed = 0U;

    if (state != NULL) {
        return state;
    }
    if (sqlite3_value_type(seed_value) != SQLITE_INTEGER) {
        sqlite3_result_error(context, "invalid MyLite seeded RAND argument", -1);
        return NULL;
    }

    state = malloc(sizeof(*state));
    if (state == NULL) {
        sqlite3_result_error_nomem(context);
        return NULL;
    }
    seed = (uint32_t)sqlite3_value_int64(seed_value);
    mylite_rand_state_init(state, seed);
    sqlite3_set_auxdata(context, 0, state, rand_seeded_sqlite_state_free);
    return state;
}

static void rand_seeded_sqlite_state_free(void *state) {
    free(state);
}

static uint32_t rand_seed_initial_word(uint32_t seed, uint32_t multiplier, uint32_t addend) {
    uint32_t overflowed = (uint32_t)(((uint64_t)seed * multiplier) + addend);

    return overflowed % rand_seed_state_modulus;
}
