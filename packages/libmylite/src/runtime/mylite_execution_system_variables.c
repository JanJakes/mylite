#include "mylite_execution_system_variables.h"

#include "mylite_connection.h"

#include <mylite/mylite.h>

#include <string.h>

struct mylite_execution_sql_mode_descriptor {
    const char *name;
    uint64_t bit;
    uint64_t expansion;
};

struct execution_sql_mode_token {
    size_t start;
    size_t length;
};

static char execution_system_variable_ascii_lower(char byte);
static int execution_sql_mode_parse_token(
    const char *text,
    struct execution_sql_mode_token token,
    uint64_t *modes,
    const char **out_invalid_token,
    size_t *out_invalid_token_length
);
static bool execution_sql_mode_token_expansion(
    const char *text,
    size_t length,
    uint64_t *out_expansion
);
static void execution_sql_mode_set_invalid_token(
    const char *text,
    struct execution_sql_mode_token token,
    const char **out_invalid_token,
    size_t *out_invalid_token_length
);

static const struct mylite_execution_system_variable_descriptor system_variable_descriptors[] = {
    {"auto_increment_increment",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT,
     true,
     true},
    {"auto_increment_offset", MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET, true, true},
    {"autocommit", MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOCOMMIT, true, true},
    {"basedir", MYLITE_EXECUTION_SYSTEM_VARIABLE_BASEDIR, true, true},
    {"big_tables", MYLITE_EXECUTION_SYSTEM_VARIABLE_BIG_TABLES, true, true},
    {"character_set_client", MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT, true, true},
    {"character_set_connection",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_CONNECTION,
     true,
     true},
    {"character_set_database", MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_DATABASE, true, true},
    {"character_set_filesystem",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_FILESYSTEM,
     true,
     true},
    {"character_set_results", MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS, true, true},
    {"character_set_server", MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_SERVER, true, true},
    {"character_set_system", MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_SYSTEM, true, true},
    {"collation_connection", MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_CONNECTION, true, true},
    {"collation_database", MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_DATABASE, true, true},
    {"collation_server", MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_SERVER, true, true},
    {"datadir", MYLITE_EXECUTION_SYSTEM_VARIABLE_DATADIR, true, true},
    {"default_collation_for_utf8mb4",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_COLLATION_FOR_UTF8MB4,
     false,
     false},
    {"default_storage_engine", MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE, true, true},
    {"default_tmp_storage_engine",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TMP_STORAGE_ENGINE,
     true,
     true},
    {"end_markers_in_json", MYLITE_EXECUTION_SYSTEM_VARIABLE_END_MARKERS_IN_JSON, false, false},
    {"error_count", MYLITE_EXECUTION_SYSTEM_VARIABLE_ERROR_COUNT, true, false},
    {"explicit_defaults_for_timestamp",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLICIT_DEFAULTS_FOR_TIMESTAMP,
     true,
     true},
    {"foreign_key_checks", MYLITE_EXECUTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS, true, true},
    {"group_concat_max_len", MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN, true, true},
    {"gtid_executed", MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_EXECUTED, true, true},
    {"gtid_mode", MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_MODE, true, true},
    {"gtid_owned", MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_OWNED, true, true},
    {"gtid_purged", MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_PURGED, true, true},
    {"hostname", MYLITE_EXECUTION_SYSTEM_VARIABLE_HOSTNAME, true, true},
    {"information_schema_stats_expiry",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INFORMATION_SCHEMA_STATS_EXPIRY,
     true,
     true},
    {"innodb_read_only", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_ONLY, true, true},
    {"interactive_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT, true, true},
    {"keep_files_on_create", MYLITE_EXECUTION_SYSTEM_VARIABLE_KEEP_FILES_ON_CREATE, false, false},
    {"license", MYLITE_EXECUTION_SYSTEM_VARIABLE_LICENSE, true, true},
    {"log_bin", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN, true, true},
    {"log_bin_basename", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_BASENAME, true, true},
    {"log_bin_index", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_INDEX, true, true},
    {"log_bin_trust_function_creators",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS,
     true,
     true},
    {"lower_case_file_system", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_FILE_SYSTEM, true, true},
    {"lower_case_table_names", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_TABLE_NAMES, true, true},
    {"max_allowed_packet", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ALLOWED_PACKET, true, true},
    {"max_error_count", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ERROR_COUNT, true, true},
    {"old_alter_table", MYLITE_EXECUTION_SYSTEM_VARIABLE_OLD_ALTER_TABLE, false, false},
    {"pid_file", MYLITE_EXECUTION_SYSTEM_VARIABLE_PID_FILE, true, true},
    {"plugin_dir", MYLITE_EXECUTION_SYSTEM_VARIABLE_PLUGIN_DIR, true, true},
    {"port", MYLITE_EXECUTION_SYSTEM_VARIABLE_PORT, true, true},
    {"print_identified_with_as_hex",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PRINT_IDENTIFIED_WITH_AS_HEX,
     false,
     false},
    {"protocol_version", MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_VERSION, true, true},
    {"read_only", MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_ONLY, true, true},
    {"require_row_format", MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_ROW_FORMAT, false, false},
    {"resultset_metadata", MYLITE_EXECUTION_SYSTEM_VARIABLE_RESULTSET_METADATA, false, false},
    {"select_into_disk_sync", MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_DISK_SYNC, false, false},
    {"server_id", MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID, true, true},
    {"server_id_bits", MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID_BITS, true, true},
    {"server_uuid", MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_UUID, true, true},
    {"socket", MYLITE_EXECUTION_SYSTEM_VARIABLE_SOCKET, true, true},
    {"session_track_gtids", MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_GTIDS, false, false},
    {"session_track_schema", MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_SCHEMA, false, false},
    {"session_track_state_change",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_STATE_CHANGE,
     false,
     false},
    {"session_track_transaction_info",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_TRANSACTION_INFO,
     false,
     false},
    {"show_create_table_skip_secondary_engine",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_SKIP_SECONDARY_ENGINE,
     false,
     false},
    {"show_create_table_verbosity",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_VERBOSITY,
     false,
     false},
    {"sql_auto_is_null", MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL, true, true},
    {"sql_big_selects", MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BIG_SELECTS, true, true},
    {"sql_buffer_result", MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT, true, true},
    {"sql_generate_invisible_primary_key",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_GENERATE_INVISIBLE_PRIMARY_KEY,
     true,
     true},
    {"sql_log_bin", MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_BIN, true, false},
    {"sql_log_off", MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_OFF, true, true},
    {"sql_mode", MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_MODE, true, true},
    {"sql_notes", MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_NOTES, true, true},
    {"sql_quote_show_create", MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_QUOTE_SHOW_CREATE, true, true},
    {"sql_replica_skip_counter",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REPLICA_SKIP_COUNTER,
     true,
     true},
    {"sql_require_primary_key", MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY, true, true
    },
    {"sql_safe_updates", MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES, true, true},
    {"sql_select_limit", MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT, true, true},
    {"sql_slave_skip_counter", MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER, true, true},
    {"sql_warnings", MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_WARNINGS, true, true},
    {"super_read_only", MYLITE_EXECUTION_SYSTEM_VARIABLE_SUPER_READ_ONLY, true, true},
    {"system_time_zone", MYLITE_EXECUTION_SYSTEM_VARIABLE_SYSTEM_TIME_ZONE, true, true},
    {"timestamp", MYLITE_EXECUTION_SYSTEM_VARIABLE_TIMESTAMP, true, false},
    {"time_zone", MYLITE_EXECUTION_SYSTEM_VARIABLE_TIME_ZONE, true, true},
    {"transaction_isolation", MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ISOLATION, true, true},
    {"transaction_read_only", MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_READ_ONLY, true, true},
    {"unique_checks", MYLITE_EXECUTION_SYSTEM_VARIABLE_UNIQUE_CHECKS, true, true},
    {"updatable_views_with_limit",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_UPDATABLE_VIEWS_WITH_LIMIT,
     true,
     true},
    {"use_secondary_engine", MYLITE_EXECUTION_SYSTEM_VARIABLE_USE_SECONDARY_ENGINE, false, false},
    {"version", MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION, true, true},
    {"version_comment", MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMMENT, true, true},
    {"version_compile_machine", MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_MACHINE, true, true
    },
    {"version_compile_os", MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_OS, true, true},
    {"version_compile_zlib", MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_ZLIB, true, true},
    {"wait_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT, true, true},
    {"warning_count", MYLITE_EXECUTION_SYSTEM_VARIABLE_WARNING_COUNT, true, false},
};

static const struct mylite_execution_show_status_descriptor show_status_descriptors[] = {
    {"Aborted_clients", "0", true, true},
    {"Aborted_connects", "0", true, true},
    {"Bytes_received", "0", true, true},
    {"Bytes_sent", "0", true, true},
    {"Com_begin", "0", true, true},
    {"Com_commit", "0", true, true},
    {"Com_delete", "0", true, true},
    {"Com_insert", "0", true, true},
    {"Com_replace", "0", true, true},
    {"Com_rollback", "0", true, true},
    {"Com_select", "0", true, true},
    {"Com_set_option", "0", true, true},
    {"Com_show_status", "0", true, true},
    {"Com_show_variables", "0", true, true},
    {"Com_update", "0", true, true},
    {"Compression", "OFF", true, false},
    {"Connections", "1", true, true},
    {"Created_tmp_disk_tables", "0", true, true},
    {"Created_tmp_files", "0", true, true},
    {"Created_tmp_tables", "0", true, true},
    {"Handler_delete", "0", true, true},
    {"Handler_read_first", "0", true, true},
    {"Handler_read_key", "0", true, true},
    {"Handler_read_next", "0", true, true},
    {"Handler_read_rnd", "0", true, true},
    {"Handler_read_rnd_next", "0", true, true},
    {"Handler_update", "0", true, true},
    {"Handler_write", "0", true, true},
    {"Open_files", "0", true, true},
    {"Open_streams", "0", true, true},
    {"Open_table_definitions", "0", true, true},
    {"Open_tables", "0", true, true},
    {"Opened_files", "0", true, true},
    {"Opened_table_definitions", "0", true, true},
    {"Opened_tables", "0", true, true},
    {"Prepared_stmt_count", "0", true, true},
    {"Queries", "0", true, true},
    {"Questions", "0", true, true},
    {"Select_full_join", "0", true, true},
    {"Select_full_range_join", "0", true, true},
    {"Select_range", "0", true, true},
    {"Select_scan", "0", true, true},
    {"Slow_queries", "0", true, true},
    {"Ssl_cipher", "", true, true},
    {"Ssl_version", "", true, true},
    {"Table_locks_immediate", "0", true, true},
    {"Table_locks_waited", "0", true, true},
    {"Threads_cached", "0", true, true},
    {"Threads_connected", "1", true, true},
    {"Threads_created", "1", true, true},
    {"Threads_running", "1", true, true},
    {"Uptime", "0", true, true},
    {"Uptime_since_flush_status", "0", true, true},
};

static const struct mylite_execution_sql_mode_descriptor sql_mode_descriptors[] = {
    {"REAL_AS_FLOAT", MYLITE_SESSION_SQL_MODE_REAL_AS_FLOAT, MYLITE_SESSION_SQL_MODE_REAL_AS_FLOAT},
    {"PIPES_AS_CONCAT",
     MYLITE_SESSION_SQL_MODE_PIPES_AS_CONCAT,
     MYLITE_SESSION_SQL_MODE_PIPES_AS_CONCAT},
    {"ANSI_QUOTES", MYLITE_SESSION_SQL_MODE_ANSI_QUOTES, MYLITE_SESSION_SQL_MODE_ANSI_QUOTES},
    {"IGNORE_SPACE", MYLITE_SESSION_SQL_MODE_IGNORE_SPACE, MYLITE_SESSION_SQL_MODE_IGNORE_SPACE},
    {"ONLY_FULL_GROUP_BY",
     MYLITE_SESSION_SQL_MODE_ONLY_FULL_GROUP_BY,
     MYLITE_SESSION_SQL_MODE_ONLY_FULL_GROUP_BY},
    {"NO_UNSIGNED_SUBTRACTION",
     MYLITE_SESSION_SQL_MODE_NO_UNSIGNED_SUBTRACTION,
     MYLITE_SESSION_SQL_MODE_NO_UNSIGNED_SUBTRACTION},
    {"NO_DIR_IN_CREATE",
     MYLITE_SESSION_SQL_MODE_NO_DIR_IN_CREATE,
     MYLITE_SESSION_SQL_MODE_NO_DIR_IN_CREATE},
    {"ANSI",
     MYLITE_SESSION_SQL_MODE_ANSI,
     MYLITE_SESSION_SQL_MODE_REAL_AS_FLOAT | MYLITE_SESSION_SQL_MODE_PIPES_AS_CONCAT |
         MYLITE_SESSION_SQL_MODE_ANSI_QUOTES | MYLITE_SESSION_SQL_MODE_IGNORE_SPACE |
         MYLITE_SESSION_SQL_MODE_ONLY_FULL_GROUP_BY | MYLITE_SESSION_SQL_MODE_ANSI},
    {"NO_AUTO_VALUE_ON_ZERO",
     MYLITE_SESSION_SQL_MODE_NO_AUTO_VALUE_ON_ZERO,
     MYLITE_SESSION_SQL_MODE_NO_AUTO_VALUE_ON_ZERO},
    {"NO_BACKSLASH_ESCAPES",
     MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES,
     MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES},
    {"STRICT_TRANS_TABLES",
     MYLITE_SESSION_SQL_MODE_STRICT_TRANS_TABLES,
     MYLITE_SESSION_SQL_MODE_STRICT_TRANS_TABLES},
    {"STRICT_ALL_TABLES",
     MYLITE_SESSION_SQL_MODE_STRICT_ALL_TABLES,
     MYLITE_SESSION_SQL_MODE_STRICT_ALL_TABLES},
    {"NO_ZERO_IN_DATE",
     MYLITE_SESSION_SQL_MODE_NO_ZERO_IN_DATE,
     MYLITE_SESSION_SQL_MODE_NO_ZERO_IN_DATE},
    {"NO_ZERO_DATE", MYLITE_SESSION_SQL_MODE_NO_ZERO_DATE, MYLITE_SESSION_SQL_MODE_NO_ZERO_DATE},
    {"ALLOW_INVALID_DATES",
     MYLITE_SESSION_SQL_MODE_ALLOW_INVALID_DATES,
     MYLITE_SESSION_SQL_MODE_ALLOW_INVALID_DATES},
    {"ERROR_FOR_DIVISION_BY_ZERO",
     MYLITE_SESSION_SQL_MODE_ERROR_FOR_DIVISION_BY_ZERO,
     MYLITE_SESSION_SQL_MODE_ERROR_FOR_DIVISION_BY_ZERO},
    {"TRADITIONAL",
     MYLITE_SESSION_SQL_MODE_TRADITIONAL,
     MYLITE_SESSION_SQL_MODE_STRICT_TRANS_TABLES | MYLITE_SESSION_SQL_MODE_STRICT_ALL_TABLES |
         MYLITE_SESSION_SQL_MODE_NO_ZERO_IN_DATE | MYLITE_SESSION_SQL_MODE_NO_ZERO_DATE |
         MYLITE_SESSION_SQL_MODE_ERROR_FOR_DIVISION_BY_ZERO | MYLITE_SESSION_SQL_MODE_TRADITIONAL |
         MYLITE_SESSION_SQL_MODE_NO_ENGINE_SUBSTITUTION},
    {"HIGH_NOT_PRECEDENCE",
     MYLITE_SESSION_SQL_MODE_HIGH_NOT_PRECEDENCE,
     MYLITE_SESSION_SQL_MODE_HIGH_NOT_PRECEDENCE},
    {"NO_ENGINE_SUBSTITUTION",
     MYLITE_SESSION_SQL_MODE_NO_ENGINE_SUBSTITUTION,
     MYLITE_SESSION_SQL_MODE_NO_ENGINE_SUBSTITUTION},
    {"PAD_CHAR_TO_FULL_LENGTH",
     MYLITE_SESSION_SQL_MODE_PAD_CHAR_TO_FULL_LENGTH,
     MYLITE_SESSION_SQL_MODE_PAD_CHAR_TO_FULL_LENGTH},
    {"TIME_TRUNCATE_FRACTIONAL",
     MYLITE_SESSION_SQL_MODE_TIME_TRUNCATE_FRACTIONAL,
     MYLITE_SESSION_SQL_MODE_TIME_TRUNCATE_FRACTIONAL},
};

size_t mylite_execution_system_variable_descriptor_count(void) {
    return sizeof(system_variable_descriptors) / sizeof(system_variable_descriptors[0]);
}

const struct mylite_execution_system_variable_descriptor *mylite_execution_system_variable_descriptor_at(
    size_t index
) {
    if (index >= mylite_execution_system_variable_descriptor_count()) {
        return NULL;
    }
    return &system_variable_descriptors[index];
}

const struct mylite_execution_system_variable_descriptor *mylite_execution_system_variable_descriptor_by_name(
    const char *name
) {
    if (name == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < mylite_execution_system_variable_descriptor_count(); ++index) {
        if (mylite_execution_sql_mode_token_matches(
                name,
                strlen(name),
                system_variable_descriptors[index].name
            )) {
            return &system_variable_descriptors[index];
        }
    }
    return NULL;
}

const struct mylite_execution_system_variable_descriptor *mylite_execution_system_variable_descriptor_by_kind(
    enum mylite_execution_system_variable_kind kind
) {
    for (size_t index = 0U; index < mylite_execution_system_variable_descriptor_count(); ++index) {
        if (system_variable_descriptors[index].kind == kind) {
            return &system_variable_descriptors[index];
        }
    }
    return NULL;
}

bool mylite_execution_system_variable_allows_global_scope(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BASEDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_CONNECTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_CONNECTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_SERVER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_SERVER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_DATABASE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_DATABASE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DATADIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TMP_STORAGE_ENGINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_SYSTEM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_FILESYSTEM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOCOMMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BIG_TABLES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_QUOTE_SHOW_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UNIQUE_CHECKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UPDATABLE_VIEWS_WITH_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BIG_SELECTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLICIT_DEFAULTS_FOR_TIMESTAMP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_GENERATE_INVISIBLE_PRIMARY_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_OFF:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REPLICA_SKIP_COUNTER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_NOTES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_WARNINGS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HOSTNAME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYSTEM_TIME_ZONE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TIME_ZONE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_EXECUTED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_OWNED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_PURGED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INFORMATION_SCHEMA_STATS_EXPIRY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LICENSE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_BASENAME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_INDEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_FILE_SYSTEM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_TABLE_NAMES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ALLOWED_PACKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ERROR_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PID_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PLUGIN_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID_BITS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_UUID:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SOCKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SUPER_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ISOLATION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_MACHINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_OS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_ZLIB:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_allows_session_scope(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BASEDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_SYSTEM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DATADIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HOSTNAME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LICENSE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PID_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PLUGIN_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SOCKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REPLICA_SKIP_COUNTER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYSTEM_TIME_ZONE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_EXECUTED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_PURGED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_BASENAME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_INDEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_FILE_SYSTEM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_TABLE_NAMES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID_BITS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_UUID:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SUPER_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_MACHINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_OS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_ZLIB:
        return false;
    default:
        return true;
    }
}

bool mylite_execution_system_variable_warns_on_scalar_read(
    enum mylite_execution_system_variable_kind kind
) {
    return (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS) != 0;
}

bool mylite_execution_system_variable_fixed_boolean_value(
    enum mylite_execution_system_variable_kind kind,
    bool *out_value
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_QUOTE_SHOW_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UNIQUE_CHECKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BIG_SELECTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLICIT_DEFAULTS_FOR_TIMESTAMP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_NOTES:
        if (out_value != NULL) {
            *out_value = true;
        }
        return true;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_GENERATE_INVISIBLE_PRIMARY_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_OFF:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_WARNINGS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SUPER_READ_ONLY:
        if (out_value != NULL) {
            *out_value = false;
        }
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_global_read_only_toggle(
    enum mylite_execution_system_variable_kind kind
) {
    return (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_ONLY ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SUPER_READ_ONLY) != 0;
}

bool mylite_execution_system_variable_is_read_only_server_identity_binary_log(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_BASENAME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_INDEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_UUID:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_read_only_server_build(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_MACHINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_OS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_ZLIB:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_read_only_server_environment(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BASEDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DATADIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HOSTNAME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LICENSE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PID_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PLUGIN_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SOCKET:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_timeout(enum mylite_execution_system_variable_kind kind) {
    return (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT) != 0;
}

bool mylite_execution_system_variable_is_session_placeholder(
    enum mylite_execution_system_variable_kind kind
) {
    return mylite_execution_system_variable_default_value(kind) != NULL;
}

bool mylite_execution_system_variable_is_boolean_session_placeholder(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_END_MARKERS_IN_JSON:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLICIT_DEFAULTS_FOR_TIMESTAMP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEEP_FILES_ON_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OLD_ALTER_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PRINT_IDENTIFIED_WITH_AS_HEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_ROW_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_DISK_SYNC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_STATE_CHANGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_SKIP_SECONDARY_ENGINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_VERBOSITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UPDATABLE_VIEWS_WITH_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BIG_SELECTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_OFF:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_NOTES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_QUOTE_SHOW_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UNIQUE_CHECKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_WARNINGS:
        return true;
    default:
        return false;
    }
}

const char *mylite_execution_system_variable_default_value(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_COLLATION_FOR_UTF8MB4:
        return "utf8mb4_0900_ai_ci";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TMP_STORAGE_ENGINE:
        return "InnoDB";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_END_MARKERS_IN_JSON:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEEP_FILES_ON_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OLD_ALTER_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PRINT_IDENTIFIED_WITH_AS_HEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_ROW_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_DISK_SYNC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_STATE_CHANGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_SKIP_SECONDARY_ENGINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_VERBOSITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_OFF:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_WARNINGS:
        return "0";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLICIT_DEFAULTS_FOR_TIMESTAMP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BIG_SELECTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_NOTES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_QUOTE_SHOW_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UNIQUE_CHECKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UPDATABLE_VIEWS_WITH_LIMIT:
        return "1";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RESULTSET_METADATA:
        return "FULL";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_GTIDS:
        return "OWN_GTID";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_TRANSACTION_INFO:
        return "STATE";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_USE_SECONDARY_ENGINE:
        return "FORCED";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOCOMMIT:
        return "1";
    default:
        return NULL;
    }
}

size_t mylite_execution_show_status_descriptor_count(void) {
    return sizeof(show_status_descriptors) / sizeof(show_status_descriptors[0]);
}

const struct mylite_execution_show_status_descriptor *mylite_execution_show_status_descriptor_at(
    size_t index
) {
    if (index >= mylite_execution_show_status_descriptor_count()) {
        return NULL;
    }
    return &show_status_descriptors[index];
}

bool mylite_execution_sql_mode_token_matches(
    const char *text,
    size_t length,
    const char *expected
) {
    if (text == NULL || expected == NULL || strlen(expected) != length) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        if (execution_system_variable_ascii_lower(text[index]) !=
            execution_system_variable_ascii_lower(expected[index])) {
            return false;
        }
    }
    return true;
}

int mylite_execution_sql_mode_parse(
    const char *text,
    uint64_t *out_modes,
    const char **out_invalid_token,
    size_t *out_invalid_token_length
) {
    size_t token_start = 0U;
    size_t index = 0U;
    uint64_t modes = 0U;
    int rc = MYLITE_OK;

    if (out_invalid_token != NULL) {
        *out_invalid_token = NULL;
    }
    if (out_invalid_token_length != NULL) {
        *out_invalid_token_length = 0U;
    }
    if (text == NULL || out_modes == NULL) {
        return MYLITE_MISUSE;
    }

    for (;;) {
        if (text[index] == ',' || text[index] == '\0') {
            struct execution_sql_mode_token token = {
                .start = token_start,
                .length = index - token_start,
            };

            rc = execution_sql_mode_parse_token(
                text,
                token,
                &modes,
                out_invalid_token,
                out_invalid_token_length
            );
            if (rc != MYLITE_OK) {
                return rc;
            }
            token_start = index + 1U;
        }
        if (text[index] == '\0') {
            break;
        }
        ++index;
    }

    *out_modes = modes;
    return MYLITE_OK;
}

static int execution_sql_mode_parse_token(
    const char *text,
    struct execution_sql_mode_token token,
    uint64_t *modes,
    const char **out_invalid_token,
    size_t *out_invalid_token_length
) {
    uint64_t expansion = 0U;

    if (token.length == 0U) {
        return MYLITE_OK;
    }
    if (modes == NULL ||
        !execution_sql_mode_token_expansion(&text[token.start], token.length, &expansion)) {
        execution_sql_mode_set_invalid_token(
            text,
            token,
            out_invalid_token,
            out_invalid_token_length
        );
        return MYLITE_ERROR;
    }

    *modes |= expansion;
    return MYLITE_OK;
}

static bool execution_sql_mode_token_expansion(
    const char *text,
    size_t length,
    uint64_t *out_expansion
) {
    if (out_expansion == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(sql_mode_descriptors) / sizeof(sql_mode_descriptors[0]);
         ++index) {
        if (mylite_execution_sql_mode_token_matches(
                text,
                length,
                sql_mode_descriptors[index].name
            )) {
            *out_expansion = sql_mode_descriptors[index].expansion;
            return true;
        }
    }
    return false;
}

static void execution_sql_mode_set_invalid_token(
    const char *text,
    struct execution_sql_mode_token token,
    const char **out_invalid_token,
    size_t *out_invalid_token_length
) {
    if (out_invalid_token != NULL) {
        *out_invalid_token = &text[token.start];
    }
    if (out_invalid_token_length != NULL) {
        *out_invalid_token_length = token.length;
    }
}

int mylite_execution_sql_mode_format(uint64_t modes, char *buffer, size_t buffer_size) {
    size_t offset = 0U;

    if (buffer == NULL || buffer_size == 0U) {
        return MYLITE_NOMEM;
    }
    buffer[0] = '\0';
    for (size_t index = 0U; index < sizeof(sql_mode_descriptors) / sizeof(sql_mode_descriptors[0]);
         ++index) {
        const char *name = sql_mode_descriptors[index].name;
        size_t name_length = strlen(name);

        if ((modes & sql_mode_descriptors[index].bit) == 0U) {
            continue;
        }
        if (offset != 0U) {
            if (offset + 1U >= buffer_size) {
                return MYLITE_NOMEM;
            }
            buffer[offset] = ',';
            ++offset;
        }
        if (offset + name_length >= buffer_size) {
            return MYLITE_NOMEM;
        }
        memcpy(&buffer[offset], name, name_length);
        offset += name_length;
        buffer[offset] = '\0';
    }
    return MYLITE_OK;
}

static char execution_system_variable_ascii_lower(char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte - 'A' + 'a');
    }
    return byte;
}
