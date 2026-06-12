#include "mylite_digest.h"

#include "mylite_sqlite_registration.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// NOLINTBEGIN(readability-identifier-length, readability-magic-numbers): digest compression
// functions use standard state-word names, bit rotations, and round constants.

enum {
    md5_block_size = 64,
    md5_digest_size = 16,
    sha1_block_size = 64,
    sha1_digest_size = 20,
    sha224_digest_size = 28,
    sha256_block_size = 64,
    sha256_digest_size = 32,
    sha384_digest_size = 48,
    sha512_block_size = 128,
    sha512_digest_size = 64,
    digest_hex_chars_per_byte = 2,
    byte_bits = 8,
};

struct md5_context {
    uint32_t state[4];
    uint64_t bit_count;
    unsigned char buffer[md5_block_size];
    size_t buffer_length;
};

struct sha1_context {
    uint32_t state[5];
    uint64_t bit_count;
    unsigned char buffer[sha1_block_size];
    size_t buffer_length;
};

struct sha256_context {
    uint32_t state[8];
    uint64_t bit_count;
    unsigned char buffer[sha256_block_size];
    size_t buffer_length;
};

struct sha512_context {
    uint64_t state[8];
    uint64_t bit_count_high;
    uint64_t bit_count_low;
    unsigned char buffer[sha512_block_size];
    size_t buffer_length;
};

static const uint32_t md5_round_constants[64] = {
    UINT32_C(0xd76aa478), UINT32_C(0xe8c7b756), UINT32_C(0x242070db), UINT32_C(0xc1bdceee),
    UINT32_C(0xf57c0faf), UINT32_C(0x4787c62a), UINT32_C(0xa8304613), UINT32_C(0xfd469501),
    UINT32_C(0x698098d8), UINT32_C(0x8b44f7af), UINT32_C(0xffff5bb1), UINT32_C(0x895cd7be),
    UINT32_C(0x6b901122), UINT32_C(0xfd987193), UINT32_C(0xa679438e), UINT32_C(0x49b40821),
    UINT32_C(0xf61e2562), UINT32_C(0xc040b340), UINT32_C(0x265e5a51), UINT32_C(0xe9b6c7aa),
    UINT32_C(0xd62f105d), UINT32_C(0x02441453), UINT32_C(0xd8a1e681), UINT32_C(0xe7d3fbc8),
    UINT32_C(0x21e1cde6), UINT32_C(0xc33707d6), UINT32_C(0xf4d50d87), UINT32_C(0x455a14ed),
    UINT32_C(0xa9e3e905), UINT32_C(0xfcefa3f8), UINT32_C(0x676f02d9), UINT32_C(0x8d2a4c8a),
    UINT32_C(0xfffa3942), UINT32_C(0x8771f681), UINT32_C(0x6d9d6122), UINT32_C(0xfde5380c),
    UINT32_C(0xa4beea44), UINT32_C(0x4bdecfa9), UINT32_C(0xf6bb4b60), UINT32_C(0xbebfbc70),
    UINT32_C(0x289b7ec6), UINT32_C(0xeaa127fa), UINT32_C(0xd4ef3085), UINT32_C(0x04881d05),
    UINT32_C(0xd9d4d039), UINT32_C(0xe6db99e5), UINT32_C(0x1fa27cf8), UINT32_C(0xc4ac5665),
    UINT32_C(0xf4292244), UINT32_C(0x432aff97), UINT32_C(0xab9423a7), UINT32_C(0xfc93a039),
    UINT32_C(0x655b59c3), UINT32_C(0x8f0ccc92), UINT32_C(0xffeff47d), UINT32_C(0x85845dd1),
    UINT32_C(0x6fa87e4f), UINT32_C(0xfe2ce6e0), UINT32_C(0xa3014314), UINT32_C(0x4e0811a1),
    UINT32_C(0xf7537e82), UINT32_C(0xbd3af235), UINT32_C(0x2ad7d2bb), UINT32_C(0xeb86d391),
};

static const uint32_t sha256_round_constants[64] = {
    UINT32_C(0x428a2f98), UINT32_C(0x71374491), UINT32_C(0xb5c0fbcf), UINT32_C(0xe9b5dba5),
    UINT32_C(0x3956c25b), UINT32_C(0x59f111f1), UINT32_C(0x923f82a4), UINT32_C(0xab1c5ed5),
    UINT32_C(0xd807aa98), UINT32_C(0x12835b01), UINT32_C(0x243185be), UINT32_C(0x550c7dc3),
    UINT32_C(0x72be5d74), UINT32_C(0x80deb1fe), UINT32_C(0x9bdc06a7), UINT32_C(0xc19bf174),
    UINT32_C(0xe49b69c1), UINT32_C(0xefbe4786), UINT32_C(0x0fc19dc6), UINT32_C(0x240ca1cc),
    UINT32_C(0x2de92c6f), UINT32_C(0x4a7484aa), UINT32_C(0x5cb0a9dc), UINT32_C(0x76f988da),
    UINT32_C(0x983e5152), UINT32_C(0xa831c66d), UINT32_C(0xb00327c8), UINT32_C(0xbf597fc7),
    UINT32_C(0xc6e00bf3), UINT32_C(0xd5a79147), UINT32_C(0x06ca6351), UINT32_C(0x14292967),
    UINT32_C(0x27b70a85), UINT32_C(0x2e1b2138), UINT32_C(0x4d2c6dfc), UINT32_C(0x53380d13),
    UINT32_C(0x650a7354), UINT32_C(0x766a0abb), UINT32_C(0x81c2c92e), UINT32_C(0x92722c85),
    UINT32_C(0xa2bfe8a1), UINT32_C(0xa81a664b), UINT32_C(0xc24b8b70), UINT32_C(0xc76c51a3),
    UINT32_C(0xd192e819), UINT32_C(0xd6990624), UINT32_C(0xf40e3585), UINT32_C(0x106aa070),
    UINT32_C(0x19a4c116), UINT32_C(0x1e376c08), UINT32_C(0x2748774c), UINT32_C(0x34b0bcb5),
    UINT32_C(0x391c0cb3), UINT32_C(0x4ed8aa4a), UINT32_C(0x5b9cca4f), UINT32_C(0x682e6ff3),
    UINT32_C(0x748f82ee), UINT32_C(0x78a5636f), UINT32_C(0x84c87814), UINT32_C(0x8cc70208),
    UINT32_C(0x90befffa), UINT32_C(0xa4506ceb), UINT32_C(0xbef9a3f7), UINT32_C(0xc67178f2),
};

