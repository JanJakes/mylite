#ifndef MYLITE_RUNTIME_MYLITE_CONNECTION_STATEMENT_TYPES_H
#define MYLITE_RUNTIME_MYLITE_CONNECTION_STATEMENT_TYPES_H

#include <stdbool.h>

struct mylite_connection_charset_plan {
    char *character_set_name;
    char *collation_name;
    bool use_default;
};

struct mylite_connection_sql_mode_plan {
    char *value;
    char *replace_search;
    char *replace_replacement;
    bool use_default;
    bool replace_current_value;
};

#endif
