#ifndef MYLITE_RUNTIME_MYLITE_TRANSACTION_TYPES_H
#define MYLITE_RUNTIME_MYLITE_TRANSACTION_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum mylite_transaction_access_mode {
    MYLITE_TRANSACTION_ACCESS_READ_WRITE = 0,
    MYLITE_TRANSACTION_ACCESS_READ_ONLY = 1,
};

enum mylite_transaction_completion_chain {
    MYLITE_TRANSACTION_COMPLETION_CHAIN_DEFAULT = 0,
    MYLITE_TRANSACTION_COMPLETION_CHAIN_YES = 1,
    MYLITE_TRANSACTION_COMPLETION_CHAIN_NO = 2,
};

enum mylite_transaction_completion_release {
    MYLITE_TRANSACTION_COMPLETION_RELEASE_DEFAULT = 0,
    MYLITE_TRANSACTION_COMPLETION_RELEASE_YES = 1,
    MYLITE_TRANSACTION_COMPLETION_RELEASE_NO = 2,
};

enum mylite_statement_atomicity_kind {
    MYLITE_STATEMENT_ATOMICITY_NONE = 0,
    MYLITE_STATEMENT_ATOMICITY_TRANSACTION = 1,
    MYLITE_STATEMENT_ATOMICITY_SAVEPOINT = 2,
};

struct mylite_transaction_plan {
    enum mylite_transaction_access_mode access_mode;
    enum mylite_transaction_completion_chain completion_chain;
    enum mylite_transaction_completion_release completion_release;
    bool has_access_mode;
    bool consistent_snapshot;
};

struct mylite_savepoint_plan {
    char *name;
    char *normalized_name;
};

struct mylite_statement_atomicity {
    enum mylite_statement_atomicity_kind kind;
};

struct mylite_savepoint {
    char *original_name;
    char *normalized_name;
    char *sqlite_name;
    unsigned int level;
};

struct mylite_savepoint_state {
    struct mylite_savepoint *items;
    size_t count;
    size_t capacity;
    uint64_t next_sqlite_id;
    unsigned int current_level;
};

struct mylite_pending_auto_increment {
    char *schema_name;
    char *table_name;
    uint64_t next_auto_increment;
};

#endif
