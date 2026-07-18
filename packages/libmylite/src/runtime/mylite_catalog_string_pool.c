#include "mylite_catalog_string_pool.h"

#include <mylite/mylite.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    catalog_string_pool_initial_capacity = 64,
    catalog_string_pool_load_numerator = 7,
    catalog_string_pool_load_denominator = 10,
};

struct mylite_catalog_string_pool_entry {
    uint64_t hash;
    size_t length;
    char text[];
};

static uint64_t catalog_string_hash(const char *text, size_t length);
static struct mylite_catalog_string_pool_entry *find_catalog_string(
    const struct mylite_catalog_string_pool *pool,
    const char *text,
    size_t length,
    uint64_t hash
);
static int reserve_catalog_string_pool(
    struct mylite_catalog_string_pool *pool,
    size_t required_count
);
static size_t catalog_string_pool_load_limit(size_t capacity);
static void insert_catalog_string_entry(
    struct mylite_catalog_string_pool_entry **slots,
    size_t capacity,
    struct mylite_catalog_string_pool_entry *entry
);

void mylite_catalog_string_pool_init(struct mylite_catalog_string_pool *pool) {
    if (pool == NULL) {
        return;
    }
    *pool = (struct mylite_catalog_string_pool){0};
}

void mylite_catalog_string_pool_deinit(struct mylite_catalog_string_pool *pool) {
    if (pool == NULL) {
        return;
    }

    for (size_t index = 0U; index < pool->capacity; ++index) {
        free(pool->slots[index]);
    }
    free((void *)pool->slots);
    *pool = (struct mylite_catalog_string_pool){0};
}

int mylite_catalog_string_pool_intern(
    struct mylite_catalog_string_pool *pool,
    const char *text,
    size_t length,
    const char **out_text
) {
    struct mylite_catalog_string_pool_entry *entry = NULL;
    uint64_t hash = 0U;
    int rc = MYLITE_OK;

    if (pool == NULL || text == NULL || out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    hash = catalog_string_hash(text, length);
    entry = find_catalog_string(pool, text, length, hash);
    if (entry != NULL) {
        *out_text = entry->text;
        return MYLITE_OK;
    }

    if (pool->count == SIZE_MAX) {
        return MYLITE_NOMEM;
    }
    rc = reserve_catalog_string_pool(pool, pool->count + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (length == SIZE_MAX || sizeof(*entry) > SIZE_MAX - length - 1U) {
        return MYLITE_NOMEM;
    }
    entry = (struct mylite_catalog_string_pool_entry *)malloc(sizeof(*entry) + length + 1U);
    if (entry == NULL) {
        return MYLITE_NOMEM;
    }
    entry->hash = hash;
    entry->length = length;
    if (length != 0U) {
        memcpy(entry->text, text, length);
    }
    entry->text[length] = '\0';
    insert_catalog_string_entry(pool->slots, pool->capacity, entry);
    ++pool->count;
    *out_text = entry->text;
    return MYLITE_OK;
}

int mylite_catalog_string_pool_intern_c_string(
    struct mylite_catalog_string_pool *pool,
    const char *text,
    const char **out_text
) {
    if (text == NULL) {
        text = "";
    }
    return mylite_catalog_string_pool_intern(pool, text, strlen(text), out_text);
}

static uint64_t catalog_string_hash(const char *text, size_t length) {
    uint64_t hash = UINT64_C(14695981039346656037);

    for (size_t index = 0U; index < length; ++index) {
        hash ^= (unsigned char)text[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static struct mylite_catalog_string_pool_entry *find_catalog_string(
    const struct mylite_catalog_string_pool *pool,
    const char *text,
    size_t length,
    uint64_t hash
) {
    size_t index = 0U;

    if (pool->capacity == 0U) {
        return NULL;
    }
    index = (size_t)(hash % pool->capacity);
    while (pool->slots[index] != NULL) {
        struct mylite_catalog_string_pool_entry *entry = pool->slots[index];

        if (entry->hash == hash && entry->length == length &&
            memcmp(entry->text, text, length) == 0) {
            return entry;
        }
        index = (index + 1U) % pool->capacity;
    }
    return NULL;
}

static int reserve_catalog_string_pool(
    struct mylite_catalog_string_pool *pool,
    size_t required_count
) {
    struct mylite_catalog_string_pool_entry **slots = NULL;
    size_t capacity = pool->capacity;

    if (required_count <= catalog_string_pool_load_limit(capacity)) {
        return MYLITE_OK;
    }
    capacity = capacity == 0U ? catalog_string_pool_initial_capacity : capacity * 2U;
    if (capacity < pool->capacity || capacity > SIZE_MAX / sizeof(*slots)) {
        return MYLITE_NOMEM;
    }
    slots = (struct mylite_catalog_string_pool_entry **)calloc(capacity, sizeof(*slots));
    if (slots == NULL) {
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < pool->capacity; ++index) {
        if (pool->slots[index] != NULL) {
            insert_catalog_string_entry(slots, capacity, pool->slots[index]);
        }
    }
    free((void *)pool->slots);
    pool->slots = slots;
    pool->capacity = capacity;
    return MYLITE_OK;
}

static size_t catalog_string_pool_load_limit(size_t capacity) {
    return ((capacity / catalog_string_pool_load_denominator) * catalog_string_pool_load_numerator
           ) +
           (((capacity % catalog_string_pool_load_denominator) * catalog_string_pool_load_numerator
            ) /
            catalog_string_pool_load_denominator);
}

static void insert_catalog_string_entry(
    struct mylite_catalog_string_pool_entry **slots,
    size_t capacity,
    struct mylite_catalog_string_pool_entry *entry
) {
    size_t index = (size_t)(entry->hash % capacity);

    while (slots[index] != NULL) {
        index = (index + 1U) % capacity;
    }
    slots[index] = entry;
}
