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
    MYLITE_CONNECTION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE = 3,
    MYLITE_CONNECTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS = 4,
    MYLITE_CONNECTION_SYSTEM_VARIABLE_TIME_ZONE = 5,
    MYLITE_CONNECTION_SYSTEM_VARIABLE_UNIQUE_CHECKS = 6,
    MYLITE_CONNECTION_SYSTEM_VARIABLE_WAIT_TIMEOUT = 7,
    MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_NOTES = 8,
    MYLITE_CONNECTION_SYSTEM_VARIABLE_SQL_LOG_BIN = 9,
    MYLITE_CONNECTION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT = 10,
    MYLITE_CONNECTION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS = 11,
    MYLITE_CONNECTION_SYSTEM_VARIABLE_COLLATION_CONNECTION = 12,
    MYLITE_CONNECTION_SYSTEM_VARIABLE_DEFAULT_COLLATION_FOR_UTF8MB4 = 13,
    MYLITE_CONNECTION_SYSTEM_VARIABLE_SKIP_EXTERNAL_LOCKING = 14,
    MYLITE_CONNECTION_SYSTEM_VARIABLE_RAND_SEED1 = 15,
    MYLITE_CONNECTION_SYSTEM_VARIABLE_RAND_SEED2 = 16,
};

struct mylite_connection_system_variable_plan {
    char *value;
    char *user_variable_name;
    char *replace_search;
    char *replace_replacement;
    uint64_t unsigned_value;
    enum mylite_connection_system_variable variable;
    bool use_default;
    bool use_user_variable_value;
    bool replace_current_value;
    bool emit_truncation_warning;
    bool use_null_value;
    bool global_scope;
};

#endif
