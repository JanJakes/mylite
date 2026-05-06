#include "mylite_digest.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

static uint32_t rotate_left32(uint32_t value, unsigned int count);

static void md5_process_block(uint32_t state[4], const unsigned char block[64]);

static void md5_write_digest(const uint32_t state[4], unsigned char digest[16]);

static uint32_t load_le32(const unsigned char *bytes);

static void store_le64(unsigned char *bytes, uint64_t value);

static void sha1_process_block(uint32_t state[5], const unsigned char block[64]);

static void sha1_write_digest(const uint32_t state[5], unsigned char digest[20]);

static uint32_t load_be32(const unsigned char *bytes);

static void store_be64(unsigned char *bytes, uint64_t value);

static void sha256_process_block(uint32_t state[8], const unsigned char block[64]);

static void sha256_digest(
    const unsigned char *data,
    size_t length,
    const uint32_t initial[8],
    unsigned char digest[32]
);

static uint32_t rotate_right32(uint32_t value, unsigned int count);

static void store_be32(unsigned char *bytes, uint32_t value);

static void sha512_process_block(uint64_t state[8], const unsigned char block[128]);

static void sha512_digest(
    const unsigned char *data,
    size_t length,
    const uint64_t initial[8],
    unsigned char digest[64]
);

static uint64_t load_be64(const unsigned char *bytes);

static uint64_t rotate_right64(uint64_t value, unsigned int count);

static void write_lower_hex(const unsigned char *bytes, size_t length, char *out_hex);

void mylite_digest_md5_hex(const unsigned char *data, size_t length, char *out_hex) {
    uint32_t state[4] = {
        UINT32_C(0x67452301),
        UINT32_C(0xefcdab89),
        UINT32_C(0x98badcfe),
        UINT32_C(0x10325476),
    };
    unsigned char block[64] = {0};
    uint64_t bit_length = (uint64_t)length * CHAR_BIT;
    size_t offset = 0U;
    size_t remaining = 0U;
    unsigned char digest[16] = {0};

    while (length - offset >= sizeof(block)) {
        md5_process_block(state, data + offset);
        offset += sizeof(block);
    }

    remaining = length - offset;
    if (remaining != 0U) {
        memcpy(block, data + offset, remaining);
    }
    block[remaining] = 0x80U;
    if (remaining >= 56U) {
        md5_process_block(state, block);
        memset(block, 0, sizeof(block));
    }
    store_le64(block + 56U, bit_length);
    md5_process_block(state, block);

    md5_write_digest(state, digest);
    write_lower_hex(digest, sizeof(digest), out_hex);
}

void mylite_digest_sha1_hex(const unsigned char *data, size_t length, char *out_hex) {
    uint32_t state[5] = {
        UINT32_C(0x67452301),
        UINT32_C(0xefcdab89),
        UINT32_C(0x98badcfe),
        UINT32_C(0x10325476),
        UINT32_C(0xc3d2e1f0),
    };
    unsigned char block[64] = {0};
    uint64_t bit_length = (uint64_t)length * CHAR_BIT;
    size_t offset = 0U;
    size_t remaining = 0U;
    unsigned char digest[20] = {0};

    while (length - offset >= sizeof(block)) {
        sha1_process_block(state, data + offset);
        offset += sizeof(block);
    }

    remaining = length - offset;
    if (remaining != 0U) {
        memcpy(block, data + offset, remaining);
    }
    block[remaining] = 0x80U;
    if (remaining >= 56U) {
        sha1_process_block(state, block);
        memset(block, 0, sizeof(block));
    }
    store_be64(block + 56U, bit_length);
    sha1_process_block(state, block);

    sha1_write_digest(state, digest);
    write_lower_hex(digest, sizeof(digest), out_hex);
}