static const uint64_t sha512_round_constants[80] = {
    UINT64_C(0x428a2f98d728ae22), UINT64_C(0x7137449123ef65cd), UINT64_C(0xb5c0fbcfec4d3b2f),
    UINT64_C(0xe9b5dba58189dbbc), UINT64_C(0x3956c25bf348b538), UINT64_C(0x59f111f1b605d019),
    UINT64_C(0x923f82a4af194f9b), UINT64_C(0xab1c5ed5da6d8118), UINT64_C(0xd807aa98a3030242),
    UINT64_C(0x12835b0145706fbe), UINT64_C(0x243185be4ee4b28c), UINT64_C(0x550c7dc3d5ffb4e2),
    UINT64_C(0x72be5d74f27b896f), UINT64_C(0x80deb1fe3b1696b1), UINT64_C(0x9bdc06a725c71235),
    UINT64_C(0xc19bf174cf692694), UINT64_C(0xe49b69c19ef14ad2), UINT64_C(0xefbe4786384f25e3),
    UINT64_C(0x0fc19dc68b8cd5b5), UINT64_C(0x240ca1cc77ac9c65), UINT64_C(0x2de92c6f592b0275),
    UINT64_C(0x4a7484aa6ea6e483), UINT64_C(0x5cb0a9dcbd41fbd4), UINT64_C(0x76f988da831153b5),
    UINT64_C(0x983e5152ee66dfab), UINT64_C(0xa831c66d2db43210), UINT64_C(0xb00327c898fb213f),
    UINT64_C(0xbf597fc7beef0ee4), UINT64_C(0xc6e00bf33da88fc2), UINT64_C(0xd5a79147930aa725),
    UINT64_C(0x06ca6351e003826f), UINT64_C(0x142929670a0e6e70), UINT64_C(0x27b70a8546d22ffc),
    UINT64_C(0x2e1b21385c26c926), UINT64_C(0x4d2c6dfc5ac42aed), UINT64_C(0x53380d139d95b3df),
    UINT64_C(0x650a73548baf63de), UINT64_C(0x766a0abb3c77b2a8), UINT64_C(0x81c2c92e47edaee6),
    UINT64_C(0x92722c851482353b), UINT64_C(0xa2bfe8a14cf10364), UINT64_C(0xa81a664bbc423001),
    UINT64_C(0xc24b8b70d0f89791), UINT64_C(0xc76c51a30654be30), UINT64_C(0xd192e819d6ef5218),
    UINT64_C(0xd69906245565a910), UINT64_C(0xf40e35855771202a), UINT64_C(0x106aa07032bbd1b8),
    UINT64_C(0x19a4c116b8d2d0c8), UINT64_C(0x1e376c085141ab53), UINT64_C(0x2748774cdf8eeb99),
    UINT64_C(0x34b0bcb5e19b48a8), UINT64_C(0x391c0cb3c5c95a63), UINT64_C(0x4ed8aa4ae3418acb),
    UINT64_C(0x5b9cca4f7763e373), UINT64_C(0x682e6ff3d6b2b8a3), UINT64_C(0x748f82ee5defb2fc),
    UINT64_C(0x78a5636f43172f60), UINT64_C(0x84c87814a1f0ab72), UINT64_C(0x8cc702081a6439ec),
    UINT64_C(0x90befffa23631e28), UINT64_C(0xa4506cebde82bde9), UINT64_C(0xbef9a3f7b2c67915),
    UINT64_C(0xc67178f2e372532b), UINT64_C(0xca273eceea26619c), UINT64_C(0xd186b8c721c0c207),
    UINT64_C(0xeada7dd6cde0eb1e), UINT64_C(0xf57d4f7fee6ed178), UINT64_C(0x06f067aa72176fba),
    UINT64_C(0x0a637dc5a2c898a6), UINT64_C(0x113f9804bef90dae), UINT64_C(0x1b710b35131c471b),
    UINT64_C(0x28db77f523047d84), UINT64_C(0x32caab7b40c72493), UINT64_C(0x3c9ebe0a15c9bebc),
    UINT64_C(0x431d67c49c100d4c), UINT64_C(0x4cc5d4becb3e42b6), UINT64_C(0x597f299cfc657e2a),
    UINT64_C(0x5fcb6fab3ad6faec), UINT64_C(0x6c44198c4a475817),
};

