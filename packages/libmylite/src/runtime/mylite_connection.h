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
    MYLITE_SESSION_SQL_MODE_TEXT_CAPACITY = 512,
    MYLITE_SESSION_TIME_ZONE_CAPACITY = 64,
    MYLITE_SESSION_CHARSET_NAME_CAPACITY = 64,
};

#define MYLITE_SESSION_SQL_MODE_DEFAULT_TEXT                                                       \
    "ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,"                         \
    "ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION"

enum mylite_session_sql_mode {
    MYLITE_SESSION_SQL_MODE_REAL_AS_FLOAT = 1U << 0U,
    MYLITE_SESSION_SQL_MODE_PIPES_AS_CONCAT = 1U << 1U,
    MYLITE_SESSION_SQL_MODE_ANSI_QUOTES = 1U << 2U,
    MYLITE_SESSION_SQL_MODE_IGNORE_SPACE = 1U << 3U,
    MYLITE_SESSION_SQL_MODE_ONLY_FULL_GROUP_BY = 1U << 4U,
    MYLITE_SESSION_SQL_MODE_ANSI = 1U << 5U,
    MYLITE_SESSION_SQL_MODE_NO_UNSIGNED_SUBTRACTION = 1U << 6U,
    MYLITE_SESSION_SQL_MODE_NO_DIR_IN_CREATE = 1U << 7U,
    MYLITE_SESSION_SQL_MODE_NO_AUTO_VALUE_ON_ZERO = 1U << 8U,
    MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES = 1U << 9U,
    MYLITE_SESSION_SQL_MODE_STRICT_TRANS_TABLES = 1U << 10U,
    MYLITE_SESSION_SQL_MODE_STRICT_ALL_TABLES = 1U << 11U,
    MYLITE_SESSION_SQL_MODE_NO_ZERO_IN_DATE = 1U << 12U,
    MYLITE_SESSION_SQL_MODE_NO_ZERO_DATE = 1U << 13U,
    MYLITE_SESSION_SQL_MODE_ALLOW_INVALID_DATES = 1U << 14U,
    MYLITE_SESSION_SQL_MODE_ERROR_FOR_DIVISION_BY_ZERO = 1U << 15U,
    MYLITE_SESSION_SQL_MODE_TRADITIONAL = 1U << 16U,
    MYLITE_SESSION_SQL_MODE_HIGH_NOT_PRECEDENCE = 1U << 17U,
    MYLITE_SESSION_SQL_MODE_NO_ENGINE_SUBSTITUTION = 1U << 18U,
    MYLITE_SESSION_SQL_MODE_PAD_CHAR_TO_FULL_LENGTH = 1U << 19U,
    MYLITE_SESSION_SQL_MODE_TIME_TRUNCATE_FRACTIONAL = 1U << 20U,
};

enum {
    MYLITE_SESSION_SQL_MODE_DEFAULT_BITS =
        MYLITE_SESSION_SQL_MODE_ONLY_FULL_GROUP_BY | MYLITE_SESSION_SQL_MODE_STRICT_TRANS_TABLES |
        MYLITE_SESSION_SQL_MODE_NO_ZERO_IN_DATE | MYLITE_SESSION_SQL_MODE_NO_ZERO_DATE |
        MYLITE_SESSION_SQL_MODE_ERROR_FOR_DIVISION_BY_ZERO |
        MYLITE_SESSION_SQL_MODE_NO_ENGINE_SUBSTITUTION,
};

struct sqlite3;

struct mylite_session_state {
    bool has_selected_schema;
    char selected_schema[MYLITE_SESSION_SCHEMA_CAPACITY];
    char current_user_identity[MYLITE_SESSION_IDENTIFIER_CAPACITY];
    char client_user_identity[MYLITE_SESSION_IDENTIFIER_CAPACITY];
    uint64_t sql_mode;
    char sql_mode_text[MYLITE_SESSION_SQL_MODE_TEXT_CAPACITY];
    bool sql_mode_is_placeholder;
    char time_zone[MYLITE_SESSION_TIME_ZONE_CAPACITY];
    bool time_zone_is_placeholder;
    char character_set_client[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    char character_set_connection[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    char character_set_results[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    char collation_connection[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    bool character_set_state_is_placeholder;
    bool system_variables_are_placeholder;
    uint64_t connection_id;
    int64_t previous_row_count;
    uint64_t found_rows;
    uint64_t last_insert_id;
    uint64_t catalog_generation;
    uint64_t sqlite_schema_generation;
    bool has_timestamp_override;
    int64_t timestamp_override;
    int64_t active_statement_time;
};

struct mylite_db {
    struct sqlite3 *sqlite;
    struct mylite_diagnostics diagnostics;
    struct mylite_diagnostics previous_diagnostics;
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
