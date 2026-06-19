#ifndef MYLITE_RUNTIME_MYLITE_NAMED_LOCKS_H
#define MYLITE_RUNTIME_MYLITE_NAMED_LOCKS_H

#include "sqlite3.h"

#include <stddef.h>
#include <stdint.h>

struct mylite_db;

struct mylite_named_lock_name {
    const char *data;
    size_t size;
};

int mylite_named_lock_get(
    struct mylite_db *database,
    struct mylite_named_lock_name name,
    int64_t timeout,
    int64_t *out_value
);
int mylite_named_lock_is_free(
    struct mylite_db *database,
    struct mylite_named_lock_name name,
    int64_t *out_value
);
int mylite_named_lock_is_used(
    struct mylite_db *database,
    struct mylite_named_lock_name name,
    uint64_t *out_connection_id,
    int *out_is_null
);
int mylite_named_lock_release(
    struct mylite_db *database,
    struct mylite_named_lock_name name,
    int64_t *out_value,
    int *out_is_null
);
int mylite_named_lock_release_all(struct mylite_db *database, uint64_t *out_count);
void mylite_named_lock_release_all_for_connection(uint64_t connection_id);

int mylite_sqlite_register_named_lock_functions(sqlite3 *sqlite);

#endif
