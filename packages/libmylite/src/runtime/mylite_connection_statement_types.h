#ifndef MYLITE_RUNTIME_MYLITE_CONNECTION_STATEMENT_TYPES_H
#define MYLITE_RUNTIME_MYLITE_CONNECTION_STATEMENT_TYPES_H

#include <stdbool.h>

struct mylite_connection_charset_plan {
    char *character_set_name;
    char *collation_name;
    bool use_default;
};

#endif