bool mylite_digest_sha2_hex(
    const unsigned char *data,
    size_t length,
    unsigned int bits,
    char *out_hex,
    size_t *out_length
) {
    static const uint32_t sha224_initial[8] = {
        UINT32_C(0xc1059ed8),
        UINT32_C(0x367cd507),
        UINT32_C(0x3070dd17),
        UINT32_C(0xf70e5939),
        UINT32_C(0xffc00b31),
        UINT32_C(0x68581511),
        UINT32_C(0x64f98fa7),
        UINT32_C(0xbefa4fa4),
    };
    static const uint32_t sha256_initial[8] = {
        UINT32_C(0x6a09e667),
        UINT32_C(0xbb67ae85),
        UINT32_C(0x3c6ef372),
        UINT32_C(0xa54ff53a),
        UINT32_C(0x510e527f),
        UINT32_C(0x9b05688c),
        UINT32_C(0x1f83d9ab),
        UINT32_C(0x5be0cd19),
    };
    static const uint64_t sha384_initial[8] = {
        UINT64_C(0xcbbb9d5dc1059ed8),
        UINT64_C(0x629a292a367cd507),
        UINT64_C(0x9159015a3070dd17),
        UINT64_C(0x152fecd8f70e5939),
        UINT64_C(0x67332667ffc00b31),
        UINT64_C(0x8eb44a8768581511),
        UINT64_C(0xdb0c2e0d64f98fa7),
        UINT64_C(0x47b5481dbefa4fa4),
    };
    static const uint64_t sha512_initial[8] = {
        UINT64_C(0x6a09e667f3bcc908),
        UINT64_C(0xbb67ae8584caa73b),
        UINT64_C(0x3c6ef372fe94f82b),
        UINT64_C(0xa54ff53a5f1d36f1),
        UINT64_C(0x510e527fade682d1),
        UINT64_C(0x9b05688c2b3e6c1f),
        UINT64_C(0x1f83d9abfb41bd6b),
        UINT64_C(0x5be0cd19137e2179),
    };
    unsigned char digest64[64] = {0};
    unsigned char digest32[32] = {0};

    if (out_hex == NULL || out_length == NULL) {
        return false;
    }
    switch (bits) {
    case 224U:
        sha256_digest(data, length, sha224_initial, digest32);
        write_lower_hex(digest32, 28U, out_hex);
        *out_length = MYLITE_DIGEST_SHA2_224_HEX_LENGTH;
        return true;
    case 0U:
    case 256U:
        sha256_digest(data, length, sha256_initial, digest32);
        write_lower_hex(digest32, 32U, out_hex);
        *out_length = MYLITE_DIGEST_SHA2_256_HEX_LENGTH;
        return true;
    case 384U:
        sha512_digest(data, length, sha384_initial, digest64);
        write_lower_hex(digest64, 48U, out_hex);
        *out_length = MYLITE_DIGEST_SHA2_384_HEX_LENGTH;
        return true;
    case 512U:
        sha512_digest(data, length, sha512_initial, digest64);
        write_lower_hex(digest64, 64U, out_hex);
        *out_length = MYLITE_DIGEST_SHA2_512_HEX_LENGTH;
        return true;
    default:
        return false;
    }
}

static uint32_t rotate_left32(uint32_t value, unsigned int count) {
    return (value << count) | (value >> (32U - count));
}