static const enum mylite_digest_algorithm sqlite_md5_algorithm = MYLITE_DIGEST_ALGORITHM_MD5;
static const enum mylite_digest_algorithm sqlite_sha_algorithm = MYLITE_DIGEST_ALGORITHM_SHA1;
static const enum mylite_digest_algorithm sqlite_sha1_algorithm = MYLITE_DIGEST_ALGORITHM_SHA1;

static int digest_hex_alloc(
    enum mylite_digest_algorithm algorithm,
    const unsigned char *digest,
    size_t digest_length,
    char **out_text,
    size_t *out_text_length
);
static void digest_bytes_to_lower_hex(
    const unsigned char *digest,
    size_t digest_length,
    char *out_text
);
static size_t digest_size(enum mylite_digest_algorithm algorithm);
static void md5_digest(const unsigned char *bytes, size_t byte_count, unsigned char out[16]);
static void md5_init(struct md5_context *context);
static void md5_update(struct md5_context *context, const unsigned char *bytes, size_t byte_count);
static void md5_final(struct md5_context *context, unsigned char out[16]);
static void md5_transform(uint32_t state[4], const unsigned char block[64]);
static uint32_t md5_left_rotate(uint32_t value, unsigned int bits);
static uint32_t load_le32(const unsigned char *bytes);
static void store_le64(unsigned char *bytes, uint64_t value);
static void sha1_digest(const unsigned char *bytes, size_t byte_count, unsigned char out[20]);
static void sha1_init(struct sha1_context *context);
static void sha1_update(
    struct sha1_context *context,
    const unsigned char *bytes,
    size_t byte_count
);
static void sha1_final(struct sha1_context *context, unsigned char out[20]);
static void sha1_transform(uint32_t state[5], const unsigned char block[64]);
static uint32_t rotate_left32(uint32_t value, unsigned int bits);
static uint32_t rotate_right32(uint32_t value, unsigned int bits);
static uint64_t rotate_right64(uint64_t value, unsigned int bits);
static uint32_t load_be32(const unsigned char *bytes);
static uint64_t load_be64(const unsigned char *bytes);
static void store_be32(unsigned char *bytes, uint32_t value);
static void store_be64(unsigned char *bytes, uint64_t value);
static void sha256_digest(
    const unsigned char *bytes,
    size_t byte_count,
    bool sha224,
    unsigned char *out
);
static void sha256_init(struct sha256_context *context, bool sha224);
static void sha256_update(
    struct sha256_context *context,
    const unsigned char *bytes,
    size_t byte_count
);
static void sha256_final(struct sha256_context *context, bool sha224, unsigned char *out);
static void sha256_transform(uint32_t state[8], const unsigned char block[64]);
static void sha512_digest(
    const unsigned char *bytes,
    size_t byte_count,
    bool sha384,
    unsigned char *out
);
static void sha512_init(struct sha512_context *context, bool sha384);
static void sha512_update(
    struct sha512_context *context,
    const unsigned char *bytes,
    size_t byte_count
);
static void sha512_final(struct sha512_context *context, bool sha384, unsigned char *out);
static void sha512_add_bits(struct sha512_context *context, size_t byte_count);
static void sha512_transform(uint64_t state[8], const unsigned char block[128]);
static void digest_one_arg_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static void digest_sha2_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int digest_sqlite_value_bytes(
    sqlite3_value *value,
    const unsigned char **out_bytes,
    size_t *out_byte_count
);
static bool digest_sha2_algorithm_from_sqlite_value(
    sqlite3_value *value,
    enum mylite_digest_algorithm *out_algorithm,
    bool *out_is_null
);
static void digest_sqlite_result(
    sqlite3_context *context,
    enum mylite_digest_algorithm algorithm,
    const unsigned char *bytes,
    size_t byte_count
);

size_t mylite_digest_hex_length(enum mylite_digest_algorithm algorithm) {
    return digest_size(algorithm) * digest_hex_chars_per_byte;
}

int mylite_digest_hex_value(
    enum mylite_digest_algorithm algorithm,
    const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    size_t *out_text_length
) {
    unsigned char digest[sha512_digest_size];
    size_t length = digest_size(algorithm);

    if ((bytes == NULL && byte_count != 0U) || out_text == NULL || out_text_length == NULL ||
        length == 0U) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;

    switch (algorithm) {
    case MYLITE_DIGEST_ALGORITHM_MD5:
        md5_digest(bytes, byte_count, digest);
        break;
    case MYLITE_DIGEST_ALGORITHM_SHA1:
        sha1_digest(bytes, byte_count, digest);
        break;
    case MYLITE_DIGEST_ALGORITHM_SHA224:
        sha256_digest(bytes, byte_count, true, digest);
        break;
    case MYLITE_DIGEST_ALGORITHM_SHA256:
        sha256_digest(bytes, byte_count, false, digest);
        break;
    case MYLITE_DIGEST_ALGORITHM_SHA384:
        sha512_digest(bytes, byte_count, true, digest);
        break;
    case MYLITE_DIGEST_ALGORITHM_SHA512:
        sha512_digest(bytes, byte_count, false, digest);
        break;
    }

    return digest_hex_alloc(algorithm, digest, length, out_text, out_text_length);
}

