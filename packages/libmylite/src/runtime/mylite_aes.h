#ifndef MYLITE_RUNTIME_MYLITE_AES_H
#define MYLITE_RUNTIME_MYLITE_AES_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>

int mylite_aes_encrypt_default(
    const unsigned char *input,
    size_t input_size,
    const unsigned char *key,
    size_t key_size,
    unsigned char **out_bytes,
    size_t *out_size,
    bool *out_long_key_warning
);
int mylite_aes_decrypt_default(
    const unsigned char *input,
    size_t input_size,
    const unsigned char *key,
    size_t key_size,
    unsigned char **out_bytes,
    size_t *out_size,
    bool *out_invalid,
    bool *out_long_key_warning
);
int mylite_sqlite_register_aes_functions(sqlite3 *sqlite);

#endif
