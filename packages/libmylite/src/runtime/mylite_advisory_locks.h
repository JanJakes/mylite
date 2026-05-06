#ifndef MYLITE_RUNTIME_MYLITE_ADVISORY_LOCKS_H
#define MYLITE_RUNTIME_MYLITE_ADVISORY_LOCKS_H

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_expression_value;
struct mylite_expression_warnings;

struct mylite_advisory_lock_name {
    char *text;
    size_t length;
};

struct mylite_advisory_lock_result {
    bool is_null;
    uint64_t value;
};

int mylite_advisory_lock_name_from_value(mylite_db *database,
                                         const struct mylite_expression_value *value,
                                         struct mylite_expression_warnings *warnings,
                                         struct mylite_advisory_lock_name *out_name);
void mylite_advisory_lock_name_deinit(struct mylite_advisory_lock_name *name);

int mylite_advisory_lock_get(mylite_db *database, const struct mylite_advisory_lock_name *name,
                             uint64_t *out_value);
int mylite_advisory_lock_release(mylite_db *database, const struct mylite_advisory_lock_name *name,
                                 struct mylite_advisory_lock_result *out_result);
int mylite_advisory_lock_is_free(mylite_db *database, const struct mylite_advisory_lock_name *name,
                                 uint64_t *out_value);
int mylite_advisory_lock_is_used(mylite_db *database, const struct mylite_advisory_lock_name *name,
                                 struct mylite_advisory_lock_result *out_result);
uint64_t mylite_advisory_locks_release_all(mylite_db *database);
void mylite_advisory_locks_release_handle(mylite_db *database);

#endif
