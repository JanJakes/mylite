#ifndef MYLITE_RUNTIME_MYLITE_CONNECTION_H
#define MYLITE_RUNTIME_MYLITE_CONNECTION_H

#include <mylite/mylite.h>

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_temporary_catalog.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    MYLITE_SESSION_IDENTIFIER_CAPACITY = 64,
    MYLITE_SESSION_SCHEMA_CAPACITY = 64,
    MYLITE_SESSION_SAVEPOINT_NAME_CAPACITY = 64,
    MYLITE_SESSION_SAVEPOINT_INTERNAL_NAME_CAPACITY = 128,
    MYLITE_SESSION_SQL_MODE_TEXT_CAPACITY = 512,
    MYLITE_SESSION_TIME_ZONE_CAPACITY = 64,
    MYLITE_SESSION_CHARSET_NAME_CAPACITY = 64,
    MYLITE_SESSION_USER_VARIABLE_NAME_MAX_CHARACTERS = 64,
    MYLITE_SESSION_USER_VARIABLE_UTF8MB4_MAX_BYTES_PER_CHARACTER = 4,
    MYLITE_SESSION_USER_VARIABLE_NAME_MAX_BYTES =
        MYLITE_SESSION_USER_VARIABLE_NAME_MAX_CHARACTERS *
        MYLITE_SESSION_USER_VARIABLE_UTF8MB4_MAX_BYTES_PER_CHARACTER,
    MYLITE_SESSION_USER_VARIABLE_NAME_CAPACITY = MYLITE_SESSION_USER_VARIABLE_NAME_MAX_BYTES + 1,
    MYLITE_SESSION_TIMEOUT_DEFAULT_VALUE = 28800,
    MYLITE_SESSION_GROUP_CONCAT_MAX_LEN_DEFAULT_VALUE = 1024,
    MYLITE_SESSION_UUID_NODE_SIZE = 6,
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

struct mylite_session_savepoint {
    char name[MYLITE_SESSION_SAVEPOINT_NAME_CAPACITY];
    char folded_name[MYLITE_SESSION_SAVEPOINT_NAME_CAPACITY];
    char sqlite_name[MYLITE_SESSION_SAVEPOINT_INTERNAL_NAME_CAPACITY];
};

enum mylite_session_table_lock_mode {
    MYLITE_SESSION_TABLE_LOCK_READ = 1,
    MYLITE_SESSION_TABLE_LOCK_READ_LOCAL = 2,
    MYLITE_SESSION_TABLE_LOCK_WRITE = 3,
};

struct mylite_session_table_lock {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char alias[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char effective_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_alias;
    bool is_temporary;
    enum mylite_session_table_lock_mode mode;
};

enum mylite_session_user_variable_value_kind {
    MYLITE_SESSION_USER_VARIABLE_VALUE_NULL = 0,
    MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER = 1,
    MYLITE_SESSION_USER_VARIABLE_VALUE_STRING = 2,
};

struct mylite_session_user_variable {
    char name[MYLITE_SESSION_USER_VARIABLE_NAME_CAPACITY];
    char *value;
    size_t value_size;
    enum mylite_session_user_variable_value_kind value_kind;
    bool is_null;
};

struct mylite_session_prepared_statement {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *sql;
    size_t sql_size;
    size_t parameter_count;
};

enum mylite_transaction_isolation {
    MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ = 0,
    MYLITE_TRANSACTION_ISOLATION_READ_COMMITTED = 1,
    MYLITE_TRANSACTION_ISOLATION_READ_UNCOMMITTED = 2,
    MYLITE_TRANSACTION_ISOLATION_SERIALIZABLE = 3,
};

enum mylite_transaction_access_mode {
    MYLITE_TRANSACTION_ACCESS_READ_WRITE = 0,
    MYLITE_TRANSACTION_ACCESS_READ_ONLY = 1,
};

struct mylite_session_state {
    uint64_t sql_mode;
    uint64_t connection_id;
    int64_t previous_row_count;
    uint64_t found_rows;
    uint64_t last_insert_id;
    uint64_t auto_increment_increment;
    uint64_t auto_increment_offset;
    uint64_t sql_select_limit;
    uint64_t group_concat_max_len;
    uint64_t group_concat_value_ordinal;
    uint64_t wait_timeout;
    uint64_t interactive_timeout;
    uint64_t catalog_generation;
    uint64_t sqlite_schema_generation;
    uint64_t uuid_last_timestamp_100ns;
    struct mylite_session_savepoint *savepoints;
    size_t savepoint_count;
    size_t savepoint_capacity;
    uint64_t next_savepoint_id;
    struct mylite_session_table_lock *table_locks;
    size_t table_lock_count;
    size_t table_lock_capacity;
    struct mylite_session_user_variable *user_variables;
    size_t user_variable_count;
    size_t user_variable_capacity;
    struct mylite_session_prepared_statement *prepared_statements;
    size_t prepared_statement_count;
    size_t prepared_statement_capacity;
    int64_t timestamp_override;
    int64_t active_statement_time;
    struct mylite_temporary_catalog temporary_catalog;
    int time_zone_offset_minutes;
    unsigned char uuid_node[MYLITE_SESSION_UUID_NODE_SIZE];
    uint16_t uuid_clock_sequence;
    enum mylite_transaction_isolation session_transaction_isolation;
    enum mylite_transaction_access_mode session_transaction_access_mode;
    enum mylite_transaction_isolation next_transaction_isolation;
    enum mylite_transaction_access_mode next_transaction_access_mode;
    bool has_selected_schema;
    bool sql_mode_is_placeholder;
    bool time_zone_is_placeholder;
    bool character_set_state_is_placeholder;
    bool system_variables_are_placeholder;
    bool foreign_key_checks_enabled;
    bool user_transaction_active;
    bool has_next_transaction_isolation;
    bool has_next_transaction_access_mode;
    bool next_transaction_isolation_from_system_variable;
    bool next_transaction_access_mode_from_system_variable;
    bool active_transaction_read_only;
    bool has_timestamp_override;
    bool uuid_state_initialized;
    char selected_schema[MYLITE_SESSION_SCHEMA_CAPACITY];
    char current_user_identity[MYLITE_SESSION_IDENTIFIER_CAPACITY];
    char client_user_identity[MYLITE_SESSION_IDENTIFIER_CAPACITY];
    char time_zone[MYLITE_SESSION_TIME_ZONE_CAPACITY];
    char character_set_client[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    char character_set_connection[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    char character_set_results[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    char collation_connection[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    char sql_mode_text[MYLITE_SESSION_SQL_MODE_TEXT_CAPACITY];
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
