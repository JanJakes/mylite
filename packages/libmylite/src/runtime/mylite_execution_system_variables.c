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
    {"activate_all_roles_on_login",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_ACTIVATE_ALL_ROLES_ON_LOGIN,
     true,
     true},
    {"admin_address", MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_ADDRESS, true, true},
    {"admin_port", MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_PORT, true, true},
    {"admin_ssl_ca", MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CA, true, true},
    {"admin_ssl_capath", MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CAPATH, true, true},
    {"admin_ssl_cert", MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CERT, true, true},
    {"admin_ssl_cipher", MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CIPHER, true, true},
    {"admin_ssl_crl", MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CRL, true, true},
    {"admin_ssl_crlpath", MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CRLPATH, true, true},
    {"admin_ssl_key", MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_KEY, true, true},
    {"admin_tls_ciphersuites", MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_TLS_CIPHERSUITES, true, true},
    {"admin_tls_version", MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_TLS_VERSION, true, true},
    {"auto_increment_increment",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT,
     true,
     true},
    {"auto_increment_offset", MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET, true, true},
    {"auto_generate_certs", MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_GENERATE_CERTS, true, true},
    {"automatic_sp_privileges", MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOMATIC_SP_PRIVILEGES, true, true
    },
    {"authentication_policy", MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTHENTICATION_POLICY, true, true},
    {"autocommit", MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOCOMMIT, true, true},
    {"back_log", MYLITE_EXECUTION_SYSTEM_VARIABLE_BACK_LOG, true, true},
    {"basedir", MYLITE_EXECUTION_SYSTEM_VARIABLE_BASEDIR, true, true},
    {"big_tables", MYLITE_EXECUTION_SYSTEM_VARIABLE_BIG_TABLES, true, true},
    {"bind_address", MYLITE_EXECUTION_SYSTEM_VARIABLE_BIND_ADDRESS, true, true},
    {"binlog_cache_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_CACHE_SIZE, true, true},
    {"binlog_checksum", MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_CHECKSUM, true, true},
    {"binlog_direct_non_transactional_updates",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_DIRECT_NON_TRANSACTIONAL_UPDATES,
     true,
     true},
    {"binlog_encryption", MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ENCRYPTION, true, true},
    {"binlog_error_action", MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ERROR_ACTION, true, true},
    {"binlog_expire_logs_auto_purge",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_EXPIRE_LOGS_AUTO_PURGE,
     true,
     true},
    {"binlog_expire_logs_seconds",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_EXPIRE_LOGS_SECONDS,
     true,
     true},
    {"binlog_format", MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_FORMAT, true, true},
    {"binlog_group_commit_sync_delay",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GROUP_COMMIT_SYNC_DELAY,
     true,
     true},
    {"binlog_group_commit_sync_no_delay_count",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GROUP_COMMIT_SYNC_NO_DELAY_COUNT,
     true,
     true},
    {"binlog_gtid_simple_recovery",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GTID_SIMPLE_RECOVERY,
     true,
     true},
    {"binlog_max_flush_queue_time",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_MAX_FLUSH_QUEUE_TIME,
     true,
     true},
    {"binlog_order_commits", MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ORDER_COMMITS, true, true},
    {"binlog_rotate_encryption_master_key_at_startup",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROTATE_ENCRYPTION_MASTER_KEY_AT_STARTUP,
     true,
     true},
    {"binlog_row_event_max_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_EVENT_MAX_SIZE,
     true,
     true},
    {"binlog_row_image", MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_IMAGE, true, true},
    {"binlog_row_metadata", MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_METADATA, true, true},
    {"binlog_row_value_options",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_VALUE_OPTIONS,
     true,
     true},
    {"binlog_rows_query_log_events",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROWS_QUERY_LOG_EVENTS,
     true,
     true},
    {"binlog_stmt_cache_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_STMT_CACHE_SIZE, true, true},
    {"binlog_transaction_compression",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_COMPRESSION,
     true,
     true},
    {"binlog_transaction_compression_level_zstd",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_COMPRESSION_LEVEL_ZSTD,
     true,
     true},
    {"binlog_transaction_dependency_history_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_DEPENDENCY_HISTORY_SIZE,
     true,
     true},
    {"block_encryption_mode", MYLITE_EXECUTION_SYSTEM_VARIABLE_BLOCK_ENCRYPTION_MODE, true, true},
    {"build_id", MYLITE_EXECUTION_SYSTEM_VARIABLE_BUILD_ID, true, true},
    {"bulk_insert_buffer_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_BULK_INSERT_BUFFER_SIZE, true, true
    },
    {"caching_sha2_password_auto_generate_rsa_keys",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_AUTO_GENERATE_RSA_KEYS,
     true,
     true},
    {"caching_sha2_password_digest_rounds",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_DIGEST_ROUNDS,
     true,
     true},
    {"caching_sha2_password_private_key_path",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_PRIVATE_KEY_PATH,
     true,
     true},
    {"caching_sha2_password_public_key_path",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_PUBLIC_KEY_PATH,
     true,
     true},
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
    {"character_sets_dir", MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SETS_DIR, true, true},
    {"check_proxy_users", MYLITE_EXECUTION_SYSTEM_VARIABLE_CHECK_PROXY_USERS, true, true},
    {"collation_connection", MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_CONNECTION, true, true},
    {"collation_database", MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_DATABASE, true, true},
    {"collation_server", MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_SERVER, true, true},
    {"completion_type", MYLITE_EXECUTION_SYSTEM_VARIABLE_COMPLETION_TYPE, true, true},
    {"concurrent_insert", MYLITE_EXECUTION_SYSTEM_VARIABLE_CONCURRENT_INSERT, true, true},
    {"connect_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECT_TIMEOUT, true, true},
    {"connection_control_failed_connections_threshold",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_FAILED_CONNECTIONS_THRESHOLD,
     true,
     true},
    {"connection_control_max_connection_delay",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_MAX_CONNECTION_DELAY,
     true,
     true},
    {"connection_control_min_connection_delay",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_MIN_CONNECTION_DELAY,
     true,
     true},
    {"connection_memory_chunk_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_MEMORY_CHUNK_SIZE,
     true,
     true},
    {"connection_memory_limit", MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_MEMORY_LIMIT, true, true
    },
    {"core_file", MYLITE_EXECUTION_SYSTEM_VARIABLE_CORE_FILE, true, true},
    {"create_admin_listener_thread",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_CREATE_ADMIN_LISTENER_THREAD,
     true,
     true},
    {"cte_max_recursion_depth", MYLITE_EXECUTION_SYSTEM_VARIABLE_CTE_MAX_RECURSION_DEPTH, true, true
    },
    {"datadir", MYLITE_EXECUTION_SYSTEM_VARIABLE_DATADIR, true, true},
    {"default_collation_for_utf8mb4",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_COLLATION_FOR_UTF8MB4,
     true,
     true},
    {"default_password_lifetime",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_PASSWORD_LIFETIME,
     true,
     true},
    {"default_storage_engine", MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE, true, true},
    {"default_table_encryption",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TABLE_ENCRYPTION,
     true,
     true},
    {"default_tmp_storage_engine",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TMP_STORAGE_ENGINE,
     true,
     true},
    {"default_week_format", MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_WEEK_FORMAT, true, true},
    {"delay_key_write", MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAY_KEY_WRITE, true, true},
    {"delayed_insert_limit", MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_LIMIT, true, true},
    {"delayed_insert_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_TIMEOUT, true, true},
    {"delayed_queue_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_QUEUE_SIZE, true, true},
    {"disconnect_on_expired_password",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_DISCONNECT_ON_EXPIRED_PASSWORD,
     true,
     true},
    {"disabled_storage_engines",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_DISABLED_STORAGE_ENGINES,
     true,
     true},
    {"div_precision_increment", MYLITE_EXECUTION_SYSTEM_VARIABLE_DIV_PRECISION_INCREMENT, true, true
    },
    {"end_markers_in_json", MYLITE_EXECUTION_SYSTEM_VARIABLE_END_MARKERS_IN_JSON, true, true},
    {"enforce_gtid_consistency",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_ENFORCE_GTID_CONSISTENCY,
     true,
     true},
    {"eq_range_index_dive_limit",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_EQ_RANGE_INDEX_DIVE_LIMIT,
     true,
     true},
    {"error_count", MYLITE_EXECUTION_SYSTEM_VARIABLE_ERROR_COUNT, true, false},
    {"event_scheduler", MYLITE_EXECUTION_SYSTEM_VARIABLE_EVENT_SCHEDULER, true, true},
    {"explain_format", MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLAIN_FORMAT, true, true},
    {"explain_json_format_version",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLAIN_JSON_FORMAT_VERSION,
     true,
     true},
    {"explicit_defaults_for_timestamp",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLICIT_DEFAULTS_FOR_TIMESTAMP,
     true,
     true},
    {"external_user", MYLITE_EXECUTION_SYSTEM_VARIABLE_EXTERNAL_USER, true, false},
    {"flush", MYLITE_EXECUTION_SYSTEM_VARIABLE_FLUSH, true, true},
    {"flush_time", MYLITE_EXECUTION_SYSTEM_VARIABLE_FLUSH_TIME, true, true},
    {"foreign_key_checks", MYLITE_EXECUTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS, true, true},
    {"ft_boolean_syntax", MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_BOOLEAN_SYNTAX, true, true},
    {"ft_max_word_len", MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_MAX_WORD_LEN, true, true},
    {"ft_min_word_len", MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_MIN_WORD_LEN, true, true},
    {"ft_query_expansion_limit",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_QUERY_EXPANSION_LIMIT,
     true,
     true},
    {"ft_stopword_file", MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_STOPWORD_FILE, true, true},
    {"generated_random_password_length",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERATED_RANDOM_PASSWORD_LENGTH,
     true,
     true},
    {"general_log", MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERAL_LOG, true, true},
    {"general_log_file", MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERAL_LOG_FILE, true, true},
    {"group_concat_max_len", MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN, true, true},
    {"group_replication_consistency",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_REPLICATION_CONSISTENCY,
     true,
     true},
    {"gtid_executed", MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_EXECUTED, true, true},
    {"gtid_executed_compression_period",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_EXECUTED_COMPRESSION_PERIOD,
     true,
     true},
    {"gtid_mode", MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_MODE, true, true},
    {"gtid_next", MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_NEXT, true, false},
    {"gtid_owned", MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_OWNED, true, true},
    {"gtid_purged", MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_PURGED, true, true},
    {"global_connection_memory_limit",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_GLOBAL_CONNECTION_MEMORY_LIMIT,
     true,
     true},
    {"global_connection_memory_tracking",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_GLOBAL_CONNECTION_MEMORY_TRACKING,
     true,
     true},
    {"have_compress", MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_COMPRESS, true, true},
    {"have_dynamic_loading", MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_DYNAMIC_LOADING, true, true},
    {"have_geometry", MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_GEOMETRY, true, true},
    {"have_profiling", MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_PROFILING, true, true},
    {"have_query_cache", MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_QUERY_CACHE, true, true},
    {"have_rtree_keys", MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_RTREE_KEYS, true, true},
    {"have_statement_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_STATEMENT_TIMEOUT, true, true},
    {"have_symlink", MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_SYMLINK, true, true},
    {"histogram_generation_max_mem_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_HISTOGRAM_GENERATION_MAX_MEM_SIZE,
     true,
     true},
    {"host_cache_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_HOST_CACHE_SIZE, true, true},
    {"hostname", MYLITE_EXECUTION_SYSTEM_VARIABLE_HOSTNAME, true, true},
    {"identity", MYLITE_EXECUTION_SYSTEM_VARIABLE_IDENTITY, true, false},
    {"immediate_server_version",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_IMMEDIATE_SERVER_VERSION,
     true,
     false},
    {"information_schema_stats_expiry",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INFORMATION_SCHEMA_STATS_EXPIRY,
     true,
     true},
    {"init_connect", MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_CONNECT, true, true},
    {"init_file", MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_FILE, true, true},
    {"init_replica", MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_REPLICA, true, true},
    {"init_slave", MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_SLAVE, true, true},
    {"innodb_read_only", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_ONLY, true, true},
    {"interactive_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT, true, true},
    {"keep_files_on_create", MYLITE_EXECUTION_SYSTEM_VARIABLE_KEEP_FILES_ON_CREATE, true, true},
    {"last_insert_id", MYLITE_EXECUTION_SYSTEM_VARIABLE_LAST_INSERT_ID, true, false},
    {"license", MYLITE_EXECUTION_SYSTEM_VARIABLE_LICENSE, true, true},
    {"log_bin", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN, true, true},
    {"log_bin_basename", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_BASENAME, true, true},
    {"log_bin_index", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_INDEX, true, true},
    {"log_bin_trust_function_creators",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS,
     true,
     true},
    {"log_error", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR, true, true},
    {"log_error_services", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_SERVICES, true, true},
    {"log_error_suppression_list",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_SUPPRESSION_LIST,
     true,
     true},
    {"log_error_verbosity", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_VERBOSITY, true, true},
    {"log_output", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_OUTPUT, true, true},
    {"log_queries_not_using_indexes",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_QUERIES_NOT_USING_INDEXES,
     true,
     true},
    {"log_raw", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_RAW, true, true},
    {"log_replica_updates", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_REPLICA_UPDATES, true, true},
    {"log_slave_updates", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLAVE_UPDATES, true, true},
    {"log_slow_admin_statements",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_ADMIN_STATEMENTS,
     true,
     true},
    {"log_slow_extra", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_EXTRA, true, true},
    {"log_slow_replica_statements",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_REPLICA_STATEMENTS,
     true,
     true},
    {"log_slow_slave_statements",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_SLAVE_STATEMENTS,
     true,
     true},
    {"log_statements_unsafe_for_binlog",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_STATEMENTS_UNSAFE_FOR_BINLOG,
     true,
     true},
    {"log_throttle_queries_not_using_indexes",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_THROTTLE_QUERIES_NOT_USING_INDEXES,
     true,
     true},
    {"log_timestamps", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_TIMESTAMPS, true, true},
    {"lock_wait_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCK_WAIT_TIMEOUT, true, true},
    {"long_query_time", MYLITE_EXECUTION_SYSTEM_VARIABLE_LONG_QUERY_TIME, true, true},
    {"low_priority_updates", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOW_PRIORITY_UPDATES, true, true},
    {"lower_case_file_system", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_FILE_SYSTEM, true, true},
    {"lower_case_table_names", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_TABLE_NAMES, true, true},
    {"max_allowed_packet", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ALLOWED_PACKET, true, true},
    {"max_error_count", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ERROR_COUNT, true, true},
    {"mysql_native_password_proxy_users",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQL_NATIVE_PASSWORD_PROXY_USERS,
     true,
     true},
    {"mysqlx_bind_address", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_BIND_ADDRESS, true, true},
    {"mysqlx_compression_algorithms",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_COMPRESSION_ALGORITHMS,
     true,
     true},
    {"mysqlx_connect_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_CONNECT_TIMEOUT, true, true},
    {"mysqlx_deflate_default_compression_level",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_DEFLATE_DEFAULT_COMPRESSION_LEVEL,
     true,
     true},
    {"mysqlx_deflate_max_client_compression_level",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_DEFLATE_MAX_CLIENT_COMPRESSION_LEVEL,
     true,
     true},
    {"mysqlx_document_id_unique_prefix",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_DOCUMENT_ID_UNIQUE_PREFIX,
     true,
     true},
    {"mysqlx_enable_hello_notice",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ENABLE_HELLO_NOTICE,
     true,
     true},
    {"mysqlx_idle_worker_thread_timeout",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_IDLE_WORKER_THREAD_TIMEOUT,
     true,
     true},
    {"mysqlx_interactive_timeout",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_INTERACTIVE_TIMEOUT,
     true,
     true},
    {"mysqlx_lz4_default_compression_level",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_LZ4_DEFAULT_COMPRESSION_LEVEL,
     true,
     true},
    {"mysqlx_lz4_max_client_compression_level",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_LZ4_MAX_CLIENT_COMPRESSION_LEVEL,
     true,
     true},
    {"mysqlx_max_allowed_packet",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_MAX_ALLOWED_PACKET,
     true,
     true},
    {"mysqlx_max_connections", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_MAX_CONNECTIONS, true, true},
    {"mysqlx_min_worker_threads",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_MIN_WORKER_THREADS,
     true,
     true},
    {"mysqlx_port", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_PORT, true, true},
    {"mysqlx_port_open_timeout",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_PORT_OPEN_TIMEOUT,
     true,
     true},
    {"mysqlx_read_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_READ_TIMEOUT, true, true},
    {"mysqlx_socket", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SOCKET, true, true},
    {"mysqlx_ssl_ca", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CA, true, true},
    {"mysqlx_ssl_capath", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CAPATH, true, true},
    {"mysqlx_ssl_cert", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CERT, true, true},
    {"mysqlx_ssl_cipher", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CIPHER, true, true},
    {"mysqlx_ssl_crl", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CRL, true, true},
    {"mysqlx_ssl_crlpath", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CRLPATH, true, true},
    {"mysqlx_ssl_key", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_KEY, true, true},
    {"mysqlx_wait_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_WAIT_TIMEOUT, true, true},
    {"mysqlx_write_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_WRITE_TIMEOUT, true, true},
    {"mysqlx_zstd_default_compression_level",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ZSTD_DEFAULT_COMPRESSION_LEVEL,
     true,
     true},
    {"mysqlx_zstd_max_client_compression_level",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ZSTD_MAX_CLIENT_COMPRESSION_LEVEL,
     true,
     true},
    {"net_read_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_READ_TIMEOUT, true, true},
    {"net_retry_count", MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_RETRY_COUNT, true, true},
    {"net_write_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_WRITE_TIMEOUT, true, true},
    {"old_alter_table", MYLITE_EXECUTION_SYSTEM_VARIABLE_OLD_ALTER_TABLE, true, true},
    {"password_history", MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_HISTORY, true, true},
    {"password_require_current",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REQUIRE_CURRENT,
     true,
     true},
    {"password_reuse_interval", MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REUSE_INTERVAL, true, true
    },
    {"pid_file", MYLITE_EXECUTION_SYSTEM_VARIABLE_PID_FILE, true, true},
    {"plugin_dir", MYLITE_EXECUTION_SYSTEM_VARIABLE_PLUGIN_DIR, true, true},
    {"port", MYLITE_EXECUTION_SYSTEM_VARIABLE_PORT, true, true},
    {"print_identified_with_as_hex",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PRINT_IDENTIFIED_WITH_AS_HEX,
     true,
     true},
    {"protocol_version", MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_VERSION, true, true},
    {"read_only", MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_ONLY, true, true},
    {"require_row_format", MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_ROW_FORMAT, true, false},
    {"require_secure_transport",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_SECURE_TRANSPORT,
     true,
     true},
    {"resultset_metadata", MYLITE_EXECUTION_SYSTEM_VARIABLE_RESULTSET_METADATA, true, false},
    {"secure_file_priv", MYLITE_EXECUTION_SYSTEM_VARIABLE_SECURE_FILE_PRIV, true, true},
    {"select_into_disk_sync", MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_DISK_SYNC, true, true},
    {"server_id", MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID, true, true},
    {"server_id_bits", MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID_BITS, true, true},
    {"server_uuid", MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_UUID, true, true},
    {"socket", MYLITE_EXECUTION_SYSTEM_VARIABLE_SOCKET, true, true},
    {"session_track_gtids", MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_GTIDS, true, true},
    {"session_track_schema", MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_SCHEMA, true, true},
    {"session_track_state_change",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_STATE_CHANGE,
     true,
     true},
    {"session_track_transaction_info",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_TRANSACTION_INFO,
     true,
     true},
    {"sha256_password_auto_generate_rsa_keys",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_AUTO_GENERATE_RSA_KEYS,
     true,
     true},
    {"sha256_password_private_key_path",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PRIVATE_KEY_PATH,
     true,
     true},
    {"sha256_password_proxy_users",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PROXY_USERS,
     true,
     true},
    {"sha256_password_public_key_path",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PUBLIC_KEY_PATH,
     true,
     true},
    {"show_create_table_skip_secondary_engine",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_SKIP_SECONDARY_ENGINE,
     true,
     false},
    {"show_create_table_verbosity",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_VERBOSITY,
     true,
     true},
    {"skip_external_locking", MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_EXTERNAL_LOCKING, true, true},
    {"skip_name_resolve", MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_NAME_RESOLVE, true, true},
    {"skip_networking", MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_NETWORKING, true, true},
    {"skip_show_database", MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SHOW_DATABASE, true, true},
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
    {"ssl_ca", MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CA, true, true},
    {"ssl_capath", MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CAPATH, true, true},
    {"ssl_cert", MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CERT, true, true},
    {"ssl_cipher", MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CIPHER, true, true},
    {"ssl_crl", MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CRL, true, true},
    {"ssl_crlpath", MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CRLPATH, true, true},
    {"ssl_fips_mode", MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_FIPS_MODE, true, true},
    {"ssl_key", MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_KEY, true, true},
    {"ssl_session_cache_mode", MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_SESSION_CACHE_MODE, true, true},
    {"ssl_session_cache_timeout",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_SESSION_CACHE_TIMEOUT,
     true,
     true},
    {"slow_launch_time", MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_LAUNCH_TIME, true, true},
    {"slow_query_log", MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_QUERY_LOG, true, true},
    {"slow_query_log_file", MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_QUERY_LOG_FILE, true, true},
    {"sort_buffer_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_SORT_BUFFER_SIZE, true, true},
    {"super_read_only", MYLITE_EXECUTION_SYSTEM_VARIABLE_SUPER_READ_ONLY, true, true},
    {"system_time_zone", MYLITE_EXECUTION_SYSTEM_VARIABLE_SYSTEM_TIME_ZONE, true, true},
    {"thread_handling", MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_HANDLING, true, true},
    {"timestamp", MYLITE_EXECUTION_SYSTEM_VARIABLE_TIMESTAMP, true, false},
    {"time_zone", MYLITE_EXECUTION_SYSTEM_VARIABLE_TIME_ZONE, true, true},
    {"tls_certificates_enforced_validation",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_CERTIFICATES_ENFORCED_VALIDATION,
     true,
     true},
    {"tls_ciphersuites", MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_CIPHERSUITES, true, true},
    {"tls_version", MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_VERSION, true, true},
    {"tmpdir", MYLITE_EXECUTION_SYSTEM_VARIABLE_TMPDIR, true, true},
    {"transaction_isolation", MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ISOLATION, true, true},
    {"transaction_read_only", MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_READ_ONLY, true, true},
    {"unique_checks", MYLITE_EXECUTION_SYSTEM_VARIABLE_UNIQUE_CHECKS, true, true},
    {"updatable_views_with_limit",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_UPDATABLE_VIEWS_WITH_LIMIT,
     true,
     true},
    {"use_secondary_engine", MYLITE_EXECUTION_SYSTEM_VARIABLE_USE_SECONDARY_ENGINE, true, false},
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
    {"Acl_cache_items_count", "0", true, true},
    {"Binlog_cache_disk_use", "0", true, true},
    {"Binlog_cache_use", "0", true, true},
    {"Binlog_stmt_cache_disk_use", "0", true, true},
    {"Binlog_stmt_cache_use", "0", true, true},
    {"Bytes_received", "0", true, true},
    {"Bytes_sent", "0", true, true},
    {"Caching_sha2_password_rsa_public_key", "", true, true},
    {"Com_admin_commands", "0", true, true},
    {"Com_assign_to_keycache", "0", true, true},
    {"Com_alter_db", "0", true, true},
    {"Com_alter_event", "0", true, true},
    {"Com_alter_function", "0", true, true},
    {"Com_alter_instance", "0", true, true},
    {"Com_alter_procedure", "0", true, true},
    {"Com_alter_resource_group", "0", true, true},
    {"Com_alter_server", "0", true, true},
    {"Com_alter_table", "0", true, true},
    {"Com_alter_tablespace", "0", true, true},
    {"Com_alter_user", "0", true, true},
    {"Com_alter_user_default_role", "0", true, true},
    {"Com_analyze", "0", true, true},
    {"Com_begin", "0", true, true},
    {"Com_binlog", "0", true, true},
    {"Com_call_procedure", "0", true, true},
    {"Com_change_db", "0", true, true},
    {"Com_change_repl_filter", "0", true, true},
    {"Com_change_replication_source", "0", true, true},
    {"Com_check", "0", true, true},
    {"Com_checksum", "0", true, true},
    {"Com_clone", "0", true, true},
    {"Com_commit", "0", true, true},
    {"Com_create_db", "0", true, true},
    {"Com_create_event", "0", true, true},
    {"Com_create_function", "0", true, true},
    {"Com_create_index", "0", true, true},
    {"Com_create_procedure", "0", true, true},
    {"Com_create_role", "0", true, true},
    {"Com_create_server", "0", true, true},
    {"Com_create_table", "0", true, true},
    {"Com_create_resource_group", "0", true, true},
    {"Com_create_trigger", "0", true, true},
    {"Com_create_udf", "0", true, true},
    {"Com_create_user", "0", true, true},
    {"Com_create_view", "0", true, true},
    {"Com_create_spatial_reference_system", "0", true, true},
    {"Com_dealloc_sql", "0", true, true},
    {"Com_delete", "0", true, true},
    {"Com_delete_multi", "0", true, true},
    {"Com_do", "0", true, true},
    {"Com_drop_db", "0", true, true},
    {"Com_drop_event", "0", true, true},
    {"Com_drop_function", "0", true, true},
    {"Com_drop_index", "0", true, true},
    {"Com_drop_procedure", "0", true, true},
    {"Com_drop_resource_group", "0", true, true},
    {"Com_drop_role", "0", true, true},
    {"Com_drop_server", "0", true, true},
    {"Com_drop_spatial_reference_system", "0", true, true},
    {"Com_drop_table", "0", true, true},
    {"Com_drop_trigger", "0", true, true},
    {"Com_drop_user", "0", true, true},
    {"Com_drop_view", "0", true, true},
    {"Com_empty_query", "0", true, true},
    {"Com_execute_sql", "0", true, true},
    {"Com_explain_other", "0", true, true},
    {"Com_flush", "0", true, true},
    {"Com_get_diagnostics", "0", true, true},
    {"Com_grant", "0", true, true},
    {"Com_grant_roles", "0", true, true},
    {"Com_ha_close", "0", true, true},
    {"Com_ha_open", "0", true, true},
    {"Com_ha_read", "0", true, true},
    {"Com_help", "0", true, true},
    {"Com_import", "0", true, true},
    {"Com_insert", "0", true, true},
    {"Com_insert_select", "0", true, true},
    {"Com_install_component", "0", true, true},
    {"Com_install_plugin", "0", true, true},
    {"Com_kill", "0", true, true},
    {"Com_load", "0", true, true},
    {"Com_lock_instance", "0", true, true},
    {"Com_lock_tables", "0", true, true},
    {"Com_optimize", "0", true, true},
    {"Com_preload_keys", "0", true, true},
    {"Com_prepare_sql", "0", true, true},
    {"Com_purge", "0", true, true},
    {"Com_purge_before_date", "0", true, true},
    {"Com_release_savepoint", "0", true, true},
    {"Com_rename_table", "0", true, true},
    {"Com_rename_user", "0", true, true},
    {"Com_repair", "0", true, true},
    {"Com_replace", "0", true, true},
    {"Com_replace_select", "0", true, true},
    {"Com_reset", "0", true, true},
    {"Com_resignal", "0", true, true},
    {"Com_restart", "0", true, true},
    {"Com_revoke", "0", true, true},
    {"Com_revoke_all", "0", true, true},
    {"Com_revoke_roles", "0", true, true},
    {"Com_rollback", "0", true, true},
    {"Com_rollback_to_savepoint", "0", true, true},
    {"Com_savepoint", "0", true, true},
    {"Com_select", "0", true, true},
    {"Com_set_option", "0", true, true},
    {"Com_set_password", "0", true, true},
    {"Com_set_resource_group", "0", true, true},
    {"Com_set_role", "0", true, true},
    {"Com_signal", "0", true, true},
    {"Com_show_binlog_events", "0", true, true},
    {"Com_show_binlogs", "0", true, true},
    {"Com_show_charsets", "0", true, true},
    {"Com_show_collations", "0", true, true},
    {"Com_show_create_db", "0", true, true},
    {"Com_show_create_event", "0", true, true},
    {"Com_show_create_func", "0", true, true},
    {"Com_show_create_proc", "0", true, true},
    {"Com_show_create_table", "0", true, true},
    {"Com_show_create_trigger", "0", true, true},
    {"Com_show_databases", "0", true, true},
    {"Com_show_engine_logs", "0", true, true},
    {"Com_show_engine_mutex", "0", true, true},
    {"Com_show_engine_status", "0", true, true},
    {"Com_show_events", "0", true, true},
    {"Com_show_errors", "0", true, true},
    {"Com_show_fields", "0", true, true},
    {"Com_show_function_code", "0", true, true},
    {"Com_show_function_status", "0", true, true},
    {"Com_show_grants", "0", true, true},
    {"Com_show_keys", "0", true, true},
    {"Com_show_binary_log_status", "0", true, true},
    {"Com_show_open_tables", "0", true, true},
    {"Com_show_parse_tree", "0", true, true},
    {"Com_show_plugins", "0", true, true},
    {"Com_show_privileges", "0", true, true},
    {"Com_show_procedure_code", "0", true, true},
    {"Com_show_procedure_status", "0", true, true},
    {"Com_show_processlist", "0", true, true},
    {"Com_show_profile", "0", true, true},
    {"Com_show_profiles", "0", true, true},
    {"Com_show_relaylog_events", "0", true, true},
    {"Com_show_replicas", "0", true, true},
    {"Com_show_replica_status", "0", true, true},
    {"Com_show_status", "0", true, true},
    {"Com_show_storage_engines", "0", true, true},
    {"Com_show_table_status", "0", true, true},
    {"Com_show_tables", "0", true, true},
    {"Com_show_triggers", "0", true, true},
    {"Com_show_variables", "0", true, true},
    {"Com_show_warnings", "0", true, true},
    {"Com_show_create_user", "0", true, true},
    {"Com_shutdown", "0", true, true},
    {"Com_replica_start", "0", true, true},
    {"Com_replica_stop", "0", true, true},
    {"Com_group_replication_start", "0", true, true},
    {"Com_group_replication_stop", "0", true, true},
    {"Com_stmt_execute", "0", true, true},
    {"Com_stmt_close", "0", true, true},
    {"Com_stmt_fetch", "0", true, true},
    {"Com_stmt_prepare", "0", true, true},
    {"Com_stmt_reset", "0", true, true},
    {"Com_stmt_send_long_data", "0", true, true},
    {"Com_truncate", "0", true, true},
    {"Com_uninstall_component", "0", true, true},
    {"Com_uninstall_plugin", "0", true, true},
    {"Com_unlock_instance", "0", true, true},
    {"Com_unlock_tables", "0", true, true},
    {"Com_update", "0", true, true},
    {"Com_update_multi", "0", true, true},
    {"Com_xa_commit", "0", true, true},
    {"Com_xa_end", "0", true, true},
    {"Com_xa_prepare", "0", true, true},
    {"Com_xa_recover", "0", true, true},
    {"Com_xa_rollback", "0", true, true},
    {"Com_xa_start", "0", true, true},
    {"Com_stmt_reprepare", "0", true, true},
    {"Compression", "OFF", true, false},
    {"Compression_algorithm", "", true, false},
    {"Compression_level", "0", true, false},
    {"Connection_control_delay_generated", "0", true, true},
    {"Connection_control_exempted_unknown_users", "0", true, true},
    {"Connection_errors_accept", "0", true, true},
    {"Connection_errors_internal", "0", true, true},
    {"Connection_errors_max_connections", "0", true, true},
    {"Connection_errors_peer_address", "0", true, true},
    {"Connection_errors_select", "0", true, true},
    {"Connection_errors_tcpwrap", "0", true, true},
    {"Connections", "1", true, true},
    {"Created_tmp_disk_tables", "0", true, true},
    {"Created_tmp_files", "0", true, true},
    {"Created_tmp_tables", "0", true, true},
    {"Current_tls_ca", "", true, true},
    {"Current_tls_capath", "", true, true},
    {"Current_tls_cert", "", true, true},
    {"Current_tls_cipher", "", true, true},
    {"Current_tls_ciphersuites", "", true, true},
    {"Current_tls_crl", "", true, true},
    {"Current_tls_crlpath", "", true, true},
    {"Current_tls_key", "", true, true},
    {"Current_tls_version", "", true, true},
    {"Delayed_errors", "0", true, true},
    {"Delayed_insert_threads", "0", true, true},
    {"Delayed_writes", "0", true, true},
    {"Deprecated_use_fk_on_non_standard_key_count", "0", true, true},
    {"Deprecated_use_fk_on_non_standard_key_last_timestamp", "0", true, true},
    {"Deprecated_use_i_s_processlist_count", "0", true, true},
    {"Deprecated_use_i_s_processlist_last_timestamp", "0", true, true},
    {"Error_log_buffered_bytes", "0", true, true},
    {"Error_log_buffered_events", "0", true, true},
    {"Error_log_expired_events", "0", true, true},
    {"Error_log_latest_write", "0", true, true},
    {"Flush_commands", "0", true, true},
    {"Global_connection_memory", "0", true, true},
    {"Handler_commit", "0", true, true},
    {"Handler_delete", "0", true, true},
    {"Handler_discover", "0", true, true},
    {"Handler_external_lock", "0", true, true},
    {"Handler_mrr_init", "0", true, true},
    {"Handler_prepare", "0", true, true},
    {"Handler_read_first", "0", true, true},
    {"Handler_read_key", "0", true, true},
    {"Handler_read_last", "0", true, true},
    {"Handler_read_next", "0", true, true},
    {"Handler_read_prev", "0", true, true},
    {"Handler_read_rnd", "0", true, true},
    {"Handler_read_rnd_next", "0", true, true},
    {"Handler_rollback", "0", true, true},
    {"Handler_savepoint", "0", true, true},
    {"Handler_savepoint_rollback", "0", true, true},
    {"Handler_update", "0", true, true},
    {"Handler_write", "0", true, true},
    {"Innodb_buffer_pool_dump_status", "Dumping of buffer pool not started", true, true},
    {"Innodb_buffer_pool_load_status", "", true, true},
    {"Innodb_buffer_pool_resize_status", "", true, true},
    {"Innodb_buffer_pool_resize_status_code", "0", true, true},
    {"Innodb_buffer_pool_resize_status_progress", "0", true, true},
    {"Innodb_buffer_pool_pages_data", "0", true, true},
    {"Innodb_buffer_pool_bytes_data", "0", true, true},
    {"Innodb_buffer_pool_pages_dirty", "0", true, true},
    {"Innodb_buffer_pool_bytes_dirty", "0", true, true},
    {"Innodb_buffer_pool_pages_flushed", "0", true, true},
    {"Innodb_buffer_pool_pages_free", "0", true, true},
    {"Innodb_buffer_pool_pages_misc", "0", true, true},
    {"Innodb_buffer_pool_pages_total", "0", true, true},
    {"Innodb_buffer_pool_read_ahead_rnd", "0", true, true},
    {"Innodb_buffer_pool_read_ahead", "0", true, true},
    {"Innodb_buffer_pool_read_ahead_evicted", "0", true, true},
    {"Innodb_buffer_pool_read_requests", "0", true, true},
    {"Innodb_buffer_pool_reads", "0", true, true},
    {"Innodb_buffer_pool_wait_free", "0", true, true},
    {"Innodb_buffer_pool_write_requests", "0", true, true},
    {"Innodb_data_fsyncs", "0", true, true},
    {"Innodb_data_pending_fsyncs", "0", true, true},
    {"Innodb_data_pending_reads", "0", true, true},
    {"Innodb_data_pending_writes", "0", true, true},
    {"Innodb_data_read", "0", true, true},
    {"Innodb_data_reads", "0", true, true},
    {"Innodb_data_writes", "0", true, true},
    {"Innodb_data_written", "0", true, true},
    {"Innodb_dblwr_pages_written", "0", true, true},
    {"Innodb_dblwr_writes", "0", true, true},
    {"Innodb_redo_log_read_only", "OFF", true, true},
    {"Innodb_redo_log_uuid", "0", true, true},
    {"Innodb_redo_log_checkpoint_lsn", "0", true, true},
    {"Innodb_redo_log_current_lsn", "0", true, true},
    {"Innodb_redo_log_flushed_to_disk_lsn", "0", true, true},
    {"Innodb_redo_log_logical_size", "0", true, true},
    {"Innodb_redo_log_physical_size", "0", true, true},
    {"Innodb_redo_log_capacity_resized", "0", true, true},
    {"Innodb_redo_log_resize_status", "OK", true, true},
    {"Innodb_log_waits", "0", true, true},
    {"Innodb_log_write_requests", "0", true, true},
    {"Innodb_log_writes", "0", true, true},
    {"Innodb_os_log_fsyncs", "0", true, true},
    {"Innodb_os_log_pending_fsyncs", "0", true, true},
    {"Innodb_os_log_pending_writes", "0", true, true},
    {"Innodb_os_log_written", "0", true, true},
    {"Innodb_page_size", "16384", true, true},
    {"Innodb_pages_created", "0", true, true},
    {"Innodb_pages_read", "0", true, true},
    {"Innodb_pages_written", "0", true, true},
    {"Innodb_redo_log_enabled", "ON", true, true},
    {"Innodb_row_lock_current_waits", "0", true, true},
    {"Innodb_row_lock_time", "0", true, true},
    {"Innodb_row_lock_time_avg", "0", true, true},
    {"Innodb_row_lock_time_max", "0", true, true},
    {"Innodb_row_lock_waits", "0", true, true},
    {"Innodb_rows_deleted", "0", true, true},
    {"Innodb_rows_inserted", "0", true, true},
    {"Innodb_rows_read", "0", true, true},
    {"Innodb_rows_updated", "0", true, true},
    {"Innodb_system_rows_deleted", "0", true, true},
    {"Innodb_system_rows_inserted", "0", true, true},
    {"Innodb_system_rows_read", "0", true, true},
    {"Innodb_system_rows_updated", "0", true, true},
    {"Innodb_sampled_pages_read", "0", true, true},
    {"Innodb_sampled_pages_skipped", "0", true, true},
    {"Innodb_num_open_files", "0", true, true},
    {"Innodb_truncated_status_writes", "0", true, true},
    {"Innodb_undo_tablespaces_total", "0", true, true},
    {"Innodb_undo_tablespaces_implicit", "0", true, true},
    {"Innodb_undo_tablespaces_explicit", "0", true, true},
    {"Innodb_undo_tablespaces_active", "0", true, true},
    {"Key_blocks_not_flushed", "0", true, true},
    {"Key_blocks_unused", "0", true, true},
    {"Key_blocks_used", "0", true, true},
    {"Key_read_requests", "0", true, true},
    {"Key_reads", "0", true, true},
    {"Key_write_requests", "0", true, true},
    {"Key_writes", "0", true, true},
    {"Last_query_cost", "0.000000", true, true},
    {"Last_query_partial_plans", "0", true, true},
    {"Locked_connects", "0", true, true},
    {"Max_execution_time_exceeded", "0", true, true},
    {"Max_execution_time_set", "0", true, true},
    {"Max_execution_time_set_failed", "0", true, true},
    {"Max_used_connections", "1", true, true},
    {"Max_used_connections_time", "1970-01-01 00:00:00", true, true},
    {"Mysqlx_aborted_clients", "0", true, true},
    {"Mysqlx_address", "UNDEFINED", true, true},
    {"Mysqlx_bytes_received", "0", true, true},
    {"Mysqlx_bytes_received_compressed_payload", "0", true, true},
    {"Mysqlx_bytes_received_uncompressed_frame", "0", true, true},
    {"Mysqlx_bytes_sent", "0", true, true},
    {"Mysqlx_bytes_sent_compressed_payload", "0", true, true},
    {"Mysqlx_bytes_sent_uncompressed_frame", "0", true, true},
    {"Mysqlx_compression_algorithm", "", true, true},
    {"Mysqlx_compression_level", "", true, true},
    {"Mysqlx_connection_accept_errors", "0", true, true},
    {"Mysqlx_connection_errors", "0", true, true},
    {"Mysqlx_connections_accepted", "0", true, true},
    {"Mysqlx_connections_closed", "0", true, true},
    {"Mysqlx_connections_rejected", "0", true, true},
    {"Mysqlx_crud_create_view", "0", true, true},
    {"Mysqlx_crud_delete", "0", true, true},
    {"Mysqlx_crud_drop_view", "0", true, true},
    {"Mysqlx_crud_find", "0", true, true},
    {"Mysqlx_crud_insert", "0", true, true},
    {"Mysqlx_crud_modify_view", "0", true, true},
    {"Mysqlx_crud_update", "0", true, true},
    {"Mysqlx_cursor_close", "0", true, true},
    {"Mysqlx_cursor_fetch", "0", true, true},
    {"Mysqlx_cursor_open", "0", true, true},
    {"Mysqlx_errors_sent", "0", true, true},
    {"Mysqlx_errors_unknown_message_type", "0", true, true},
    {"Mysqlx_expect_close", "0", true, true},
    {"Mysqlx_expect_open", "0", true, true},
    {"Mysqlx_init_error", "0", true, true},
    {"Mysqlx_messages_sent", "0", true, true},
    {"Mysqlx_notice_global_sent", "0", true, true},
    {"Mysqlx_notice_other_sent", "0", true, true},
    {"Mysqlx_notice_warning_sent", "0", true, true},
    {"Mysqlx_notified_by_group_replication", "0", true, true},
    {"Mysqlx_port", "UNDEFINED", true, true},
    {"Mysqlx_prep_deallocate", "0", true, true},
    {"Mysqlx_prep_execute", "0", true, true},
    {"Mysqlx_prep_prepare", "0", true, true},
    {"Mysqlx_rows_sent", "0", true, true},
    {"Mysqlx_sessions", "0", true, true},
    {"Mysqlx_sessions_accepted", "0", true, true},
    {"Mysqlx_sessions_closed", "0", true, true},
    {"Mysqlx_sessions_fatal_error", "0", true, true},
    {"Mysqlx_sessions_killed", "0", true, true},
    {"Mysqlx_sessions_rejected", "0", true, true},
    {"Mysqlx_socket", "", true, true},
    {"Mysqlx_ssl_accepts", "0", true, true},
    {"Mysqlx_ssl_active", "", true, true},
    {"Mysqlx_ssl_cipher", "", true, true},
    {"Mysqlx_ssl_cipher_list", "", true, true},
    {"Mysqlx_ssl_ctx_verify_depth", "0", true, true},
    {"Mysqlx_ssl_ctx_verify_mode", "0", true, true},
    {"Mysqlx_ssl_finished_accepts", "0", true, true},
    {"Mysqlx_ssl_server_not_after", "", true, true},
    {"Mysqlx_ssl_server_not_before", "", true, true},
    {"Mysqlx_ssl_verify_depth", "", true, true},
    {"Mysqlx_ssl_verify_mode", "", true, true},
    {"Mysqlx_ssl_version", "", true, true},
    {"Mysqlx_stmt_create_collection", "0", true, true},
    {"Mysqlx_stmt_create_collection_index", "0", true, true},
    {"Mysqlx_stmt_disable_notices", "0", true, true},
    {"Mysqlx_stmt_drop_collection", "0", true, true},
    {"Mysqlx_stmt_drop_collection_index", "0", true, true},
    {"Mysqlx_stmt_enable_notices", "0", true, true},
    {"Mysqlx_stmt_ensure_collection", "0", true, true},
    {"Mysqlx_stmt_execute_mysqlx", "0", true, true},
    {"Mysqlx_stmt_execute_sql", "0", true, true},
    {"Mysqlx_stmt_execute_xplugin", "0", true, true},
    {"Mysqlx_stmt_get_collection_options", "0", true, true},
    {"Mysqlx_stmt_kill_client", "0", true, true},
    {"Mysqlx_stmt_list_clients", "0", true, true},
    {"Mysqlx_stmt_list_notices", "0", true, true},
    {"Mysqlx_stmt_list_objects", "0", true, true},
    {"Mysqlx_stmt_modify_collection_options", "0", true, true},
    {"Mysqlx_stmt_ping", "0", true, true},
    {"Mysqlx_worker_threads", "0", true, true},
    {"Mysqlx_worker_threads_active", "0", true, true},
    {"Not_flushed_delayed_rows", "0", true, true},
    {"Ongoing_anonymous_transaction_count", "0", true, true},
    {"Open_files", "0", true, true},
    {"Open_streams", "0", true, true},
    {"Open_table_definitions", "0", true, true},
    {"Open_tables", "0", true, true},
    {"Opened_files", "0", true, true},
    {"Opened_table_definitions", "0", true, true},
    {"Opened_tables", "0", true, true},
    {"Performance_schema_accounts_lost", "0", true, true},
    {"Performance_schema_cond_classes_lost", "0", true, true},
    {"Performance_schema_cond_instances_lost", "0", true, true},
    {"Performance_schema_digest_lost", "0", true, true},
    {"Performance_schema_file_classes_lost", "0", true, true},
    {"Performance_schema_file_handles_lost", "0", true, true},
    {"Performance_schema_file_instances_lost", "0", true, true},
    {"Performance_schema_hosts_lost", "0", true, true},
    {"Performance_schema_index_stat_lost", "0", true, true},
    {"Performance_schema_locker_lost", "0", true, true},
    {"Performance_schema_logger_lost", "0", true, true},
    {"Performance_schema_memory_classes_lost", "0", true, true},
    {"Performance_schema_metadata_lock_lost", "0", true, true},
    {"Performance_schema_meter_lost", "0", true, true},
    {"Performance_schema_metric_lost", "0", true, true},
    {"Performance_schema_mutex_classes_lost", "0", true, true},
    {"Performance_schema_mutex_instances_lost", "0", true, true},
    {"Performance_schema_nested_statement_lost", "0", true, true},
    {"Performance_schema_prepared_statements_lost", "0", true, true},
    {"Performance_schema_program_lost", "0", true, true},
    {"Performance_schema_rwlock_classes_lost", "0", true, true},
    {"Performance_schema_rwlock_instances_lost", "0", true, true},
    {"Performance_schema_session_connect_attrs_longest_seen", "0", true, true},
    {"Performance_schema_session_connect_attrs_lost", "0", true, true},
    {"Performance_schema_socket_classes_lost", "0", true, true},
    {"Performance_schema_socket_instances_lost", "0", true, true},
    {"Performance_schema_stage_classes_lost", "0", true, true},
    {"Performance_schema_statement_classes_lost", "0", true, true},
    {"Performance_schema_table_handles_lost", "0", true, true},
    {"Performance_schema_table_instances_lost", "0", true, true},
    {"Performance_schema_table_lock_stat_lost", "0", true, true},
    {"Performance_schema_thread_classes_lost", "0", true, true},
    {"Performance_schema_thread_instances_lost", "0", true, true},
    {"Performance_schema_users_lost", "0", true, true},
    {"Prepared_stmt_count", "0", true, true},
    {"Queries", "0", true, true},
    {"Questions", "0", true, true},
    {"Replica_open_temp_tables", "0", true, true},
    {"Resource_group_supported", "OFF", true, true},
    {"Rsa_public_key", "", true, true},
    {"Secondary_engine_execution_count", "0", true, true},
    {"Select_full_join", "0", true, true},
    {"Select_full_range_join", "0", true, true},
    {"Select_range", "0", true, true},
    {"Select_range_check", "0", true, true},
    {"Select_scan", "0", true, true},
    {"Slave_open_temp_tables", "0", true, true},
    {"Slow_launch_threads", "0", true, true},
    {"Slow_queries", "0", true, true},
    {"Sort_merge_passes", "0", true, true},
    {"Sort_range", "0", true, true},
    {"Sort_rows", "0", true, true},
    {"Sort_scan", "0", true, true},
    {"Ssl_accept_renegotiates", "0", true, true},
    {"Ssl_accepts", "0", true, true},
    {"Ssl_callback_cache_hits", "0", true, true},
    {"Ssl_cipher", "", true, true},
    {"Ssl_cipher_list", "", true, true},
    {"Ssl_client_connects", "0", true, true},
    {"Ssl_connect_renegotiates", "0", true, true},
    {"Ssl_ctx_verify_depth", "0", true, true},
    {"Ssl_ctx_verify_mode", "0", true, true},
    {"Ssl_default_timeout", "0", true, true},
    {"Ssl_finished_accepts", "0", true, true},
    {"Ssl_finished_connects", "0", true, true},
    {"Ssl_server_not_after", "", true, true},
    {"Ssl_server_not_before", "", true, true},
    {"Ssl_session_cache_hits", "0", true, true},
    {"Ssl_session_cache_misses", "0", true, true},
    {"Ssl_session_cache_mode", "", true, true},
    {"Ssl_session_cache_overflows", "0", true, true},
    {"Ssl_session_cache_size", "0", true, true},
    {"Ssl_session_cache_timeout", "0", true, true},
    {"Ssl_session_cache_timeouts", "0", true, true},
    {"Ssl_sessions_reused", "0", true, true},
    {"Ssl_used_session_cache_entries", "0", true, true},
    {"Ssl_verify_depth", "0", true, true},
    {"Ssl_verify_mode", "0", true, true},
    {"Ssl_version", "", true, true},
    {"Table_locks_immediate", "0", true, true},
    {"Table_locks_waited", "0", true, true},
    {"Table_open_cache_hits", "0", true, true},
    {"Table_open_cache_misses", "0", true, true},
    {"Table_open_cache_overflows", "0", true, true},
    {"Tc_log_max_pages_used", "0", true, true},
    {"Tc_log_page_size", "0", true, true},
    {"Tc_log_page_waits", "0", true, true},
    {"Telemetry_logs_supported", "OFF", true, true},
    {"Telemetry_metrics_supported", "OFF", true, true},
    {"Telemetry_traces_supported", "OFF", true, true},
    {"Threads_cached", "0", true, true},
    {"Threads_connected", "1", true, true},
    {"Threads_created", "1", true, true},
    {"Threads_running", "1", true, true},
    {"Tls_library_version", "", true, true},
    {"Tls_sni_server_name", "", true, false},
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ACTIVATE_ALL_ROLES_ON_LOGIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_ADDRESS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_TLS_CIPHERSUITES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_TLS_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTHENTICATION_POLICY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_GENERATE_CERTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOMATIC_SP_PRIVILEGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BACK_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BASEDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BIND_ADDRESS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_DIRECT_NON_TRANSACTIONAL_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ENCRYPTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ERROR_ACTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_EXPIRE_LOGS_AUTO_PURGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_EXPIRE_LOGS_SECONDS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GROUP_COMMIT_SYNC_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GROUP_COMMIT_SYNC_NO_DELAY_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GTID_SIMPLE_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_MAX_FLUSH_QUEUE_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ORDER_COMMITS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROTATE_ENCRYPTION_MASTER_KEY_AT_STARTUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_EVENT_MAX_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_IMAGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_METADATA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_VALUE_OPTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROWS_QUERY_LOG_EVENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_STMT_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_COMPRESSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_COMPRESSION_LEVEL_ZSTD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_DEPENDENCY_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BLOCK_ENCRYPTION_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BUILD_ID:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BULK_INSERT_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SETS_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHECK_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_AUTO_GENERATE_RSA_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_DIGEST_ROUNDS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_PRIVATE_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_PUBLIC_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_CONNECTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_CONNECTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_SERVER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_SERVER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COMPLETION_TYPE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONCURRENT_INSERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_FAILED_CONNECTIONS_THRESHOLD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_MAX_CONNECTION_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_MIN_CONNECTION_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_MEMORY_CHUNK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_MEMORY_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_DATABASE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_DATABASE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CORE_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CREATE_ADMIN_LISTENER_THREAD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CTE_MAX_RECURSION_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DATADIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_COLLATION_FOR_UTF8MB4:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_PASSWORD_LIFETIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TABLE_ENCRYPTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TMP_STORAGE_ENGINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_WEEK_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAY_KEY_WRITE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_QUEUE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DISCONNECT_ON_EXPIRED_PASSWORD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DISABLED_STORAGE_ENGINES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DIV_PRECISION_INCREMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_END_MARKERS_IN_JSON:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ENFORCE_GTID_CONSISTENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EQ_RANGE_INDEX_DIVE_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EVENT_SCHEDULER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLAIN_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLAIN_JSON_FORMAT_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FLUSH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FLUSH_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_BOOLEAN_SYNTAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_MAX_WORD_LEN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_MIN_WORD_LEN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_QUERY_EXPANSION_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_STOPWORD_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERATED_RANDOM_PASSWORD_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_REPLICATION_CONSISTENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_EXECUTED_COMPRESSION_PERIOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HISTOGRAM_GENERATION_MAX_MEM_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_CONNECT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_REPLICA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_SLAVE:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEEP_FILES_ON_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_EXECUTED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_OWNED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_PURGED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GLOBAL_CONNECTION_MEMORY_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GLOBAL_CONNECTION_MEMORY_TRACKING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_COMPRESS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_DYNAMIC_LOADING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_GEOMETRY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_PROFILING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_QUERY_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_RTREE_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_STATEMENT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_SYMLINK:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HOST_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INFORMATION_SCHEMA_STATS_EXPIRY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LICENSE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERAL_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERAL_LOG_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_BASENAME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_INDEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_SERVICES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_SUPPRESSION_LIST:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_VERBOSITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_OUTPUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_QUERIES_NOT_USING_INDEXES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_RAW:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_REPLICA_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLAVE_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_ADMIN_STATEMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_EXTRA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_REPLICA_STATEMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_SLAVE_STATEMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_STATEMENTS_UNSAFE_FOR_BINLOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_THROTTLE_QUERIES_NOT_USING_INDEXES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_TIMESTAMPS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCK_WAIT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LONG_QUERY_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOW_PRIORITY_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_FILE_SYSTEM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_TABLE_NAMES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ALLOWED_PACKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ERROR_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQL_NATIVE_PASSWORD_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_READ_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_RETRY_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_WRITE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_BIND_ADDRESS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_COMPRESSION_ALGORITHMS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_CONNECT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_DEFLATE_DEFAULT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_DEFLATE_MAX_CLIENT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_DOCUMENT_ID_UNIQUE_PREFIX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ENABLE_HELLO_NOTICE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_IDLE_WORKER_THREAD_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_INTERACTIVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_LZ4_DEFAULT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_LZ4_MAX_CLIENT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_MAX_ALLOWED_PACKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_MAX_CONNECTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_MIN_WORKER_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_PORT_OPEN_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_READ_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SOCKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_WAIT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_WRITE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ZSTD_DEFAULT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ZSTD_MAX_CLIENT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OLD_ALTER_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_HISTORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REQUIRE_CURRENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REUSE_INTERVAL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PID_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PLUGIN_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PRINT_IDENTIFIED_WITH_AS_HEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_SECURE_TRANSPORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_DISK_SYNC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SECURE_FILE_PRIV:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID_BITS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_UUID:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_GTIDS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_STATE_CHANGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_TRANSACTION_INFO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_AUTO_GENERATE_RSA_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PRIVATE_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PUBLIC_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_VERBOSITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_EXTERNAL_LOCKING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_NAME_RESOLVE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_NETWORKING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SHOW_DATABASE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SOCKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_FIPS_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_SESSION_CACHE_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_SESSION_CACHE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_QUERY_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_QUERY_LOG_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_LAUNCH_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SORT_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SUPER_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_HANDLING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_CERTIFICATES_ENFORCED_VALIDATION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_CIPHERSUITES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TMPDIR:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ACTIVATE_ALL_ROLES_ON_LOGIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_ADDRESS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_TLS_CIPHERSUITES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_TLS_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTHENTICATION_POLICY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_GENERATE_CERTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOMATIC_SP_PRIVILEGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BACK_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BASEDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BIND_ADDRESS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ENCRYPTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ERROR_ACTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_EXPIRE_LOGS_AUTO_PURGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_EXPIRE_LOGS_SECONDS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GROUP_COMMIT_SYNC_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GROUP_COMMIT_SYNC_NO_DELAY_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GTID_SIMPLE_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_MAX_FLUSH_QUEUE_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ORDER_COMMITS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROTATE_ENCRYPTION_MASTER_KEY_AT_STARTUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_EVENT_MAX_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_METADATA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_STMT_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_DEPENDENCY_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BUILD_ID:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_AUTO_GENERATE_RSA_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_DIGEST_ROUNDS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_PRIVATE_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_PUBLIC_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_SYSTEM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SETS_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHECK_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_FAILED_CONNECTIONS_THRESHOLD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_MAX_CONNECTION_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_MIN_CONNECTION_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONCURRENT_INSERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CORE_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CREATE_ADMIN_LISTENER_THREAD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DATADIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_PASSWORD_LIFETIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAY_KEY_WRITE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_QUEUE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DISCONNECT_ON_EXPIRED_PASSWORD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DISABLED_STORAGE_ENGINES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ENFORCE_GTID_CONSISTENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EVENT_SCHEDULER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FLUSH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FLUSH_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_BOOLEAN_SYNTAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_MAX_WORD_LEN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_MIN_WORD_LEN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_QUERY_EXPANSION_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_STOPWORD_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_EXECUTED_COMPRESSION_PERIOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_CONNECT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_REPLICA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_SLAVE:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GLOBAL_CONNECTION_MEMORY_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_COMPRESS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_DYNAMIC_LOADING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_GEOMETRY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_PROFILING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_QUERY_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_RTREE_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_STATEMENT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_SYMLINK:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HOST_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERAL_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERAL_LOG_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_BASENAME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_INDEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_SERVICES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_SUPPRESSION_LIST:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_VERBOSITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_OUTPUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_QUERIES_NOT_USING_INDEXES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_RAW:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_REPLICA_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLAVE_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_ADMIN_STATEMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_EXTRA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_REPLICA_STATEMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_SLAVE_STATEMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_STATEMENTS_UNSAFE_FOR_BINLOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_THROTTLE_QUERIES_NOT_USING_INDEXES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_TIMESTAMPS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_LAUNCH_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_FILE_SYSTEM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_TABLE_NAMES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQL_NATIVE_PASSWORD_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_BIND_ADDRESS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_COMPRESSION_ALGORITHMS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_CONNECT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_DEFLATE_DEFAULT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_DEFLATE_MAX_CLIENT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_DOCUMENT_ID_UNIQUE_PREFIX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ENABLE_HELLO_NOTICE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_IDLE_WORKER_THREAD_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_INTERACTIVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_LZ4_DEFAULT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_LZ4_MAX_CLIENT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_MAX_ALLOWED_PACKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_MAX_CONNECTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_MIN_WORKER_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_PORT_OPEN_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SOCKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ZSTD_DEFAULT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ZSTD_MAX_CLIENT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_HISTORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REQUIRE_CURRENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REUSE_INTERVAL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_SECURE_TRANSPORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SECURE_FILE_PRIV:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID_BITS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_UUID:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_AUTO_GENERATE_RSA_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PRIVATE_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PUBLIC_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_EXTERNAL_LOCKING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_NAME_RESOLVE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_NETWORKING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SHOW_DATABASE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_FIPS_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_SESSION_CACHE_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_SESSION_CACHE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_QUERY_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_QUERY_LOG_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SUPER_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_HANDLING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_CERTIFICATES_ENFORCED_VALIDATION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_CIPHERSUITES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TMPDIR:
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
    return (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_FORMAT ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_MAX_FLUSH_QUEUE_TIME ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_LIMIT ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_TIMEOUT ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_QUEUE_SIZE ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_SLAVE ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLAVE_UPDATES ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_SLAVE_STATEMENTS ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_STATEMENTS_UNSAFE_FOR_BINLOG) != 0;
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BUILD_ID:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_GENERATE_CERTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BASEDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CORE_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SETS_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DATADIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DISABLED_STORAGE_ENGINES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXTERNAL_USER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_MAX_WORD_LEN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_MIN_WORD_LEN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_QUERY_EXPANSION_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_STOPWORD_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HOSTNAME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LICENSE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PID_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PLUGIN_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SECURE_FILE_PRIV:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_EXTERNAL_LOCKING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_NAME_RESOLVE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_NETWORKING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SHOW_DATABASE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SOCKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_FIPS_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_HANDLING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_CERTIFICATES_ENFORCED_VALIDATION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TMPDIR:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_read_only_authentication_password(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_AUTO_GENERATE_RSA_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_DIGEST_ROUNDS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_PRIVATE_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_PUBLIC_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DISCONNECT_ON_EXPIRED_PASSWORD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_AUTO_GENERATE_RSA_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PRIVATE_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PUBLIC_KEY_PATH:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_read_only_server_capability(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_COMPRESS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_DYNAMIC_LOADING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_GEOMETRY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_PROFILING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_QUERY_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_RTREE_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_STATEMENT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_SYMLINK:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_read_only_admin_listener(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_ADDRESS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CREATE_ADMIN_LISTENER_THREAD:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_read_only_connection_listener(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BACK_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BIND_ADDRESS:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_read_only_binary_log(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GTID_SIMPLE_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROTATE_ENCRYPTION_MASTER_KEY_AT_STARTUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_EVENT_MAX_SIZE:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_read_only_mysqlx(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_BIND_ADDRESS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_PORT_OPEN_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SOCKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_KEY:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_read_only_server_logging(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_REPLICA_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLAVE_UPDATES:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_fixed_global_server_security(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_SECURE_TRANSPORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_SESSION_CACHE_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_SESSION_CACHE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_VERSION:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_fixed_global_authentication_password(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTHENTICATION_POLICY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_PASSWORD_LIFETIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQL_NATIVE_PASSWORD_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_HISTORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REQUIRE_CURRENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REUSE_INTERVAL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PROXY_USERS:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_fixed_global_admin_listener(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_TLS_CIPHERSUITES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_TLS_VERSION:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_fixed_global_mysqlx(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_COMPRESSION_ALGORITHMS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_CONNECT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_DEFLATE_DEFAULT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_DEFLATE_MAX_CLIENT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_DOCUMENT_ID_UNIQUE_PREFIX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ENABLE_HELLO_NOTICE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_IDLE_WORKER_THREAD_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_INTERACTIVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_LZ4_DEFAULT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_LZ4_MAX_CLIENT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_MAX_ALLOWED_PACKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_MAX_CONNECTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_MIN_WORKER_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ZSTD_DEFAULT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ZSTD_MAX_CLIENT_COMPRESSION_LEVEL:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_fixed_global_server_tls(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_CIPHERSUITES:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_fixed_global_server_logging(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERAL_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERAL_LOG_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_SERVICES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_SUPPRESSION_LIST:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_VERBOSITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_OUTPUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_QUERIES_NOT_USING_INDEXES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_RAW:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_ADMIN_STATEMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_EXTRA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_REPLICA_STATEMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_SLAVE_STATEMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_STATEMENTS_UNSAFE_FOR_BINLOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_THROTTLE_QUERIES_NOT_USING_INDEXES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_TIMESTAMPS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_QUERY_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_QUERY_LOG_FILE:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_fixed_global_connection_system(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HOST_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GLOBAL_CONNECTION_MEMORY_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_FAILED_CONNECTIONS_THRESHOLD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_MAX_CONNECTION_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_MIN_CONNECTION_DELAY:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_binary_log(enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_DIRECT_NON_TRANSACTIONAL_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ENCRYPTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ERROR_ACTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_EXPIRE_LOGS_AUTO_PURGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_EXPIRE_LOGS_SECONDS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GROUP_COMMIT_SYNC_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GROUP_COMMIT_SYNC_NO_DELAY_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GTID_SIMPLE_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_MAX_FLUSH_QUEUE_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ORDER_COMMITS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROTATE_ENCRYPTION_MASTER_KEY_AT_STARTUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_EVENT_MAX_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_IMAGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_METADATA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_VALUE_OPTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROWS_QUERY_LOG_EVENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_STMT_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_COMPRESSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_COMPRESSION_LEVEL_ZSTD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_DEPENDENCY_HISTORY_SIZE:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_bootstrap_placeholder(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ACTIVATE_ALL_ROLES_ON_LOGIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOMATIC_SP_PRIVILEGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BLOCK_ENCRYPTION_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BULK_INSERT_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHECK_PROXY_USERS:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_compatibility_placeholder(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COMPLETION_TYPE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONCURRENT_INSERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CTE_MAX_RECURSION_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TABLE_ENCRYPTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_WEEK_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAY_KEY_WRITE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_QUEUE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DIV_PRECISION_INCREMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ENFORCE_GTID_CONSISTENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EQ_RANGE_INDEX_DIVE_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EVENT_SCHEDULER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLAIN_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLAIN_JSON_FORMAT_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FLUSH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FLUSH_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_BOOLEAN_SYNTAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERATED_RANDOM_PASSWORD_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_REPLICATION_CONSISTENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_EXECUTED_COMPRESSION_PERIOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_NEXT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HISTOGRAM_GENERATION_MAX_MEM_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_IMMEDIATE_SERVER_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_CONNECT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_REPLICA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_SLAVE:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_timeout(enum mylite_execution_system_variable_kind kind) {
    return (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCK_WAIT_TIMEOUT ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECT_TIMEOUT ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_READ_TIMEOUT ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_RETRY_COUNT ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_WRITE_TIMEOUT) != 0;
}

bool mylite_execution_system_variable_is_connection_memory(
    enum mylite_execution_system_variable_kind kind
) {
    return (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_MEMORY_CHUNK_SIZE ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_MEMORY_LIMIT) != 0;
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ACTIVATE_ALL_ROLES_ON_LOGIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_GENERATE_CERTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOMATIC_SP_PRIVILEGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_AUTO_GENERATE_RSA_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CORE_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_DIRECT_NON_TRANSACTIONAL_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ENCRYPTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_EXPIRE_LOGS_AUTO_PURGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GTID_SIMPLE_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ORDER_COMMITS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROTATE_ENCRYPTION_MASTER_KEY_AT_STARTUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROWS_QUERY_LOG_EVENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_COMPRESSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHECK_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DISCONNECT_ON_EXPIRED_PASSWORD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_END_MARKERS_IN_JSON:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLICIT_DEFAULTS_FOR_TIMESTAMP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TABLE_ENCRYPTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FLUSH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GLOBAL_CONNECTION_MEMORY_TRACKING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEEP_FILES_ON_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ENABLE_HELLO_NOTICE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQL_NATIVE_PASSWORD_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OLD_ALTER_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REQUIRE_CURRENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PRINT_IDENTIFIED_WITH_AS_HEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_ROW_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_DISK_SYNC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_STATE_CHANGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_AUTO_GENERATE_RSA_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PROXY_USERS:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_PORT:
        return "33062";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_ADDRESS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_SSL_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_TLS_CIPHERSUITES:
        return "";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_TLS_VERSION:
        return "TLSv1.2,TLSv1.3";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTHENTICATION_POLICY:
        return "*,,";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_DIGEST_ROUNDS:
        return "5000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_PRIVATE_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PRIVATE_KEY_PATH:
        return "private_key.pem";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_PUBLIC_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PUBLIC_KEY_PATH:
        return "public_key.pem";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_COLLATION_FOR_UTF8MB4:
        return "utf8mb4_0900_ai_ci";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BLOCK_ENCRYPTION_MODE:
        return "aes-128-ecb";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BUILD_ID:
        return "66e221b3840955d27f740799b5b2c6eb0baf3283";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BULK_INSERT_BUFFER_SIZE:
        return "8388608";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SETS_DIR:
        return "/usr/share/mysql-8.4/charsets/";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COMPLETION_TYPE:
        return "NO_CHAIN";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONCURRENT_INSERT:
        return "AUTO";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CTE_MAX_RECURSION_DEPTH:
        return "1000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAY_KEY_WRITE:
        return "ON";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_LIMIT:
        return "100";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_TIMEOUT:
        return "300";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_QUEUE_SIZE:
        return "1000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DISABLED_STORAGE_ENGINES:
        return "";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DIV_PRECISION_INCREMENT:
        return "4";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ENFORCE_GTID_CONSISTENCY:
        return "OFF";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EQ_RANGE_INDEX_DIVE_LIMIT:
        return "200";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EVENT_SCHEDULER:
        return "ON";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLAIN_FORMAT:
        return "TRADITIONAL";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLAIN_JSON_FORMAT_VERSION:
        return "1";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXTERNAL_USER:
        return "";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FLUSH_TIME:
        return "0";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_BOOLEAN_SYNTAX:
        return "+ -><()~*:\"\"&|";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_MAX_WORD_LEN:
        return "84";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_MIN_WORD_LEN:
        return "4";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_QUERY_EXPANSION_LIMIT:
        return "20";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FT_STOPWORD_FILE:
        return "(built-in)";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERATED_RANDOM_PASSWORD_LENGTH:
        return "20";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_REPLICATION_CONSISTENCY:
        return "BEFORE_ON_PRIMARY_FAILOVER";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_EXECUTED_COMPRESSION_PERIOD:
        return "0";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GTID_NEXT:
        return "AUTOMATIC";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HISTOGRAM_GENERATION_MAX_MEM_SIZE:
        return "20000000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_IMMEDIATE_SERVER_VERSION:
        return "999999";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_CONNECT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_REPLICA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_SLAVE:
        return "";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_PASSWORD_LIFETIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CREATE_ADMIN_LISTENER_THREAD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CORE_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TABLE_ENCRYPTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_WEEK_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FLUSH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_HISTORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REUSE_INTERVAL:
        return "0";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TMP_STORAGE_ENGINE:
        return "InnoDB";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_END_MARKERS_IN_JSON:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ACTIVATE_ALL_ROLES_ON_LOGIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_DIRECT_NON_TRANSACTIONAL_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ENCRYPTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROTATE_ENCRYPTION_MASTER_KEY_AT_STARTUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROWS_QUERY_LOG_EVENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_COMPRESSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GLOBAL_CONNECTION_MEMORY_TRACKING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEEP_FILES_ON_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQL_NATIVE_PASSWORD_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_DOCUMENT_ID_UNIQUE_PREFIX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_PORT_OPEN_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OLD_ALTER_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REQUIRE_CURRENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PRINT_IDENTIFIED_WITH_AS_HEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_ROW_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_DISK_SYNC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_STATE_CHANGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_SKIP_SECONDARY_ENGINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_VERBOSITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHECK_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_OFF:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_WARNINGS:
        return "0";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLICIT_DEFAULTS_FOR_TIMESTAMP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_EXPIRE_LOGS_AUTO_PURGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GTID_SIMPLE_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ORDER_COMMITS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_GENERATE_CERTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOMATIC_SP_PRIVILEGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_AUTO_GENERATE_RSA_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DISCONNECT_ON_EXPIRED_PASSWORD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ENABLE_HELLO_NOTICE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_AUTO_GENERATE_RSA_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BIG_SELECTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_NOTES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_QUOTE_SHOW_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UNIQUE_CHECKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UPDATABLE_VIEWS_WITH_LIMIT:
        return "1";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_STMT_CACHE_SIZE:
        return "32768";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_CHECKSUM:
        return "CRC32";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ERROR_ACTION:
        return "ABORT_SERVER";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_EXPIRE_LOGS_SECONDS:
        return "2592000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_FORMAT:
        return "ROW";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GROUP_COMMIT_SYNC_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GROUP_COMMIT_SYNC_NO_DELAY_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_MAX_FLUSH_QUEUE_TIME:
        return "0";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_EVENT_MAX_SIZE:
        return "8192";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_IMAGE:
        return "FULL";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_METADATA:
        return "MINIMAL";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ROW_VALUE_OPTIONS:
        return "";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_COMPRESSION_LEVEL_ZSTD:
        return "3";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_TRANSACTION_DEPENDENCY_HISTORY_SIZE:
        return "25000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_COMPRESS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_DYNAMIC_LOADING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_GEOMETRY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_PROFILING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_RTREE_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_STATEMENT_TIMEOUT:
        return "YES";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RESULTSET_METADATA:
        return "FULL";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_GTIDS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_TRANSACTION_INFO:
        return "OFF";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SECURE_FILE_PRIV:
        return "/var/lib/mysql-files/";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_FIPS_MODE:
        return "OFF";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_SESSION_CACHE_TIMEOUT:
        return "300";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_HANDLING:
        return "one-thread-per-connection";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_VERSION:
        return "TLSv1.2,TLSv1.3";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TMPDIR:
        return "/tmp";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_USE_SECONDARY_ENGINE:
        return "ON";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_SECURE_TRANSPORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_NETWORKING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SHOW_DATABASE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_CERTIFICATES_ENFORCED_VALIDATION:
        return "0";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_QUERY_CACHE:
        return "NO";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HAVE_SYMLINK:
        return "DISABLED";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_BIND_ADDRESS:
        return "*";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_COMPRESSION_ALGORITHMS:
        return "DEFLATE_STREAM,LZ4_MESSAGE,ZSTD_STREAM";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_CONNECT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_READ_TIMEOUT:
        return "30";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_DEFLATE_DEFAULT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ZSTD_DEFAULT_COMPRESSION_LEVEL:
        return "3";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_DEFLATE_MAX_CLIENT_COMPRESSION_LEVEL:
        return "5";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_IDLE_WORKER_THREAD_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_WRITE_TIMEOUT:
        return "60";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_INTERACTIVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_WAIT_TIMEOUT:
        return "28800";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_LZ4_DEFAULT_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_MIN_WORKER_THREADS:
        return "2";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_LZ4_MAX_CLIENT_COMPRESSION_LEVEL:
        return "8";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_MAX_ALLOWED_PACKET:
        return "67108864";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_MAX_CONNECTIONS:
        return "100";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_PORT:
        return "33060";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SOCKET:
        return "/var/run/mysqld/mysqlx.sock";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_SSL_KEY:
        return "";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ZSTD_MAX_CLIENT_COMPRESSION_LEVEL:
        return "11";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_EXTERNAL_LOCKING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_NAME_RESOLVE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_SESSION_CACHE_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOCOMMIT:
        return "1";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CAPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CERT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CIPHER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CRL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_CRLPATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_CIPHERSUITES:
        return "";
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
