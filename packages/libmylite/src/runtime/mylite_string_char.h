#ifndef MYLITE_RUNTIME_MYLITE_STRING_CHAR_H
#define MYLITE_RUNTIME_MYLITE_STRING_CHAR_H

#include "sqlite3.h"

#include <stddef.h>
#include <stdint.h>

struct mylite_string_char_buffer {
    unsigned char *bytes;
    size_t size;
    size_t capacity;
};

int mylite_string_char_buffer_append_int64(struct mylite_string_char_buffer *buffer, int64_t value);
int mylite_string_char_buffer_append_uint64(
    struct mylite_string_char_buffer *buffer,
    uint64_t value
);
int mylite_string_char_buffer_finish(
    struct mylite_string_char_buffer *buffer,
    char **out_bytes,
    size_t *out_size
);
void mylite_string_char_buffer_deinit(struct mylite_string_char_buffer *buffer);

int mylite_sqlite_register_string_char_function(sqlite3 *sqlite);

#endif
