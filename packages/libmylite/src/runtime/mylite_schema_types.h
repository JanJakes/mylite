#ifndef MYLITE_RUNTIME_MYLITE_SCHEMA_TYPES_H
#define MYLITE_RUNTIME_MYLITE_SCHEMA_TYPES_H

#include <stdbool.h>

struct mylite_schema_options {
    char *character_set;
    char *collation;
    char *encryption;
    bool has_read_only;
    int read_only;
    bool invalid_encryption;
    bool invalid_read_only;
};

struct mylite_schema_presence {
    bool exists;
    bool is_system;
};

struct mylite_schema_default {
    const char *character_set;
    const char *collation;
};

#endif
