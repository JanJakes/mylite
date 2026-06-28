#include "mylite_aes.h"

#include "mylite_sqlite_registration.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// NOLINTBEGIN(readability-identifier-length, readability-magic-numbers): AES uses standard
// byte-oriented transforms, round counts, and finite-field constants.

enum {
    aes_block_size = 16,
    aes_round_count = 10,
    aes_round_key_size = 176,
    aes_word_size = 4,
};

enum aes_sqlite_operation {
    AES_SQLITE_ENCRYPT = 0,
    AES_SQLITE_DECRYPT = 1,
};

struct aes_round_keys {
    unsigned char bytes[aes_round_key_size];
};

struct aes_sboxes {
    unsigned char direct[256];
    unsigned char inverse[256];
};

static const enum aes_sqlite_operation sqlite_aes_encrypt_operation = AES_SQLITE_ENCRYPT;
static const enum aes_sqlite_operation sqlite_aes_decrypt_operation = AES_SQLITE_DECRYPT;

static void aes_encrypt_block(
    const unsigned char input[aes_block_size],
    const struct aes_round_keys *round_keys,
    const struct aes_sboxes *sboxes,
    unsigned char output[aes_block_size]
);
static void aes_decrypt_block(
    const unsigned char input[aes_block_size],
    const struct aes_round_keys *round_keys,
    const struct aes_sboxes *sboxes,
    unsigned char output[aes_block_size]
);
static void aes_expand_key(
    const unsigned char key[aes_block_size],
    const struct aes_sboxes *sboxes,
    struct aes_round_keys *round_keys
);
static void aes_fold_mysql_key(
    const unsigned char *key,
    size_t key_size,
    unsigned char out_key[aes_block_size]
);
static void aes_generate_sboxes(struct aes_sboxes *out_sboxes);
static const unsigned char *aes_round_key(const struct aes_round_keys *round_keys, size_t round);
static unsigned char aes_sbox_value(unsigned char value);
static unsigned char aes_multiplicative_inverse(unsigned char value);
static unsigned char aes_gf_mul(unsigned char left, unsigned char right);
static unsigned char aes_xtime(unsigned char value);
static unsigned char aes_rotl(unsigned char value, unsigned int shift);
static void aes_add_round_key(unsigned char state[aes_block_size], const unsigned char *round_key);
static void aes_sub_bytes(unsigned char state[aes_block_size], const unsigned char sbox[256]);
static void aes_inv_sub_bytes(
    unsigned char state[aes_block_size],
    const unsigned char inverse_sbox[256]
);
static void aes_shift_rows(unsigned char state[aes_block_size]);
static void aes_inv_shift_rows(unsigned char state[aes_block_size]);
static void aes_mix_columns(unsigned char state[aes_block_size]);
static void aes_inv_mix_columns(unsigned char state[aes_block_size]);
static bool aes_valid_padding(const unsigned char *bytes, size_t byte_count, size_t *out_size);
static void aes_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);

int mylite_aes_encrypt_default(
    const unsigned char *input,
    size_t input_size,
    const unsigned char *key,
    size_t key_size,
    unsigned char **out_bytes,
    size_t *out_size,
    bool *out_long_key_warning
) {
    unsigned char folded_key[aes_block_size];
    struct aes_round_keys round_keys;
    struct aes_sboxes sboxes;
    unsigned char *output = NULL;
    size_t padding = 0U;
    size_t output_size = 0U;

    if (out_bytes == NULL || out_size == NULL || out_long_key_warning == NULL ||
        (input == NULL && input_size != 0U) || (key == NULL && key_size != 0U)) {
        return MYLITE_MISUSE;
    }
    *out_bytes = NULL;
    *out_size = 0U;
    *out_long_key_warning = key_size > aes_block_size;
    padding = aes_block_size - (input_size % aes_block_size);
    if (input_size > SIZE_MAX - padding) {
        return MYLITE_NOMEM;
    }
    output_size = input_size + padding;
    output = (unsigned char *)malloc(output_size);
    if (output == NULL) {
        return MYLITE_NOMEM;
    }
    if (input_size != 0U) {
        memcpy(output, input, input_size);
    }
    memset(output + input_size, (int)padding, padding);

    aes_generate_sboxes(&sboxes);
    aes_fold_mysql_key(key, key_size, folded_key);
    aes_expand_key(folded_key, &sboxes, &round_keys);
    for (size_t offset = 0U; offset < output_size; offset += aes_block_size) {
        aes_encrypt_block(output + offset, &round_keys, &sboxes, output + offset);
    }
    *out_bytes = output;
    *out_size = output_size;
    return MYLITE_OK;
}