static void md5_process_block(uint32_t state[4], const unsigned char block[64]) {
    static const uint32_t k[64] = {
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
    static const unsigned int shift[64] = {
        7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U,
        5U, 9U,  14U, 20U, 5U, 9U,  14U, 20U, 5U, 9U,  14U, 20U, 5U, 9U,  14U, 20U,
        4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U,
        6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U,
    };
    uint32_t words[16] = {0};
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];

    for (size_t index = 0U; index < 16U; ++index) {
        words[index] = load_le32(block + index * 4U);
    }
    for (size_t index = 0U; index < 64U; ++index) {
        uint32_t f = 0U;
        size_t word_index = 0U;
        uint32_t next = d;

        if (index < 16U) {
            f = (b & c) | ((~b) & d);
            word_index = index;
        } else if (index < 32U) {
            f = (d & b) | ((~d) & c);
            word_index = (5U * index + 1U) % 16U;
        } else if (index < 48U) {
            f = b ^ c ^ d;
            word_index = (3U * index + 5U) % 16U;
        } else {
            f = c ^ (b | (~d));
            word_index = (7U * index) % 16U;
        }
        d = c;
        c = b;
        b += rotate_left32(a + f + k[index] + words[word_index], shift[index]);
        a = next;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

static void md5_write_digest(const uint32_t state[4], unsigned char digest[16]) {
    for (size_t index = 0U; index < 4U; ++index) {
        digest[index * 4U] = (unsigned char)(state[index] & 0xffU);
        digest[index * 4U + 1U] = (unsigned char)((state[index] >> 8U) & 0xffU);
        digest[index * 4U + 2U] = (unsigned char)((state[index] >> 16U) & 0xffU);
        digest[index * 4U + 3U] = (unsigned char)((state[index] >> 24U) & 0xffU);
    }
}

static uint32_t load_le32(const unsigned char *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) | ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static void store_le64(unsigned char *bytes, uint64_t value) {
    for (size_t index = 0U; index < 8U; ++index) {
        bytes[index] = (unsigned char)((value >> (index * 8U)) & UINT64_C(0xff));
    }
}

static void sha1_process_block(uint32_t state[5], const unsigned char block[64]) {
    uint32_t schedule[80] = {0};
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];

    for (size_t index = 0U; index < 16U; ++index) {
        schedule[index] = load_be32(block + index * 4U);
    }
    for (size_t index = 16U; index < 80U; ++index) {
        schedule[index] = rotate_left32(
            schedule[index - 3U] ^ schedule[index - 8U] ^ schedule[index - 14U] ^
                schedule[index - 16U],
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
        temp = rotate_left32(a, 5U) + f + e + k + schedule[index];
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

static void sha1_write_digest(const uint32_t state[5], unsigned char digest[20]) {
    for (size_t index = 0U; index < 5U; ++index) {
        store_be32(digest + index * 4U, state[index]);
    }
}

static uint32_t load_be32(const unsigned char *bytes) {
    return ((uint32_t)bytes[0] << 24U) | ((uint32_t)bytes[1] << 16U) | ((uint32_t)bytes[2] << 8U) |
           (uint32_t)bytes[3];
}

static void store_be64(unsigned char *bytes, uint64_t value) {
    for (size_t index = 0U; index < 8U; ++index) {
        unsigned int shift = (unsigned int)((7U - index) * 8U);

        bytes[index] = (unsigned char)((value >> shift) & UINT64_C(0xff));
    }
}

static void sha256_process_block(uint32_t state[8], const unsigned char block[64]) {
    static const uint32_t k[64] = {
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
    uint32_t schedule[64] = {0};
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];

    for (size_t index = 0U; index < 16U; ++index) {
        schedule[index] = load_be32(block + index * 4U);
    }
    for (size_t index = 16U; index < 64U; ++index) {
        uint32_t s0 = rotate_right32(schedule[index - 15U], 7U) ^
                      rotate_right32(schedule[index - 15U], 18U) ^ (schedule[index - 15U] >> 3U);
        uint32_t s1 = rotate_right32(schedule[index - 2U], 17U) ^
                      rotate_right32(schedule[index - 2U], 19U) ^ (schedule[index - 2U] >> 10U);

        schedule[index] = schedule[index - 16U] + s0 + schedule[index - 7U] + s1;
    }
    for (size_t index = 0U; index < 64U; ++index) {
        uint32_t s1 = rotate_right32(e, 6U) ^ rotate_right32(e, 11U) ^ rotate_right32(e, 25U);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + k[index] + schedule[index];
        uint32_t s0 = rotate_right32(a, 2U) ^ rotate_right32(a, 13U) ^ rotate_right32(a, 22U);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

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

static void sha256_digest(
    const unsigned char *data,
    size_t length,
    const uint32_t initial[8],
    unsigned char digest[32]
) {
    uint32_t state[8] = {0};
    unsigned char block[64] = {0};
    uint64_t bit_length = (uint64_t)length * CHAR_BIT;
    size_t offset = 0U;
    size_t remaining = 0U;

    memcpy(state, initial, sizeof(state));
    while (length - offset >= sizeof(block)) {
        sha256_process_block(state, data + offset);
        offset += sizeof(block);
    }

    remaining = length - offset;
    if (remaining != 0U) {
        memcpy(block, data + offset, remaining);
    }
    block[remaining] = 0x80U;
    if (remaining >= 56U) {
        sha256_process_block(state, block);
        memset(block, 0, sizeof(block));
    }
    store_be64(block + 56U, bit_length);
    sha256_process_block(state, block);
    for (size_t index = 0U; index < 8U; ++index) {
        store_be32(digest + index * 4U, state[index]);
    }
}

static uint32_t rotate_right32(uint32_t value, unsigned int count) {
    return (value >> count) | (value << (32U - count));
}

static void store_be32(unsigned char *bytes, uint32_t value) {
    bytes[0] = (unsigned char)((value >> 24U) & 0xffU);
    bytes[1] = (unsigned char)((value >> 16U) & 0xffU);
    bytes[2] = (unsigned char)((value >> 8U) & 0xffU);
    bytes[3] = (unsigned char)(value & 0xffU);
}

static void sha512_process_block(uint64_t state[8], const unsigned char block[128]) {
    static const uint64_t k[80] = {
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
    uint64_t schedule[80] = {0};
    uint64_t a = state[0];
    uint64_t b = state[1];
    uint64_t c = state[2];
    uint64_t d = state[3];
    uint64_t e = state[4];
    uint64_t f = state[5];
    uint64_t g = state[6];
    uint64_t h = state[7];

    for (size_t index = 0U; index < 16U; ++index) {
        schedule[index] = load_be64(block + index * 8U);
    }
    for (size_t index = 16U; index < 80U; ++index) {
        uint64_t s0 = rotate_right64(schedule[index - 15U], 1U) ^
                      rotate_right64(schedule[index - 15U], 8U) ^ (schedule[index - 15U] >> 7U);
        uint64_t s1 = rotate_right64(schedule[index - 2U], 19U) ^
                      rotate_right64(schedule[index - 2U], 61U) ^ (schedule[index - 2U] >> 6U);

        schedule[index] = schedule[index - 16U] + s0 + schedule[index - 7U] + s1;
    }
    for (size_t index = 0U; index < 80U; ++index) {
        uint64_t s1 = rotate_right64(e, 14U) ^ rotate_right64(e, 18U) ^ rotate_right64(e, 41U);
        uint64_t ch = (e & f) ^ ((~e) & g);
        uint64_t temp1 = h + s1 + ch + k[index] + schedule[index];
        uint64_t s0 = rotate_right64(a, 28U) ^ rotate_right64(a, 34U) ^ rotate_right64(a, 39U);
        uint64_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint64_t temp2 = s0 + maj;

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
    const unsigned char *data,
    size_t length,
    const uint64_t initial[8],
    unsigned char digest[64]
) {
    uint64_t state[8] = {0};
    unsigned char block[128] = {0};
    uint64_t bit_length = (uint64_t)length * CHAR_BIT;
    size_t offset = 0U;
    size_t remaining = 0U;

    memcpy(state, initial, sizeof(state));
    while (length - offset >= sizeof(block)) {
        sha512_process_block(state, data + offset);
        offset += sizeof(block);
    }

    remaining = length - offset;
    if (remaining != 0U) {
        memcpy(block, data + offset, remaining);
    }
    block[remaining] = 0x80U;
    if (remaining >= 112U) {
        sha512_process_block(state, block);
        memset(block, 0, sizeof(block));
    }
    store_be64(block + 112U, 0U);
    store_be64(block + 120U, bit_length);
    sha512_process_block(state, block);
    for (size_t index = 0U; index < 8U; ++index) {
        store_be64(digest + index * 8U, state[index]);
    }
}

static uint64_t load_be64(const unsigned char *bytes) {
    return ((uint64_t)bytes[0] << 56U) | ((uint64_t)bytes[1] << 48U) | ((uint64_t)bytes[2] << 40U) |
           ((uint64_t)bytes[3] << 32U) | ((uint64_t)bytes[4] << 24U) | ((uint64_t)bytes[5] << 16U) |
           ((uint64_t)bytes[6] << 8U) | (uint64_t)bytes[7];
}

static uint64_t rotate_right64(uint64_t value, unsigned int count) {
    return (value >> count) | (value << (64U - count));
}

static void write_lower_hex(const unsigned char *bytes, size_t length, char *out_hex) {
    static const char hex[] = "0123456789abcdef";

    for (size_t index = 0U; index < length; ++index) {
        out_hex[index * 2U] = hex[(bytes[index] >> 4U) & 0x0fU];
        out_hex[index * 2U + 1U] = hex[bytes[index] & 0x0fU];
    }
    out_hex[length * 2U] = '\0';
}
