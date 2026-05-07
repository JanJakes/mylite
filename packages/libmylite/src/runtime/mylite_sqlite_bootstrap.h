#ifndef MYLITE_RUNTIME_MYLITE_SQLITE_BOOTSTRAP_H
#define MYLITE_RUNTIME_MYLITE_SQLITE_BOOTSTRAP_H

#include <stdbool.h>

struct mylite_db;
struct sqlite3;
struct sqlite3_context;

struct mylite_sqlite_hook_registration_state {
    bool registration_surface_is_initialized;
    bool busy_handler_is_registered;
    bool progress_handler_is_registered;
    bool trace_callback_is_registered;
    bool update_hook_is_registered;
    bool commit_hook_is_registered;
    bool rollback_hook_is_registered;
};

struct mylite_sqlite_bootstrap_state {
    bool initialized;
    bool owner_client_data_is_registered;
    bool trusted_schema_policy_is_applied;
    bool trusted_schema_is_enabled;
    bool foreign_key_policy_is_applied;
    bool foreign_key_enforcement_is_enabled;
    bool foreign_key_policy_is_placeholder;
    bool function_registration_surface_is_initialized;
    bool collation_registration_surface_is_initialized;
    struct mylite_sqlite_hook_registration_state hooks;
};

int mylite_sqlite_bootstrap_connection(
    struct sqlite3 *sqlite,
    struct mylite_db *owner,
    struct mylite_sqlite_bootstrap_state *state
);
void mylite_sqlite_bootstrap_deinit(
    struct sqlite3 *sqlite,
    struct mylite_sqlite_bootstrap_state *state
);

struct mylite_db *mylite_sqlite_bootstrap_owner_from_connection(struct sqlite3 *sqlite);
struct mylite_db *mylite_sqlite_bootstrap_owner_from_context(struct sqlite3_context *context);

#endif
