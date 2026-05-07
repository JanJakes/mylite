#ifndef MYLITE_RUNTIME_MYLITE_CONNECTION_H
#define MYLITE_RUNTIME_MYLITE_CONNECTION_H

#include <mylite/mylite.h>

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_sqlite_bootstrap.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    MYLITE_SESSION_IDENTIFIER_CAPACITY = 64,
    MYLITE_SESSION_SCHEMA_CAPACITY = 64,
    MYLITE_SESSION_TIME_ZONE_CAPACITY = 64,
    MYLITE_SESSION_CHARSET_NAME_CAPACITY = 64,
};

struct sqlite3;

struct mylite_session_state {
    bool has_selected_schema;
    char selected_schema[MYLITE_SESSION_SCHEMA_CAPACITY];
    char current_user_identity[MYLITE_SESSION_IDENTIFIER_CAPACITY];
    char client_user_identity[MYLITE_SESSION_IDENTIFIER_CAPACITY];
    uint64_t sql_mode;
    bool sql_mode_is_placeholder;
    char time_zone[MYLITE_SESSION_TIME_ZONE_CAPACITY];
    bool time_zone_is_placeholder;
    char character_set_client[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    char character_set_connection[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    char character_set_results[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    char collation_connection[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    bool character_set_state_is_placeholder;
    bool system_variables_are_placeholder;
    int64_t previous_row_count;
    uint64_t catalog_generation;
    uint64_t sqlite_schema_generation;
};

struct mylite_db {
    struct sqlite3 *sqlite;
    struct mylite_diagnostics diagnostics;
    struct mylite_session_state session;
    struct mylite_sqlite_bootstrap_state sqlite_bootstrap;
    struct mylite_catalog catalog;
};

struct mylite_diagnostics *mylite_connection_diagnostics(struct mylite_db *database);
const struct mylite_session_state *mylite_connection_session_state(
    const struct mylite_db *database
);

struct sqlite3 *mylite_connection_sqlite_for_test(struct mylite_db *database);
const struct mylite_sqlite_bootstrap_state *mylite_connection_sqlite_bootstrap_state_for_test(
    const struct mylite_db *database
);
const struct mylite_catalog *mylite_connection_catalog_for_test(const struct mylite_db *database);

#endif
