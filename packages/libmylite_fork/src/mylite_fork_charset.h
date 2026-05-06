#ifndef MYLITE_FORK_MYLITE_FORK_CHARSET_H
#define MYLITE_FORK_MYLITE_FORK_CHARSET_H

#include <stdbool.h>
#include <stddef.h>

struct mylite_fork_charset {
    const char *name;
    const char *description;
    const char *default_collation;
    int max_length;
};

struct mylite_fork_collation {
    const char *name;
    const char *character_set;
    int id;
    int sort_length;
    const char *pad_attribute;
    bool is_default;
};

const char *mylite_fork_charset_default_name(void);
const char *mylite_fork_charset_default_collation_name(void);
size_t mylite_fork_charset_count(void);
const struct mylite_fork_charset *mylite_fork_charset_at(size_t index);
const struct mylite_fork_charset *mylite_fork_charset_lookup(const char *name);
size_t mylite_fork_collation_count(void);
const struct mylite_fork_collation *mylite_fork_collation_at(size_t index);
const struct mylite_fork_collation *mylite_fork_collation_lookup(const char *name);
bool mylite_fork_charset_collation_match(
    const struct mylite_fork_charset *character_set,
    const struct mylite_fork_collation *collation
);

#endif
