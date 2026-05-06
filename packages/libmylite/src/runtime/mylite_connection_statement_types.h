#ifndef MYLITE_RUNTIME_MYLITE_CONNECTION_STATEMENT_TYPES_H
#define MYLITE_RUNTIME_MYLITE_CONNECTION_STATEMENT_TYPES_H

#include <stdbool.h>
#include <stdint.h>

struct mylite_connection_charset_plan {
    char *character_set_name;
    char *collation_name;
    bool use_default;
};

enum mylite_connection_system_variable {
    MYLITE_CONNECTION_SYSTEM_VARIABLE_NONE = 0,
    MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_MODE = 1,
    MYLITE_CONNECTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN = 2,
};

struct mylite_connection_system_variable_plan {
    char *value;
    char *replace_search;
    char *replace_replacement;
    uint64_t unsigned_value;
    enum mylite_connection_system_variable variable;
    bool use_default;
    bool replace_current_value;
    bool emit_truncation_warning;
};

#endif
