#ifndef MYLITE_COLLATION_H
#define MYLITE_COLLATION_H

#include <stddef.h>

enum mylite_collation_kind {
    MYLITE_COLLATION_UTF8MB4_0900_AI_CI = 0,
    MYLITE_COLLATION_UTF8MB4_0900_AS_CI = 1,
    MYLITE_COLLATION_UTF8MB4_0900_AS_CS = 2,
    MYLITE_COLLATION_BINARY = 3,
};

int mylite_collation_kind_from_name(const char *name, enum mylite_collation_kind *out_kind);
const char *mylite_collation_sqlite_name(enum mylite_collation_kind kind);
int mylite_collation_compare(
    enum mylite_collation_kind kind,
    const void *left,
    size_t left_size,
    const void *right,
    size_t right_size
);
int mylite_collation_make_key(
    enum mylite_collation_kind kind,
    const void *text,
    size_t text_size,
    unsigned char **out_key,
    size_t *out_key_size
);

#endif
