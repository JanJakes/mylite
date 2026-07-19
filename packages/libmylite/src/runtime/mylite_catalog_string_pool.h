#ifndef MYLITE_RUNTIME_MYLITE_CATALOG_STRING_POOL_H
#define MYLITE_RUNTIME_MYLITE_CATALOG_STRING_POOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_catalog_string_pool_generation;

struct mylite_catalog_string_pool {
    struct mylite_catalog_string_pool_generation *generations;
    struct mylite_catalog_string_pool_generation *current_generation;
    uint64_t current_generation_id;
    size_t count;
    size_t byte_count;
    size_t generation_count;
    bool has_current_generation;
};

struct mylite_catalog_string_pool_reference {
    struct mylite_catalog_string_pool *pool;
    struct mylite_catalog_string_pool_generation *generation;
};

void mylite_catalog_string_pool_init(struct mylite_catalog_string_pool *pool);
void mylite_catalog_string_pool_deinit(struct mylite_catalog_string_pool *pool);
/* Advancing retires all unpinned storage from earlier catalog generations. */
void mylite_catalog_string_pool_set_generation(
    struct mylite_catalog_string_pool *pool,
    uint64_t generation_id
);
int mylite_catalog_string_pool_intern(
    struct mylite_catalog_string_pool *pool,
    const char *text,
    size_t length,
    const char **out_text
);
int mylite_catalog_string_pool_intern_for_generation(
    struct mylite_catalog_string_pool *pool,
    uint64_t generation_id,
    const char *text,
    size_t length,
    const char **out_text
);
int mylite_catalog_string_pool_intern_c_string(
    struct mylite_catalog_string_pool *pool,
    const char *text,
    const char **out_text
);
int mylite_catalog_string_pool_reference_acquire(
    struct mylite_catalog_string_pool *pool,
    uint64_t generation_id,
    struct mylite_catalog_string_pool_reference *out_reference
);
/* References keep descriptor pointers valid after their generation is retired. */
void mylite_catalog_string_pool_reference_release(
    struct mylite_catalog_string_pool_reference *reference
);
size_t mylite_catalog_string_pool_byte_count(const struct mylite_catalog_string_pool *pool);
size_t mylite_catalog_string_pool_generation_count(const struct mylite_catalog_string_pool *pool);

#endif