int mylite_aes_decrypt_default(
    const unsigned char *input,
    size_t input_size,
    const unsigned char *key,
    size_t key_size,
    unsigned char **out_bytes,
    size_t *out_size,
    bool *out_invalid,
    bool *out_long_key_warning
) {
    unsigned char folded_key[aes_block_size];
    struct aes_round_keys round_keys;
    struct aes_sboxes sboxes;
    unsigned char *output = NULL;
    size_t unpadded_size = 0U;

    if (out_bytes == NULL || out_size == NULL || out_invalid == NULL ||
        out_long_key_warning == NULL || (input == NULL && input_size != 0U) ||
        (key == NULL && key_size != 0U)) {
        return MYLITE_MISUSE;
    }
    *out_bytes = NULL;
    *out_size = 0U;
    *out_invalid = false;
    *out_long_key_warning = key_size > aes_block_size;
    if (input_size == 0U || (input_size % aes_block_size) != 0U) {
        *out_invalid = true;
        return MYLITE_OK;
    }
    output = (unsigned char *)malloc(input_size);
    if (output == NULL) {
        return MYLITE_NOMEM;
    }

    aes_generate_sboxes(&sboxes);
    aes_fold_mysql_key(key, key_size, folded_key);
    aes_expand_key(folded_key, &sboxes, &round_keys);
    for (size_t offset = 0U; offset < input_size; offset += aes_block_size) {
        aes_decrypt_block(input + offset, &round_keys, &sboxes, output + offset);
    }
    if (!aes_valid_padding(output, input_size, &unpadded_size)) {
        free(output);
        *out_invalid = true;
        return MYLITE_OK;
    }
    *out_bytes = output;
    *out_size = unpadded_size;
    return MYLITE_OK;
}

int mylite_sqlite_register_aes_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_aes_encrypt",
            .argument_count = 2,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = (void *)&sqlite_aes_encrypt_operation,
            .scalar_callback = aes_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_aes_decrypt",
            .argument_count = 2,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = (void *)&sqlite_aes_decrypt_operation,
            .scalar_callback = aes_sqlite_callback,
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

static void aes_encrypt_block(
    const unsigned char input[aes_block_size],
    const struct aes_round_keys *round_keys,
    const struct aes_sboxes *sboxes,
    unsigned char output[aes_block_size]
) {
    unsigned char state[aes_block_size];

    memcpy(state, input, sizeof(state));
    aes_add_round_key(state, aes_round_key(round_keys, 0U));
    for (size_t round = 1U; round < aes_round_count; ++round) {
        aes_sub_bytes(state, sboxes->direct);
        aes_shift_rows(state);
        aes_mix_columns(state);
        aes_add_round_key(state, aes_round_key(round_keys, round));
    }
    aes_sub_bytes(state, sboxes->direct);
    aes_shift_rows(state);
    aes_add_round_key(state, aes_round_key(round_keys, aes_round_count));
    memcpy(output, state, sizeof(state));
}

static void aes_decrypt_block(
    const unsigned char input[aes_block_size],
    const struct aes_round_keys *round_keys,
    const struct aes_sboxes *sboxes,
    unsigned char output[aes_block_size]
) {
    unsigned char state[aes_block_size];

    memcpy(state, input, sizeof(state));
    aes_add_round_key(state, aes_round_key(round_keys, aes_round_count));
    for (size_t round = aes_round_count - 1U; round > 0U; --round) {
        aes_inv_shift_rows(state);
        aes_inv_sub_bytes(state, sboxes->inverse);
        aes_add_round_key(state, aes_round_key(round_keys, round));
        aes_inv_mix_columns(state);
    }
    aes_inv_shift_rows(state);
    aes_inv_sub_bytes(state, sboxes->inverse);
    aes_add_round_key(state, aes_round_key(round_keys, 0U));
    memcpy(output, state, sizeof(state));
}

static void aes_expand_key(
    const unsigned char key[aes_block_size],
    const struct aes_sboxes *sboxes,
    struct aes_round_keys *round_keys
) {
    unsigned char rcon = 1U;
    size_t generated = aes_block_size;

    memcpy(round_keys->bytes, key, aes_block_size);
    while (generated < aes_round_key_size) {
        unsigned char temp[aes_word_size];

        memcpy(temp, round_keys->bytes + generated - aes_word_size, sizeof(temp));
        if ((generated % aes_block_size) == 0U) {
            unsigned char first = temp[0];

            temp[0] = (unsigned char)(sboxes->direct[temp[1]] ^ rcon);
            temp[1] = sboxes->direct[temp[2]];
            temp[2] = sboxes->direct[temp[3]];
            temp[3] = sboxes->direct[first];
            rcon = aes_xtime(rcon);
        }
        for (size_t index = 0U; index < aes_word_size; ++index) {
            round_keys->bytes[generated] =
                (unsigned char)(round_keys->bytes[generated - aes_block_size] ^ temp[index]);
            ++generated;
        }
    }
}

