#ifndef MYLITE_SQL_MYLITE_DIGEST_H
#define MYLITE_SQL_MYLITE_DIGEST_H

#include <stdbool.h>
#include <stddef.h>

enum {
    MYLITE_DIGEST_MD5_HEX_LENGTH = 32,
    MYLITE_DIGEST_SHA1_HEX_LENGTH = 40,
    MYLITE_DIGEST_SHA2_224_HEX_LENGTH = 56,
    MYLITE_DIGEST_SHA2_256_HEX_LENGTH = 64,
    MYLITE_DIGEST_SHA2_384_HEX_LENGTH = 96,
    MYLITE_DIGEST_SHA2_512_HEX_LENGTH = 128,
};

void mylite_digest_md5_hex(const unsigned char *data, size_t length, char *out_hex);
void mylite_digest_sha1_hex(const unsigned char *data, size_t length, char *out_hex);
bool mylite_digest_sha2_hex(
    const unsigned char *data,
    size_t length,
    unsigned int bits,
    char *out_hex,
    size_t *out_length
);

#endif
