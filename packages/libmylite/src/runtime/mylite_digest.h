#ifndef MYLITE_RUNTIME_MYLITE_DIGEST_H
#define MYLITE_RUNTIME_MYLITE_DIGEST_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stddef.h>

enum mylite_digest_algorithm {
    MYLITE_DIGEST_ALGORITHM_MD5 = 0,
    MYLITE_DIGEST_ALGORITHM_SHA1 = 1,
    MYLITE_DIGEST_ALGORITHM_SHA224 = 2,
    MYLITE_DIGEST_ALGORITHM_SHA256 = 3,
    MYLITE_DIGEST_ALGORITHM_SHA384 = 4,
    MYLITE_DIGEST_ALGORITHM_SHA512 = 5,
};

enum mylite_digest_sha2_length {
    MYLITE_DIGEST_SHA2_LENGTH_DEFAULT = 0,
    MYLITE_DIGEST_SHA2_LENGTH_SHA224 = 224,
    MYLITE_DIGEST_SHA2_LENGTH_SHA256 = 256,
    MYLITE_DIGEST_SHA2_LENGTH_SHA384 = 384,
    MYLITE_DIGEST_SHA2_LENGTH_SHA512 = 512,
};

size_t mylite_digest_hex_length(enum mylite_digest_algorithm algorithm);
int mylite_digest_hex_value(
    enum mylite_digest_algorithm algorithm,
    const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    size_t *out_text_length
);
int mylite_sqlite_register_digest_functions(sqlite3 *sqlite);

#endif