static void aes_fold_mysql_key(
    const unsigned char *key,
    size_t key_size,
    unsigned char out_key[aes_block_size]
) {
    memset(out_key, 0, aes_block_size);
    for (size_t index = 0U; index < key_size; ++index) {
        out_key[index % aes_block_size] =
            (unsigned char)(out_key[index % aes_block_size] ^ key[index]);
    }
}

static void aes_generate_sboxes(struct aes_sboxes *out_sboxes) {
    for (unsigned int value = 0U; value <= UINT8_MAX; ++value) {
        unsigned char substituted = aes_sbox_value((unsigned char)value);

        out_sboxes->direct[value] = substituted;
        out_sboxes->inverse[substituted] = (unsigned char)value;
    }
}

static const unsigned char *aes_round_key(const struct aes_round_keys *round_keys, size_t round) {
    return round_keys->bytes + (round * aes_block_size);
}

static unsigned char aes_sbox_value(unsigned char value) {
    unsigned char inverse = aes_multiplicative_inverse(value);

    return (unsigned char)(inverse ^ aes_rotl(inverse, 1U) ^ aes_rotl(inverse, 2U) ^
                           aes_rotl(inverse, 3U) ^ aes_rotl(inverse, 4U) ^ 0x63U);
}

static unsigned char aes_multiplicative_inverse(unsigned char value) {
    unsigned char result = 1U;
    unsigned char base = value;
    unsigned int exponent = 254U;

    if (value == 0U) {
        return 0U;
    }
    while (exponent != 0U) {
        if ((exponent & 1U) != 0U) {
            result = aes_gf_mul(result, base);
        }
        base = aes_gf_mul(base, base);
        exponent >>= 1U;
    }
    return result;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): finite-field multiplication is symmetric.
static unsigned char aes_gf_mul(unsigned char left, unsigned char right) {
    unsigned char result = 0U;

    while (right != 0U) {
        if ((right & 1U) != 0U) {
            result = (unsigned char)(result ^ left);
        }
        left = aes_xtime(left);
        right >>= 1U;
    }
    return result;
}

static unsigned char aes_xtime(unsigned char value) {
    unsigned int promoted = value;

    return (unsigned char)((promoted << 1U) ^ (((value & 0x80U) != 0U) ? 0x1BU : 0U));
}

static unsigned char aes_rotl(unsigned char value, unsigned int shift) {
    return (unsigned char)((value << shift) | (value >> (8U - shift)));
}

static void aes_add_round_key(unsigned char state[aes_block_size], const unsigned char *round_key) {
    for (size_t index = 0U; index < aes_block_size; ++index) {
        state[index] = (unsigned char)(state[index] ^ round_key[index]);
    }
}

static void aes_sub_bytes(unsigned char state[aes_block_size], const unsigned char sbox[256]) {
    for (size_t index = 0U; index < aes_block_size; ++index) {
        state[index] = sbox[state[index]];
    }
}

static void aes_inv_sub_bytes(
    unsigned char state[aes_block_size],
    const unsigned char inverse_sbox[256]
) {
    for (size_t index = 0U; index < aes_block_size; ++index) {
        state[index] = inverse_sbox[state[index]];
    }
}

static void aes_shift_rows(unsigned char state[aes_block_size]) {
    unsigned char temp[aes_block_size];

    memcpy(temp, state, sizeof(temp));
    for (size_t row = 0U; row < aes_word_size; ++row) {
        for (size_t column = 0U; column < aes_word_size; ++column) {
            state[row + (aes_word_size * column)] =
                temp[row + (aes_word_size * ((column + row) % aes_word_size))];
        }
    }
}

static void aes_inv_shift_rows(unsigned char state[aes_block_size]) {
    unsigned char temp[aes_block_size];

    memcpy(temp, state, sizeof(temp));
    for (size_t row = 0U; row < aes_word_size; ++row) {
        for (size_t column = 0U; column < aes_word_size; ++column) {
            state[row + (aes_word_size * column)] =
                temp[row + (aes_word_size * ((column + aes_word_size - row) % aes_word_size))];
        }
    }
}

