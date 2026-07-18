#ifndef MYLITE_RUNTIME_MYLITE_CATALOG_STRING_POOL_H
#define MYLITE_RUNTIME_MYLITE_CATALOG_STRING_POOL_H

#include <stddef.h>

struct mylite_catalog_string_pool_entry;

struct mylite_catalog_string_pool {
    struct mylite_catalog_string_pool_entry **slots;
    size_t count;
    size_t capacity;
};

void mylite_catalog_string_pool_init(struct mylite_catalog_string_pool *pool);
void mylite_catalog_string_pool_deinit(struct mylite_catalog_string_pool *pool);
int mylite_catalog_string_pool_intern(
    struct mylite_catalog_string_pool *pool,
    const char *text,
    size_t length,
    const char **out_text
);
int mylite_catalog_string_pool_intern_c_string(
    struct mylite_catalog_string_pool *pool,
    const char *text,
    const char **out_text
);

#endif
