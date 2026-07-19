#include "mylite_catalog_string_pool.h"

#include <mylite/mylite.h>

#include <stdbool.h>
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

struct mylite_catalog_string_pool_generation {
    struct mylite_catalog_string_pool_generation *next;
    struct mylite_catalog_string_pool_entry **slots;
    uint64_t generation_id;
    size_t count;
    size_t capacity;
    size_t byte_count;
    size_t reference_count;
};

static struct mylite_catalog_string_pool_generation *find_generation(
    const struct mylite_catalog_string_pool *pool,
    uint64_t generation_id
);
static int ensure_current_generation(struct mylite_catalog_string_pool *pool);
static void collect_retired_generations(struct mylite_catalog_string_pool *pool);
static void remove_generation(
    struct mylite_catalog_string_pool *pool,
    struct mylite_catalog_string_pool_generation *generation
);
static void generation_deinit(struct mylite_catalog_string_pool_generation *generation);
static uint64_t catalog_string_hash(const char *text, size_t length);
static struct mylite_catalog_string_pool_entry *find_catalog_string(
    const struct mylite_catalog_string_pool_generation *generation,
    const char *text,
    size_t length,
    uint64_t hash
);
static int reserve_catalog_string_pool(
    struct mylite_catalog_string_pool *pool,
    struct mylite_catalog_string_pool_generation *generation,
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
    struct mylite_catalog_string_pool_generation *generation = NULL;

    if (pool == NULL) {
        return;
    }

    generation = pool->generations;
    while (generation != NULL) {
        struct mylite_catalog_string_pool_generation *next = generation->next;

        generation_deinit(generation);
        free(generation);
        generation = next;
    }
    *pool = (struct mylite_catalog_string_pool){0};
}

void mylite_catalog_string_pool_set_generation(
    struct mylite_catalog_string_pool *pool,
    uint64_t generation_id
) {
    if (pool == NULL ||
        (pool->has_current_generation && pool->current_generation_id == generation_id)) {
        return;
    }

    pool->current_generation = find_generation(pool, generation_id);
    pool->current_generation_id = generation_id;
    pool->has_current_generation = true;
    collect_retired_generations(pool);
}

int mylite_catalog_string_pool_intern(
    struct mylite_catalog_string_pool *pool,
    const char *text,
    size_t length,
    const char **out_text
) {
    if (pool == NULL || text == NULL || out_text == NULL) {
        return MYLITE_MISUSE;
    }
    if (!pool->has_current_generation) {
        mylite_catalog_string_pool_set_generation(pool, 0U);
    }
    return mylite_catalog_string_pool_intern_for_generation(
        pool,
        pool->current_generation_id,
        text,
        length,
        out_text
    );
}