static void aes_mix_columns(unsigned char state[aes_block_size]) {
    for (size_t column = 0U; column < aes_word_size; ++column) {
        unsigned char *col = state + (column * aes_word_size);
        unsigned char a0 = col[0];
        unsigned char a1 = col[1];
        unsigned char a2 = col[2];
        unsigned char a3 = col[3];

        col[0] = (unsigned char)(aes_gf_mul(a0, 2U) ^ aes_gf_mul(a1, 3U) ^ a2 ^ a3);
        col[1] = (unsigned char)(a0 ^ aes_gf_mul(a1, 2U) ^ aes_gf_mul(a2, 3U) ^ a3);
        col[2] = (unsigned char)(a0 ^ a1 ^ aes_gf_mul(a2, 2U) ^ aes_gf_mul(a3, 3U));
        col[3] = (unsigned char)(aes_gf_mul(a0, 3U) ^ a1 ^ a2 ^ aes_gf_mul(a3, 2U));
    }
}

static void aes_inv_mix_columns(unsigned char state[aes_block_size]) {
    for (size_t column = 0U; column < aes_word_size; ++column) {
        unsigned char *col = state + (column * aes_word_size);
        unsigned char a0 = col[0];
        unsigned char a1 = col[1];
        unsigned char a2 = col[2];
        unsigned char a3 = col[3];

        col[0] = (unsigned char)(aes_gf_mul(a0, 14U) ^ aes_gf_mul(a1, 11U) ^ aes_gf_mul(a2, 13U) ^
                                 aes_gf_mul(a3, 9U));
        col[1] = (unsigned char)(aes_gf_mul(a0, 9U) ^ aes_gf_mul(a1, 14U) ^ aes_gf_mul(a2, 11U) ^
                                 aes_gf_mul(a3, 13U));
        col[2] = (unsigned char)(aes_gf_mul(a0, 13U) ^ aes_gf_mul(a1, 9U) ^ aes_gf_mul(a2, 14U) ^
                                 aes_gf_mul(a3, 11U));
        col[3] = (unsigned char)(aes_gf_mul(a0, 11U) ^ aes_gf_mul(a1, 13U) ^ aes_gf_mul(a2, 9U) ^
                                 aes_gf_mul(a3, 14U));
    }
}

static bool aes_valid_padding(const unsigned char *bytes, size_t byte_count, size_t *out_size) {
    unsigned char padding = 0U;

    if (bytes == NULL || byte_count == 0U || out_size == NULL) {
        return false;
    }
    padding = bytes[byte_count - 1U];
    if (padding == 0U || padding > aes_block_size || (size_t)padding > byte_count) {
        return false;
    }
    for (size_t index = byte_count - (size_t)padding; index < byte_count; ++index) {
        if (bytes[index] != padding) {
            return false;
        }
    }
    *out_size = byte_count - (size_t)padding;
    return true;
}

static void aes_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const enum aes_sqlite_operation *operation = sqlite3_user_data(context);
    const unsigned char *input = NULL;
    const unsigned char *key = NULL;
    unsigned char *output = NULL;
    size_t output_size = 0U;
    bool invalid = false;
    bool long_key_warning = false;
    int rc = MYLITE_OK;

    if (operation == NULL || argc != 2) {
        sqlite3_result_error(context, "invalid MyLite AES callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    input = sqlite3_value_blob(argv[0]);
    key = sqlite3_value_blob(argv[1]);
    if (*operation == AES_SQLITE_ENCRYPT) {
        rc = mylite_aes_encrypt_default(
            input,
            (size_t)sqlite3_value_bytes(argv[0]),
            key,
            (size_t)sqlite3_value_bytes(argv[1]),
            &output,
            &output_size,
            &long_key_warning
        );
    } else {
        rc = mylite_aes_decrypt_default(
            input,
            (size_t)sqlite3_value_bytes(argv[0]),
            key,
            (size_t)sqlite3_value_bytes(argv[1]),
            &output,
            &output_size,
            &invalid,
            &long_key_warning
        );
    }
    (void)long_key_warning;
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "MyLite AES function failed", -1);
        return;
    }
    if (invalid) {
        sqlite3_result_null(context);
        return;
    }
    if (output_size > (size_t)INT_MAX) {
        free(output);
        sqlite3_result_error_toobig(context);
        return;
    }
    sqlite3_result_blob(context, output, (int)output_size, free);
}

// NOLINTEND(readability-identifier-length, readability-magic-numbers)