int mylite_sqlite_register_digest_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_md5",
            .argument_count = 1,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = (void *)&sqlite_md5_algorithm,
            .scalar_callback = digest_one_arg_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_sha",
            .argument_count = 1,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = (void *)&sqlite_sha_algorithm,
            .scalar_callback = digest_one_arg_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_sha1",
            .argument_count = 1,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = (void *)&sqlite_sha1_algorithm,
            .scalar_callback = digest_one_arg_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_sha2",
            .argument_count = 2,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = NULL,
            .scalar_callback = digest_sha2_sqlite_callback,
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

static int digest_hex_alloc(
    enum mylite_digest_algorithm algorithm,
    const unsigned char *digest,
    size_t digest_length,
    char **out_text,
    size_t *out_text_length
) {
    char *text = NULL;
    size_t text_length = mylite_digest_hex_length(algorithm);

    if (digest == NULL || out_text == NULL || out_text_length == NULL ||
        text_length != digest_length * digest_hex_chars_per_byte) {
        return MYLITE_MISUSE;
    }
    if (text_length == SIZE_MAX) {
        return MYLITE_NOMEM;
    }
    text = (char *)malloc(text_length + 1U);
    if (text == NULL) {
        return MYLITE_NOMEM;
    }
    digest_bytes_to_lower_hex(digest, digest_length, text);
    text[text_length] = '\0';

    *out_text = text;
    *out_text_length = text_length;
    return MYLITE_OK;
}

static void digest_bytes_to_lower_hex(
    const unsigned char *digest,
    size_t digest_length,
    char *out_text
) {
    static const char digits[] = "0123456789abcdef";

    for (size_t index = 0U; index < digest_length; ++index) {
        out_text[index * 2U] = digits[digest[index] >> 4U];
        out_text[(index * 2U) + 1U] = digits[digest[index] & 0x0fU];
    }
}

static size_t digest_size(enum mylite_digest_algorithm algorithm) {
    switch (algorithm) {
    case MYLITE_DIGEST_ALGORITHM_MD5:
        return md5_digest_size;
    case MYLITE_DIGEST_ALGORITHM_SHA1:
        return sha1_digest_size;
    case MYLITE_DIGEST_ALGORITHM_SHA224:
        return sha224_digest_size;
    case MYLITE_DIGEST_ALGORITHM_SHA256:
        return sha256_digest_size;
    case MYLITE_DIGEST_ALGORITHM_SHA384:
        return sha384_digest_size;
    case MYLITE_DIGEST_ALGORITHM_SHA512:
        return sha512_digest_size;
    }
    return 0U;
}

static void md5_digest(const unsigned char *bytes, size_t byte_count, unsigned char out[16]) {
    struct md5_context context;

    md5_init(&context);
    md5_update(&context, bytes, byte_count);
    md5_final(&context, out);
}

static void md5_init(struct md5_context *context) {
    *context = (struct md5_context){
        .state =
            {
                UINT32_C(0x67452301),
                UINT32_C(0xefcdab89),
                UINT32_C(0x98badcfe),
                UINT32_C(0x10325476),
            },
        .bit_count = 0U,
        .buffer = {0},
        .buffer_length = 0U,
    };
}

static void md5_update(struct md5_context *context, const unsigned char *bytes, size_t byte_count) {
    size_t offset = 0U;

    context->bit_count += (uint64_t)byte_count * byte_bits;
    if (context->buffer_length != 0U) {
        size_t space = md5_block_size - context->buffer_length;
        size_t copy_count = byte_count < space ? byte_count : space;

        if (copy_count != 0U) {
            memcpy(context->buffer + context->buffer_length, bytes, copy_count);
        }
        context->buffer_length += copy_count;
        offset += copy_count;
        if (context->buffer_length == md5_block_size) {
            md5_transform(context->state, context->buffer);
            context->buffer_length = 0U;
        }
    }
    while (offset + md5_block_size <= byte_count) {
        md5_transform(context->state, bytes + offset);
        offset += md5_block_size;
    }
    if (offset < byte_count) {
        context->buffer_length = byte_count - offset;
        memcpy(context->buffer, bytes + offset, context->buffer_length);
    }
}

static void md5_final(struct md5_context *context, unsigned char out[16]) {
    unsigned char length[8];
    unsigned char padding[md5_block_size] = {0x80};
    size_t pad_length = context->buffer_length < 56U
                            ? 56U - context->buffer_length
                            : (md5_block_size + 56U) - context->buffer_length;

    store_le64(length, context->bit_count);
    md5_update(context, padding, pad_length);
    md5_update(context, length, sizeof(length));
    for (size_t index = 0U; index < 4U; ++index) {
        uint32_t value = context->state[index];

        out[(index * 4U)] = (unsigned char)(value & 0xffU);
        out[(index * 4U) + 1U] = (unsigned char)((value >> 8U) & 0xffU);
        out[(index * 4U) + 2U] = (unsigned char)((value >> 16U) & 0xffU);
        out[(index * 4U) + 3U] = (unsigned char)((value >> 24U) & 0xffU);
    }
}