int mylite_catalog_string_pool_intern_for_generation(
    struct mylite_catalog_string_pool *pool,
    uint64_t generation_id,
    const char *text,
    size_t length,
    const char **out_text
) {
    struct mylite_catalog_string_pool_generation *generation = NULL;
    struct mylite_catalog_string_pool_entry *entry = NULL;
    uint64_t hash = 0U;
    size_t allocation_size = 0U;
    int rc = MYLITE_OK;

    if (pool == NULL || text == NULL || out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    mylite_catalog_string_pool_set_generation(pool, generation_id);
    rc = ensure_current_generation(pool);
    if (rc != MYLITE_OK) {
        return rc;
    }
    generation = pool->current_generation;
    hash = catalog_string_hash(text, length);
    entry = find_catalog_string(generation, text, length, hash);
    if (entry != NULL) {
        *out_text = entry->text;
        return MYLITE_OK;
    }

    if (generation->count == SIZE_MAX || pool->count == SIZE_MAX) {
        return MYLITE_NOMEM;
    }
    rc = reserve_catalog_string_pool(pool, generation, generation->count + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (length == SIZE_MAX || sizeof(*entry) > SIZE_MAX - length - 1U) {
        return MYLITE_NOMEM;
    }
    allocation_size = sizeof(*entry) + length + 1U;
    if (allocation_size > SIZE_MAX - generation->byte_count ||
        allocation_size > SIZE_MAX - pool->byte_count) {
        return MYLITE_NOMEM;
    }
    entry = (struct mylite_catalog_string_pool_entry *)malloc(allocation_size);
    if (entry == NULL) {
        return MYLITE_NOMEM;
    }
    entry->hash = hash;
    entry->length = length;
    if (length != 0U) {
        memcpy(entry->text, text, length);
    }
    entry->text[length] = '\0';
    insert_catalog_string_entry(generation->slots, generation->capacity, entry);
    ++generation->count;
    generation->byte_count += allocation_size;
    ++pool->count;
    pool->byte_count += allocation_size;
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

int mylite_catalog_string_pool_reference_acquire(
    struct mylite_catalog_string_pool *pool,
    uint64_t generation_id,
    struct mylite_catalog_string_pool_reference *out_reference
) {
    struct mylite_catalog_string_pool_generation *generation = NULL;

    if (pool == NULL || out_reference == NULL) {
        return MYLITE_MISUSE;
    }
    *out_reference = (struct mylite_catalog_string_pool_reference){0};
    generation = find_generation(pool, generation_id);
    if (generation == NULL || generation->reference_count == SIZE_MAX) {
        return generation == NULL ? MYLITE_MISUSE : MYLITE_NOMEM;
    }
    ++generation->reference_count;
    *out_reference = (struct mylite_catalog_string_pool_reference){
        .pool = pool,
        .generation = generation,
    };
    return MYLITE_OK;
}

void mylite_catalog_string_pool_reference_release(
    struct mylite_catalog_string_pool_reference *reference
) {
    struct mylite_catalog_string_pool *pool = NULL;
    struct mylite_catalog_string_pool_generation *generation = NULL;

    if (reference == NULL || reference->generation == NULL) {
        return;
    }
    pool = reference->pool;
    generation = reference->generation;
    *reference = (struct mylite_catalog_string_pool_reference){0};
    if (pool == NULL || generation->reference_count == 0U) {
        return;
    }
    --generation->reference_count;
    if (generation->reference_count == 0U && generation != pool->current_generation) {
        remove_generation(pool, generation);
    }
}

size_t mylite_catalog_string_pool_byte_count(const struct mylite_catalog_string_pool *pool) {
    return pool == NULL ? 0U : pool->byte_count;
}

size_t mylite_catalog_string_pool_generation_count(const struct mylite_catalog_string_pool *pool) {
    return pool == NULL ? 0U : pool->generation_count;
}

static struct mylite_catalog_string_pool_generation *find_generation(
    const struct mylite_catalog_string_pool *pool,
    uint64_t generation_id
) {
    struct mylite_catalog_string_pool_generation *generation = NULL;

    if (pool == NULL) {
        return NULL;
    }
    generation = pool->generations;
    while (generation != NULL) {
        if (generation->generation_id == generation_id) {
            return generation;
        }
        generation = generation->next;
    }
    return NULL;
}

static int ensure_current_generation(struct mylite_catalog_string_pool *pool) {
    struct mylite_catalog_string_pool_generation *generation = NULL;

    if (pool->current_generation != NULL) {
        return MYLITE_OK;
    }
    generation = (struct mylite_catalog_string_pool_generation *)calloc(1U, sizeof(*generation));
    if (generation == NULL) {
        return MYLITE_NOMEM;
    }
    if (sizeof(*generation) > SIZE_MAX - pool->byte_count) {
        free(generation);
        return MYLITE_NOMEM;
    }
    generation->generation_id = pool->current_generation_id;
    generation->byte_count = sizeof(*generation);
    generation->next = pool->generations;
    pool->generations = generation;
    pool->current_generation = generation;
    pool->byte_count += sizeof(*generation);
    ++pool->generation_count;
    return MYLITE_OK;
}

static void collect_retired_generations(struct mylite_catalog_string_pool *pool) {
    struct mylite_catalog_string_pool_generation *generation = pool->generations;

    while (generation != NULL) {
        struct mylite_catalog_string_pool_generation *next = generation->next;

        if (generation != pool->current_generation && generation->reference_count == 0U) {
            remove_generation(pool, generation);
        }
        generation = next;
    }
}

static void remove_generation(
    struct mylite_catalog_string_pool *pool,
    struct mylite_catalog_string_pool_generation *generation
) {
    struct mylite_catalog_string_pool_generation **link = &pool->generations;

    while (*link != NULL && *link != generation) {
        link = &(*link)->next;
    }
    if (*link == NULL) {
        return;
    }
    *link = generation->next;
    pool->count -= generation->count;
    pool->byte_count -= generation->byte_count;
    --pool->generation_count;
    generation_deinit(generation);
    free(generation);
}

static void generation_deinit(struct mylite_catalog_string_pool_generation *generation) {
    if (generation == NULL) {
        return;
    }
    for (size_t index = 0U; index < generation->capacity; ++index) {
        free(generation->slots[index]);
    }
    free((void *)generation->slots);
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
    const struct mylite_catalog_string_pool_generation *generation,
    const char *text,
    size_t length,
    uint64_t hash
) {
    size_t index = 0U;

    if (generation->capacity == 0U) {
        return NULL;
    }
    index = (size_t)(hash % generation->capacity);
    while (generation->slots[index] != NULL) {
        struct mylite_catalog_string_pool_entry *entry = generation->slots[index];

        if (entry->hash == hash && entry->length == length &&
            memcmp(entry->text, text, length) == 0) {
            return entry;
        }
        index = (index + 1U) % generation->capacity;
    }
    return NULL;
}

static int reserve_catalog_string_pool(
    struct mylite_catalog_string_pool *pool,
    struct mylite_catalog_string_pool_generation *generation,
    size_t required_count
) {
    struct mylite_catalog_string_pool_entry **slots = NULL;
    size_t capacity = generation->capacity;
    size_t old_bytes = generation->capacity * sizeof(*slots);
    size_t new_bytes = 0U;

    if (required_count <= catalog_string_pool_load_limit(capacity)) {
        return MYLITE_OK;
    }
    capacity = capacity == 0U ? catalog_string_pool_initial_capacity : capacity * 2U;
    if (capacity < generation->capacity || capacity > SIZE_MAX / sizeof(*slots)) {
        return MYLITE_NOMEM;
    }
    new_bytes = capacity * sizeof(*slots);
    if (new_bytes - old_bytes > SIZE_MAX - generation->byte_count ||
        new_bytes - old_bytes > SIZE_MAX - pool->byte_count) {
        return MYLITE_NOMEM;
    }
    slots = (struct mylite_catalog_string_pool_entry **)calloc(capacity, sizeof(*slots));
    if (slots == NULL) {
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < generation->capacity; ++index) {
        if (generation->slots[index] != NULL) {
            insert_catalog_string_entry(slots, capacity, generation->slots[index]);
        }
    }
    free((void *)generation->slots);
    generation->slots = slots;
    generation->capacity = capacity;
    generation->byte_count += new_bytes - old_bytes;
    pool->byte_count += new_bytes - old_bytes;
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
