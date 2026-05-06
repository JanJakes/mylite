#ifndef MYLITE_CHARSET_MYLITE_CHARSET_H
#define MYLITE_CHARSET_MYLITE_CHARSET_H

#include <stdbool.h>
#include <stddef.h>

struct mylite_charset {
    const char *name;
    const char *description;
    const char *default_collation;
    int max_length;
};

struct mylite_collation {
    const char *name;
    const char *character_set;
    int id;
    int sort_length;
    const char *pad_attribute;
    bool is_default;
};

const char *mylite_charset_default_name(void);
const char *mylite_charset_default_collation_name(void);
size_t mylite_charset_count(void);
const struct mylite_charset *mylite_charset_at(size_t index);
const struct mylite_charset *mylite_charset_lookup(const char *name);
size_t mylite_collation_count(void);
const struct mylite_collation *mylite_collation_at(size_t index);
const struct mylite_collation *mylite_collation_lookup(const char *name);
bool mylite_charset_collation_match(
    const struct mylite_charset *character_set,
    const struct mylite_collation *collation
);

#endif