static void md5_transform(uint32_t state[4], const unsigned char block[64]) {
    static const unsigned int shifts[64] = {
        7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U,
        5U, 9U,  14U, 20U, 5U, 9U,  14U, 20U, 5U, 9U,  14U, 20U, 5U, 9U,  14U, 20U,
        4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U,
        6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U,
    };
    uint32_t words[16];
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];

    for (size_t index = 0U; index < 16U; ++index) {
        words[index] = load_le32(block + (index * 4U));
    }

    for (size_t index = 0U; index < 64U; ++index) {
        uint32_t f = 0U;
        size_t word_index = 0U;
        uint32_t next = 0U;

        if (index < 16U) {
            f = (b & c) | ((~b) & d);
            word_index = index;
        } else if (index < 32U) {
            f = (d & b) | ((~d) & c);
            word_index = ((5U * index) + 1U) % 16U;
        } else if (index < 48U) {
            f = b ^ c ^ d;
            word_index = ((3U * index) + 5U) % 16U;
        } else {
            f = c ^ (b | (~d));
            word_index = (7U * index) % 16U;
        }

        next = d;
        d = c;
        c = b;
        b += md5_left_rotate(a + f + md5_round_constants[index] + words[word_index], shifts[index]);
        a = next;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

static uint32_t md5_left_rotate(uint32_t value, unsigned int bits) {
    return (value << bits) | (value >> (32U - bits));
}

static uint32_t load_le32(const unsigned char *bytes) {
    return ((uint32_t)bytes[0]) | ((uint32_t)bytes[1] << 8U) | ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static void store_le64(unsigned char *bytes, uint64_t value) {
    for (size_t index = 0U; index < 8U; ++index) {
        bytes[index] = (unsigned char)((value >> (index * 8U)) & UINT64_C(0xff));
    }
}

static void sha1_digest(const unsigned char *bytes, size_t byte_count, unsigned char out[20]) {
    struct sha1_context context;

    sha1_init(&context);
    sha1_update(&context, bytes, byte_count);
    sha1_final(&context, out);
}

static void sha1_init(struct sha1_context *context) {
    *context = (struct sha1_context){
        .state =
            {
                UINT32_C(0x67452301),
                UINT32_C(0xefcdab89),
                UINT32_C(0x98badcfe),
                UINT32_C(0x10325476),
                UINT32_C(0xc3d2e1f0),
            },
        .bit_count = 0U,
        .buffer = {0},
        .buffer_length = 0U,
    };
}

static void sha1_update(
    struct sha1_context *context,
    const unsigned char *bytes,
    size_t byte_count
) {
    size_t offset = 0U;

    context->bit_count += (uint64_t)byte_count * byte_bits;
    if (context->buffer_length != 0U) {
        size_t space = sha1_block_size - context->buffer_length;
        size_t copy_count = byte_count < space ? byte_count : space;

        if (copy_count != 0U) {
            memcpy(context->buffer + context->buffer_length, bytes, copy_count);
        }
        context->buffer_length += copy_count;
        offset += copy_count;
        if (context->buffer_length == sha1_block_size) {
            sha1_transform(context->state, context->buffer);
            context->buffer_length = 0U;
        }
    }
    while (offset + sha1_block_size <= byte_count) {
        sha1_transform(context->state, bytes + offset);
        offset += sha1_block_size;
    }
    if (offset < byte_count) {
        context->buffer_length = byte_count - offset;
        memcpy(context->buffer, bytes + offset, context->buffer_length);
    }
}

static void sha1_final(struct sha1_context *context, unsigned char out[20]) {
    unsigned char length[8];
    unsigned char padding[sha1_block_size] = {0x80};
    size_t pad_length = context->buffer_length < 56U
                            ? 56U - context->buffer_length
                            : (sha1_block_size + 56U) - context->buffer_length;

    store_be64(length, context->bit_count);
    sha1_update(context, padding, pad_length);
    sha1_update(context, length, sizeof(length));
    for (size_t index = 0U; index < 5U; ++index) {
        store_be32(out + (index * 4U), context->state[index]);
    }
}

static void sha1_transform(uint32_t state[5], const unsigned char block[64]) {
    uint32_t words[80];
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];

    for (size_t index = 0U; index < 16U; ++index) {
        words[index] = load_be32(block + (index * 4U));
    }
    for (size_t index = 16U; index < 80U; ++index) {
        words[index] = rotate_left32(
            words[index - 3U] ^ words[index - 8U] ^ words[index - 14U] ^ words[index - 16U],
            1U
        );
    }

    for (size_t index = 0U; index < 80U; ++index) {
        uint32_t f = 0U;
        uint32_t k = 0U;
        uint32_t temp = 0U;

        if (index < 20U) {
            f = (b & c) | ((~b) & d);
            k = UINT32_C(0x5a827999);
        } else if (index < 40U) {
            f = b ^ c ^ d;
            k = UINT32_C(0x6ed9eba1);
        } else if (index < 60U) {
            f = (b & c) | (b & d) | (c & d);
            k = UINT32_C(0x8f1bbcdc);
        } else {
            f = b ^ c ^ d;
            k = UINT32_C(0xca62c1d6);
        }
        temp = rotate_left32(a, 5U) + f + e + k + words[index];
        e = d;
        d = c;
        c = rotate_left32(b, 30U);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

static uint32_t rotate_left32(uint32_t value, unsigned int bits) {
    return (value << bits) | (value >> (32U - bits));
}

static uint32_t rotate_right32(uint32_t value, unsigned int bits) {
    return (value >> bits) | (value << (32U - bits));
}

static uint64_t rotate_right64(uint64_t value, unsigned int bits) {
    return (value >> bits) | (value << (64U - bits));
}

static uint32_t load_be32(const unsigned char *bytes) {
    return ((uint32_t)bytes[0] << 24U) | ((uint32_t)bytes[1] << 16U) | ((uint32_t)bytes[2] << 8U) |
           ((uint32_t)bytes[3]);
}

static uint64_t load_be64(const unsigned char *bytes) {
    return ((uint64_t)bytes[0] << 56U) | ((uint64_t)bytes[1] << 48U) | ((uint64_t)bytes[2] << 40U) |
           ((uint64_t)bytes[3] << 32U) | ((uint64_t)bytes[4] << 24U) | ((uint64_t)bytes[5] << 16U) |
           ((uint64_t)bytes[6] << 8U) | ((uint64_t)bytes[7]);
}

static void store_be32(unsigned char *bytes, uint32_t value) {
    bytes[0] = (unsigned char)((value >> 24U) & UINT32_C(0xff));
    bytes[1] = (unsigned char)((value >> 16U) & UINT32_C(0xff));
    bytes[2] = (unsigned char)((value >> 8U) & UINT32_C(0xff));
    bytes[3] = (unsigned char)(value & UINT32_C(0xff));
}

static void store_be64(unsigned char *bytes, uint64_t value) {
    bytes[0] = (unsigned char)((value >> 56U) & UINT64_C(0xff));
    bytes[1] = (unsigned char)((value >> 48U) & UINT64_C(0xff));
    bytes[2] = (unsigned char)((value >> 40U) & UINT64_C(0xff));
    bytes[3] = (unsigned char)((value >> 32U) & UINT64_C(0xff));
    bytes[4] = (unsigned char)((value >> 24U) & UINT64_C(0xff));
    bytes[5] = (unsigned char)((value >> 16U) & UINT64_C(0xff));
    bytes[6] = (unsigned char)((value >> 8U) & UINT64_C(0xff));
    bytes[7] = (unsigned char)(value & UINT64_C(0xff));
}

static void sha256_digest(
    const unsigned char *bytes,
    size_t byte_count,
    bool sha224,
    unsigned char *out
) {
    struct sha256_context context;

    sha256_init(&context, sha224);
    sha256_update(&context, bytes, byte_count);
    sha256_final(&context, sha224, out);
}

static void sha256_init(struct sha256_context *context, bool sha224) {
    if (sha224) {
        *context = (struct sha256_context){
            .state =
                {
                    UINT32_C(0xc1059ed8),
                    UINT32_C(0x367cd507),
                    UINT32_C(0x3070dd17),
                    UINT32_C(0xf70e5939),
                    UINT32_C(0xffc00b31),
                    UINT32_C(0x68581511),
                    UINT32_C(0x64f98fa7),
                    UINT32_C(0xbefa4fa4),
                },
            .bit_count = 0U,
            .buffer = {0},
            .buffer_length = 0U,
        };
        return;
    }

    *context = (struct sha256_context){
        .state =
            {
                UINT32_C(0x6a09e667),
                UINT32_C(0xbb67ae85),
                UINT32_C(0x3c6ef372),
                UINT32_C(0xa54ff53a),
                UINT32_C(0x510e527f),
                UINT32_C(0x9b05688c),
                UINT32_C(0x1f83d9ab),
                UINT32_C(0x5be0cd19),
            },
        .bit_count = 0U,
        .buffer = {0},
        .buffer_length = 0U,
    };
}

static void sha256_update(
    struct sha256_context *context,
    const unsigned char *bytes,
    size_t byte_count
) {
    size_t offset = 0U;

    context->bit_count += (uint64_t)byte_count * byte_bits;
    if (context->buffer_length != 0U) {
        size_t space = sha256_block_size - context->buffer_length;
        size_t copy_count = byte_count < space ? byte_count : space;

        if (copy_count != 0U) {
            memcpy(context->buffer + context->buffer_length, bytes, copy_count);
        }
        context->buffer_length += copy_count;
        offset += copy_count;
        if (context->buffer_length == sha256_block_size) {
            sha256_transform(context->state, context->buffer);
            context->buffer_length = 0U;
        }
    }
    while (offset + sha256_block_size <= byte_count) {
        sha256_transform(context->state, bytes + offset);
        offset += sha256_block_size;
    }
    if (offset < byte_count) {
        context->buffer_length = byte_count - offset;
        memcpy(context->buffer, bytes + offset, context->buffer_length);
    }
}

static void sha256_final(struct sha256_context *context, bool sha224, unsigned char *out) {
    unsigned char length[8];
    unsigned char padding[sha256_block_size] = {0x80};
    size_t pad_length = context->buffer_length < 56U
                            ? 56U - context->buffer_length
                            : (sha256_block_size + 56U) - context->buffer_length;
    size_t word_count = sha224 ? 7U : 8U;

    store_be64(length, context->bit_count);
    sha256_update(context, padding, pad_length);
    sha256_update(context, length, sizeof(length));
    for (size_t index = 0U; index < word_count; ++index) {
        store_be32(out + (index * 4U), context->state[index]);
    }
}

static void sha256_transform(uint32_t state[8], const unsigned char block[64]) {
    uint32_t words[64];
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];

    for (size_t index = 0U; index < 16U; ++index) {
        words[index] = load_be32(block + (index * 4U));
    }
    for (size_t index = 16U; index < 64U; ++index) {
        uint32_t s0 = rotate_right32(words[index - 15U], 7U) ^
                      rotate_right32(words[index - 15U], 18U) ^ (words[index - 15U] >> 3U);
        uint32_t s1 = rotate_right32(words[index - 2U], 17U) ^
                      rotate_right32(words[index - 2U], 19U) ^ (words[index - 2U] >> 10U);

        words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
    }

    for (size_t index = 0U; index < 64U; ++index) {
        uint32_t s1 = rotate_right32(e, 6U) ^ rotate_right32(e, 11U) ^ rotate_right32(e, 25U);
        uint32_t choice = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + choice + sha256_round_constants[index] + words[index];
        uint32_t s0 = rotate_right32(a, 2U) ^ rotate_right32(a, 13U) ^ rotate_right32(a, 22U);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + majority;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

static void sha512_digest(
    const unsigned char *bytes,
    size_t byte_count,
    bool sha384,
    unsigned char *out
) {
    struct sha512_context context;

    sha512_init(&context, sha384);
    sha512_update(&context, bytes, byte_count);
    sha512_final(&context, sha384, out);
}

static void sha512_init(struct sha512_context *context, bool sha384) {
    if (sha384) {
        *context = (struct sha512_context){
            .state =
                {
                    UINT64_C(0xcbbb9d5dc1059ed8),
                    UINT64_C(0x629a292a367cd507),
                    UINT64_C(0x9159015a3070dd17),
                    UINT64_C(0x152fecd8f70e5939),
                    UINT64_C(0x67332667ffc00b31),
                    UINT64_C(0x8eb44a8768581511),
                    UINT64_C(0xdb0c2e0d64f98fa7),
                    UINT64_C(0x47b5481dbefa4fa4),
                },
            .bit_count_high = 0U,
            .bit_count_low = 0U,
            .buffer = {0},
            .buffer_length = 0U,
        };
        return;
    }

    *context = (struct sha512_context){
        .state =
            {
                UINT64_C(0x6a09e667f3bcc908),
                UINT64_C(0xbb67ae8584caa73b),
                UINT64_C(0x3c6ef372fe94f82b),
                UINT64_C(0xa54ff53a5f1d36f1),
                UINT64_C(0x510e527fade682d1),
                UINT64_C(0x9b05688c2b3e6c1f),
                UINT64_C(0x1f83d9abfb41bd6b),
                UINT64_C(0x5be0cd19137e2179),
            },
        .bit_count_high = 0U,
        .bit_count_low = 0U,
        .buffer = {0},
        .buffer_length = 0U,
    };
}

static void sha512_update(
    struct sha512_context *context,
    const unsigned char *bytes,
    size_t byte_count
) {
    size_t offset = 0U;

    sha512_add_bits(context, byte_count);
    if (context->buffer_length != 0U) {
        size_t space = sha512_block_size - context->buffer_length;
        size_t copy_count = byte_count < space ? byte_count : space;

        if (copy_count != 0U) {
            memcpy(context->buffer + context->buffer_length, bytes, copy_count);
        }
        context->buffer_length += copy_count;
        offset += copy_count;
        if (context->buffer_length == sha512_block_size) {
            sha512_transform(context->state, context->buffer);
            context->buffer_length = 0U;
        }
    }
    while (offset + sha512_block_size <= byte_count) {
        sha512_transform(context->state, bytes + offset);
        offset += sha512_block_size;
    }
    if (offset < byte_count) {
        context->buffer_length = byte_count - offset;
        memcpy(context->buffer, bytes + offset, context->buffer_length);
    }
}

static void sha512_final(struct sha512_context *context, bool sha384, unsigned char *out) {
    unsigned char length[16];
    unsigned char padding[sha512_block_size] = {0x80};
    size_t pad_length = context->buffer_length < 112U
                            ? 112U - context->buffer_length
                            : (sha512_block_size + 112U) - context->buffer_length;
    size_t word_count = sha384 ? 6U : 8U;

    store_be64(length, context->bit_count_high);
    store_be64(length + 8U, context->bit_count_low);
    sha512_update(context, padding, pad_length);
    sha512_update(context, length, sizeof(length));
    for (size_t index = 0U; index < word_count; ++index) {
        store_be64(out + (index * 8U), context->state[index]);
    }
}

static void sha512_add_bits(struct sha512_context *context, size_t byte_count) {
    uint64_t low = context->bit_count_low;
    uint64_t added_low = (uint64_t)byte_count << 3U;
    uint64_t added_high = (uint64_t)byte_count >> 61U;

    context->bit_count_low += added_low;
    context->bit_count_high += added_high;
    if (context->bit_count_low < low) {
        ++context->bit_count_high;
    }
}

static void sha512_transform(uint64_t state[8], const unsigned char block[128]) {
    uint64_t words[80];
    uint64_t a = state[0];
    uint64_t b = state[1];
    uint64_t c = state[2];
    uint64_t d = state[3];
    uint64_t e = state[4];
    uint64_t f = state[5];
    uint64_t g = state[6];
    uint64_t h = state[7];

    for (size_t index = 0U; index < 16U; ++index) {
        words[index] = load_be64(block + (index * 8U));
    }
    for (size_t index = 16U; index < 80U; ++index) {
        uint64_t s0 = rotate_right64(words[index - 15U], 1U) ^
                      rotate_right64(words[index - 15U], 8U) ^ (words[index - 15U] >> 7U);
        uint64_t s1 = rotate_right64(words[index - 2U], 19U) ^
                      rotate_right64(words[index - 2U], 61U) ^ (words[index - 2U] >> 6U);

        words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
    }

    for (size_t index = 0U; index < 80U; ++index) {
        uint64_t s1 = rotate_right64(e, 14U) ^ rotate_right64(e, 18U) ^ rotate_right64(e, 41U);
        uint64_t choice = (e & f) ^ ((~e) & g);
        uint64_t temp1 = h + s1 + choice + sha512_round_constants[index] + words[index];
        uint64_t s0 = rotate_right64(a, 28U) ^ rotate_right64(a, 34U) ^ rotate_right64(a, 39U);
        uint64_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint64_t temp2 = s0 + majority;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

static void digest_one_arg_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    const enum mylite_digest_algorithm *algorithm = NULL;
    const unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite digest callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    algorithm = (const enum mylite_digest_algorithm *)sqlite3_user_data(context);
    if (algorithm == NULL) {
        sqlite3_result_error(context, "invalid MyLite digest algorithm", -1);
        return;
    }

    rc = digest_sqlite_value_bytes(argv[0], &bytes, &byte_count);
    if (rc != MYLITE_OK) {
        sqlite3_result_error_nomem(context);
        return;
    }
    digest_sqlite_result(context, *algorithm, bytes, byte_count);
}

static void digest_sha2_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    enum mylite_digest_algorithm algorithm = MYLITE_DIGEST_ALGORITHM_SHA256;
    const unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        sqlite3_result_error(context, "invalid MyLite SHA2 callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (!digest_sha2_algorithm_from_sqlite_value(argv[1], &algorithm, &is_null) || is_null) {
        sqlite3_result_null(context);
        return;
    }

    rc = digest_sqlite_value_bytes(argv[0], &bytes, &byte_count);
    if (rc != MYLITE_OK) {
        sqlite3_result_error_nomem(context);
        return;
    }
    digest_sqlite_result(context, algorithm, bytes, byte_count);
}

static int digest_sqlite_value_bytes(
    sqlite3_value *value,
    const unsigned char **out_bytes,
    size_t *out_byte_count
) {
    int byte_count = 0;

    if (value == NULL || out_bytes == NULL || out_byte_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_bytes = NULL;
    *out_byte_count = 0U;

    if (sqlite3_value_type(value) == SQLITE_BLOB) {
        const void *blob = sqlite3_value_blob(value);

        byte_count = sqlite3_value_bytes(value);
        if (blob == NULL && byte_count != 0) {
            return MYLITE_NOMEM;
        }
        *out_bytes = (const unsigned char *)blob;
        *out_byte_count = (size_t)byte_count;
        return MYLITE_OK;
    }

    *out_bytes = sqlite3_value_text(value);
    byte_count = sqlite3_value_bytes(value);
    if (*out_bytes == NULL && byte_count != 0) {
        return MYLITE_NOMEM;
    }
    *out_byte_count = (size_t)byte_count;
    return MYLITE_OK;
}

static bool digest_sha2_algorithm_from_sqlite_value(
    sqlite3_value *value,
    enum mylite_digest_algorithm *out_algorithm,
    bool *out_is_null
) {
    sqlite3_int64 length = 0;

    if (value == NULL || out_algorithm == NULL || out_is_null == NULL) {
        return false;
    }
    *out_algorithm = MYLITE_DIGEST_ALGORITHM_SHA256;
    *out_is_null = false;
    if (sqlite3_value_type(value) == SQLITE_NULL) {
        *out_is_null = true;
        return true;
    }
    length = sqlite3_value_int64(value);
    switch (length) {
    case MYLITE_DIGEST_SHA2_LENGTH_DEFAULT:
    case MYLITE_DIGEST_SHA2_LENGTH_SHA256:
        *out_algorithm = MYLITE_DIGEST_ALGORITHM_SHA256;
        return true;
    case MYLITE_DIGEST_SHA2_LENGTH_SHA224:
        *out_algorithm = MYLITE_DIGEST_ALGORITHM_SHA224;
        return true;
    case MYLITE_DIGEST_SHA2_LENGTH_SHA384:
        *out_algorithm = MYLITE_DIGEST_ALGORITHM_SHA384;
        return true;
    case MYLITE_DIGEST_SHA2_LENGTH_SHA512:
        *out_algorithm = MYLITE_DIGEST_ALGORITHM_SHA512;
        return true;
    default:
        break;
    }
    return false;
}

static void digest_sqlite_result(
    sqlite3_context *context,
    enum mylite_digest_algorithm algorithm,
    const unsigned char *bytes,
    size_t byte_count
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = mylite_digest_hex_value(algorithm, bytes, byte_count, &text, &text_length);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "MyLite digest failed", -1);
        }
        free(text);
        return;
    }

    sqlite3_result_text64(context, text, (sqlite3_uint64)text_length, free, SQLITE_UTF8);
}

// NOLINTEND(readability-identifier-length, readability-magic-numbers)
