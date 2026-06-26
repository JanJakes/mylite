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
    {"insert_id", MYLITE_EXECUTION_SYSTEM_VARIABLE_INSERT_ID, true, false},
    {"innodb_adaptive_flushing",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING,
     true,
     true},
    {"innodb_adaptive_flushing_lwm",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING_LWM,
     true,
     true},
    {"innodb_adaptive_hash_index",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_HASH_INDEX,
     true,
     true},
    {"innodb_adaptive_hash_index_parts",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_HASH_INDEX_PARTS,
     true,
     true},
    {"innodb_adaptive_max_sleep_delay",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_MAX_SLEEP_DELAY,
     true,
     true},
    {"innodb_autoextend_increment",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_AUTOEXTEND_INCREMENT,
     true,
     true},
    {"innodb_autoinc_lock_mode",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_AUTOINC_LOCK_MODE,
     true,
     true},
    {"innodb_buffer_pool_chunk_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_CHUNK_SIZE,
     true,
     true},
    {"innodb_buffer_pool_dump_at_shutdown",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_AT_SHUTDOWN,
     true,
     true},
    {"innodb_buffer_pool_dump_now",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_NOW,
     true,
     true},
    {"innodb_buffer_pool_dump_pct",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_PCT,
     true,
     true},
    {"innodb_buffer_pool_filename",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_FILENAME,
     true,
     true},
    {"innodb_buffer_pool_in_core_file",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_IN_CORE_FILE,
     true,
     true},
    {"innodb_buffer_pool_instances",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_INSTANCES,
     true,
     true},
    {"innodb_buffer_pool_load_abort",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_ABORT,
     true,
     true},
    {"innodb_buffer_pool_load_at_startup",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_AT_STARTUP,
     true,
     true},
    {"innodb_buffer_pool_load_now",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_NOW,
     true,
     true},
    {"innodb_buffer_pool_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_SIZE, true, true
    },
    {"innodb_change_buffer_max_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFER_MAX_SIZE,
     true,
     true},
    {"innodb_change_buffering", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFERING, true, true
    },
    {"innodb_checksum_algorithm",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHECKSUM_ALGORITHM,
     true,
     true},
    {"innodb_cmp_per_index_enabled",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CMP_PER_INDEX_ENABLED,
     true,
     true},
    {"innodb_commit_concurrency",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMMIT_CONCURRENCY,
     true,
     true},
    {"innodb_compression_failure_threshold_pct",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_FAILURE_THRESHOLD_PCT,
     true,
     true},
    {"innodb_compression_level",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_LEVEL,
     true,
     true},
    {"innodb_compression_pad_pct_max",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_PAD_PCT_MAX,
     true,
     true},
    {"innodb_concurrency_tickets",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CONCURRENCY_TICKETS,
     true,
     true},
    {"innodb_data_file_path", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DATA_FILE_PATH, true, true},
    {"innodb_data_home_dir", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DATA_HOME_DIR, true, true},
    {"innodb_ddl_buffer_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_BUFFER_SIZE, true, true},
    {"innodb_ddl_threads", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_THREADS, true, true},
    {"innodb_deadlock_detect", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEADLOCK_DETECT, true, true},
    {"innodb_dedicated_server", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEDICATED_SERVER, true, true
    },
    {"innodb_default_row_format",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEFAULT_ROW_FORMAT,
     true,
     true},
    {"innodb_directories", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DIRECTORIES, true, true},
    {"innodb_disable_sort_file_cache",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DISABLE_SORT_FILE_CACHE,
     true,
     true},
    {"innodb_doublewrite", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE, true, true},
    {"innodb_doublewrite_batch_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_BATCH_SIZE,
     true,
     true},
    {"innodb_doublewrite_dir", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_DIR, true, true},
    {"innodb_doublewrite_files",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_FILES,
     true,
     true},
    {"innodb_doublewrite_pages",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_PAGES,
     true,
     true},
    {"innodb_extend_and_initialize",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_EXTEND_AND_INITIALIZE,
     true,
     true},
    {"innodb_fast_shutdown", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FAST_SHUTDOWN, true, true},
    {"innodb_file_per_table", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILE_PER_TABLE, true, true},
    {"innodb_fill_factor", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILL_FACTOR, true, true},
    {"innodb_flush_log_at_timeout",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TIMEOUT,
     true,
     true},
    {"innodb_flush_log_at_trx_commit",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TRX_COMMIT,
     true,
     true},
    {"innodb_flush_method", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_METHOD, true, true},
    {"innodb_flush_neighbors", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_NEIGHBORS, true, true},
    {"innodb_flush_sync", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_SYNC, true, true},
    {"innodb_flushing_avg_loops",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSHING_AVG_LOOPS,
     true,
     true},
    {"innodb_force_load_corrupted",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FORCE_LOAD_CORRUPTED,
     true,
     true},
    {"innodb_force_recovery", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FORCE_RECOVERY, true, true},
    {"innodb_fsync_threshold", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FSYNC_THRESHOLD, true, true},
    {"innodb_ft_aux_table", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_AUX_TABLE, true, true},
    {"innodb_ft_cache_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_CACHE_SIZE, true, true},
    {"innodb_ft_enable_diag_print",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_DIAG_PRINT,
     true,
     true},
    {"innodb_ft_enable_stopword",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_STOPWORD,
     true,
     true},
    {"innodb_ft_max_token_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_MAX_TOKEN_SIZE,
     true,
     true},
    {"innodb_ft_min_token_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_MIN_TOKEN_SIZE,
     true,
     true},
    {"innodb_ft_num_word_optimize",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_NUM_WORD_OPTIMIZE,
     true,
     true},
    {"innodb_ft_result_cache_limit",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_RESULT_CACHE_LIMIT,
     true,
     true},
    {"innodb_ft_server_stopword_table",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_SERVER_STOPWORD_TABLE,
     true,
     true},
    {"innodb_ft_sort_pll_degree",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_SORT_PLL_DEGREE,
     true,
     true},
    {"innodb_ft_total_cache_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_TOTAL_CACHE_SIZE,
     true,
     true},
    {"innodb_ft_user_stopword_table",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_USER_STOPWORD_TABLE,
     true,
     true},
    {"innodb_idle_flush_pct", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IDLE_FLUSH_PCT, true, true},
    {"innodb_io_capacity", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY, true, true},
    {"innodb_io_capacity_max", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY_MAX, true, true},
    {"innodb_lock_wait_timeout",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOCK_WAIT_TIMEOUT,
     true,
     true},
    {"innodb_log_buffer_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_BUFFER_SIZE, true, true},
    {"innodb_log_checksums", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_CHECKSUMS, true, true},
    {"innodb_log_compressed_pages",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_COMPRESSED_PAGES,
     true,
     true},
    {"innodb_log_file_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_FILE_SIZE, true, true},
    {"innodb_log_files_in_group",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_FILES_IN_GROUP,
     true,
     true},
    {"innodb_log_group_home_dir",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_GROUP_HOME_DIR,
     true,
     true},
    {"innodb_log_spin_cpu_abs_lwm",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_ABS_LWM,
     true,
     true},
    {"innodb_log_spin_cpu_pct_hwm",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_PCT_HWM,
     true,
     true},
    {"innodb_log_wait_for_flush_spin_hwm",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WAIT_FOR_FLUSH_SPIN_HWM,
     true,
     true},
    {"innodb_log_write_ahead_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITE_AHEAD_SIZE,
     true,
     true},
    {"innodb_log_writer_threads",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITER_THREADS,
     true,
     true},
    {"innodb_lru_scan_depth", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LRU_SCAN_DEPTH, true, true},
    {"innodb_max_dirty_pages_pct",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT,
     true,
     true},
    {"innodb_max_dirty_pages_pct_lwm",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT_LWM,
     true,
     true},
    {"innodb_max_purge_lag", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG, true, true},
    {"innodb_max_purge_lag_delay",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG_DELAY,
     true,
     true},
    {"innodb_max_undo_log_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_UNDO_LOG_SIZE,
     true,
     true},
    {"innodb_monitor_disable", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_DISABLE, true, true},
    {"innodb_monitor_enable", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_ENABLE, true, true},
    {"innodb_monitor_reset", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_RESET, true, true},
    {"innodb_monitor_reset_all",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_RESET_ALL,
     true,
     true},
    {"innodb_old_blocks_pct", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OLD_BLOCKS_PCT, true, true},
    {"innodb_old_blocks_time", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OLD_BLOCKS_TIME, true, true},
    {"innodb_online_alter_log_max_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ONLINE_ALTER_LOG_MAX_SIZE,
     true,
     true},
    {"innodb_open_files", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OPEN_FILES, true, true},
    {"innodb_optimize_fulltext_only",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OPTIMIZE_FULLTEXT_ONLY,
     true,
     true},
    {"innodb_page_cleaners", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PAGE_CLEANERS, true, true},
    {"innodb_page_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PAGE_SIZE, true, true},
    {"innodb_parallel_read_threads",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PARALLEL_READ_THREADS,
     true,
     true},
    {"innodb_print_all_deadlocks",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PRINT_ALL_DEADLOCKS,
     true,
     true},
    {"innodb_print_ddl_logs", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PRINT_DDL_LOGS, true, true},
    {"innodb_purge_batch_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_BATCH_SIZE, true, true
    },
    {"innodb_purge_rseg_truncate_frequency",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_RSEG_TRUNCATE_FREQUENCY,
     true,
     true},
    {"innodb_purge_threads", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_THREADS, true, true},
    {"innodb_random_read_ahead",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_RANDOM_READ_AHEAD,
     true,
     true},
    {"innodb_read_ahead_threshold",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_AHEAD_THRESHOLD,
     true,
     true},
    {"innodb_read_io_threads", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_IO_THREADS, true, true},
    {"innodb_read_only", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_ONLY, true, true},
    {"innodb_redo_log_archive_dirs",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_ARCHIVE_DIRS,
     true,
     true},
    {"innodb_redo_log_capacity",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_CAPACITY,
     true,
     true},
    {"innodb_redo_log_encrypt", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_ENCRYPT, true, true
    },
    {"innodb_replication_delay",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REPLICATION_DELAY,
     true,
     true},
    {"innodb_rollback_on_timeout",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ROLLBACK_ON_TIMEOUT,
     true,
     true},
    {"innodb_rollback_segments",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ROLLBACK_SEGMENTS,
     true,
     true},
    {"innodb_segment_reserve_factor",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SEGMENT_RESERVE_FACTOR,
     true,
     true},
    {"innodb_sort_buffer_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SORT_BUFFER_SIZE, true, true
    },
    {"innodb_spin_wait_delay", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SPIN_WAIT_DELAY, true, true},
    {"innodb_spin_wait_pause_multiplier",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SPIN_WAIT_PAUSE_MULTIPLIER,
     true,
     true},
    {"innodb_stats_auto_recalc",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_AUTO_RECALC,
     true,
     true},
    {"innodb_stats_include_delete_marked",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_INCLUDE_DELETE_MARKED,
     true,
     true},
    {"innodb_stats_method", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_METHOD, true, true},
    {"innodb_stats_on_metadata",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_ON_METADATA,
     true,
     true},
    {"innodb_stats_persistent", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_PERSISTENT, true, true
    },
    {"innodb_stats_persistent_sample_pages",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_PERSISTENT_SAMPLE_PAGES,
     true,
     true},
    {"innodb_stats_transient_sample_pages",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_TRANSIENT_SAMPLE_PAGES,
     true,
     true},
    {"innodb_status_output", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATUS_OUTPUT, true, true},
    {"innodb_status_output_locks",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATUS_OUTPUT_LOCKS,
     true,
     true},
    {"innodb_strict_mode", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STRICT_MODE, true, true},
    {"innodb_sync_array_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SYNC_ARRAY_SIZE, true, true},
    {"innodb_sync_spin_loops", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SYNC_SPIN_LOOPS, true, true},
    {"innodb_table_locks", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TABLE_LOCKS, true, true},
    {"innodb_temp_data_file_path",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TEMP_DATA_FILE_PATH,
     true,
     true},
    {"innodb_temp_tablespaces_dir",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TEMP_TABLESPACES_DIR,
     true,
     true},
    {"innodb_thread_concurrency",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_THREAD_CONCURRENCY,
     true,
     true},
    {"innodb_thread_sleep_delay",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_THREAD_SLEEP_DELAY,
     true,
     true},
    {"innodb_tmpdir", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TMPDIR, true, true},
    {"innodb_undo_directory", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_DIRECTORY, true, true},
    {"innodb_undo_log_encrypt", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_LOG_ENCRYPT, true, true
    },
    {"innodb_undo_log_truncate",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_LOG_TRUNCATE,
     true,
     true},
    {"innodb_undo_tablespaces", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_TABLESPACES, true, true
    },
    {"innodb_use_fdatasync", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_USE_FDATASYNC, true, true},
    {"innodb_use_native_aio", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_USE_NATIVE_AIO, true, true},
    {"innodb_validate_tablespace_paths",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_VALIDATE_TABLESPACE_PATHS,
     true,
     true},
    {"innodb_version", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_VERSION, true, true},
    {"innodb_write_io_threads", MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_WRITE_IO_THREADS, true, true
    },
    {"internal_tmp_mem_storage_engine",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERNAL_TMP_MEM_STORAGE_ENGINE,
     true,
     true},
    {"interactive_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT, true, true},
    {"join_buffer_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_JOIN_BUFFER_SIZE, true, true},
    {"keep_files_on_create", MYLITE_EXECUTION_SYSTEM_VARIABLE_KEEP_FILES_ON_CREATE, true, true},
    {"key_buffer_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_BUFFER_SIZE, true, true},
    {"key_cache_age_threshold", MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_AGE_THRESHOLD, true, true
    },
    {"key_cache_block_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_BLOCK_SIZE, true, true},
    {"key_cache_division_limit",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_DIVISION_LIMIT,
     true,
     true},
    {"keyring_operations", MYLITE_EXECUTION_SYSTEM_VARIABLE_KEYRING_OPERATIONS, true, true},
    {"large_files_support", MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_FILES_SUPPORT, true, true},
    {"large_page_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_PAGE_SIZE, true, true},
    {"large_pages", MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_PAGES, true, true},
    {"last_insert_id", MYLITE_EXECUTION_SYSTEM_VARIABLE_LAST_INSERT_ID, true, false},
    {"lc_messages", MYLITE_EXECUTION_SYSTEM_VARIABLE_LC_MESSAGES, true, true},
    {"lc_messages_dir", MYLITE_EXECUTION_SYSTEM_VARIABLE_LC_MESSAGES_DIR, true, true},
    {"lc_time_names", MYLITE_EXECUTION_SYSTEM_VARIABLE_LC_TIME_NAMES, true, true},
    {"license", MYLITE_EXECUTION_SYSTEM_VARIABLE_LICENSE, true, true},
    {"local_infile", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCAL_INFILE, true, true},
    {"locked_in_memory", MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCKED_IN_MEMORY, true, true},
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
    {"mandatory_roles", MYLITE_EXECUTION_SYSTEM_VARIABLE_MANDATORY_ROLES, true, true},
    {"master_verify_checksum", MYLITE_EXECUTION_SYSTEM_VARIABLE_MASTER_VERIFY_CHECKSUM, true, true},
    {"max_allowed_packet", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ALLOWED_PACKET, true, true},
    {"max_binlog_cache_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_BINLOG_CACHE_SIZE, true, true},
    {"max_binlog_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_BINLOG_SIZE, true, true},
    {"max_binlog_stmt_cache_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_BINLOG_STMT_CACHE_SIZE,
     true,
     true},
    {"max_connect_errors", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_CONNECT_ERRORS, true, true},
    {"max_connections", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_CONNECTIONS, true, true},
    {"max_delayed_threads", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_DELAYED_THREADS, true, true},
    {"max_digest_length", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_DIGEST_LENGTH, true, true},
    {"max_error_count", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ERROR_COUNT, true, true},
    {"max_execution_time", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_EXECUTION_TIME, true, true},
    {"max_heap_table_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_HEAP_TABLE_SIZE, true, true},
    {"max_insert_delayed_threads",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_INSERT_DELAYED_THREADS,
     true,
     true},
    {"max_join_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_JOIN_SIZE, true, true},
    {"max_length_for_sort_data",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_LENGTH_FOR_SORT_DATA,
     true,
     true},
    {"max_points_in_geometry", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_POINTS_IN_GEOMETRY, true, true},
    {"max_prepared_stmt_count", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_PREPARED_STMT_COUNT, true, true
    },
    {"max_relay_log_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_RELAY_LOG_SIZE, true, true},
    {"max_seeks_for_key", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SEEKS_FOR_KEY, true, true},
    {"max_sort_length", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SORT_LENGTH, true, true},
    {"max_sp_recursion_depth", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SP_RECURSION_DEPTH, true, true},
    {"max_user_connections", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_USER_CONNECTIONS, true, true},
    {"max_write_lock_count", MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_WRITE_LOCK_COUNT, true, true},
    {"min_examined_row_limit", MYLITE_EXECUTION_SYSTEM_VARIABLE_MIN_EXAMINED_ROW_LIMIT, true, true},
    {"myisam_data_pointer_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_DATA_POINTER_SIZE,
     true,
     true},
    {"myisam_max_sort_file_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_MAX_SORT_FILE_SIZE,
     true,
     true},
    {"myisam_mmap_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_MMAP_SIZE, true, true},
    {"myisam_recover_options", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_RECOVER_OPTIONS, true, true},
    {"myisam_sort_buffer_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_SORT_BUFFER_SIZE, true, true
    },
    {"myisam_stats_method", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_STATS_METHOD, true, true},
    {"myisam_use_mmap", MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_USE_MMAP, true, true},
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
    {"net_buffer_length", MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_BUFFER_LENGTH, true, true},
    {"net_read_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_READ_TIMEOUT, true, true},
    {"net_retry_count", MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_RETRY_COUNT, true, true},
    {"net_write_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_WRITE_TIMEOUT, true, true},
    {"ngram_token_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_NGRAM_TOKEN_SIZE, true, true},
    {"offline_mode", MYLITE_EXECUTION_SYSTEM_VARIABLE_OFFLINE_MODE, true, true},
    {"old_alter_table", MYLITE_EXECUTION_SYSTEM_VARIABLE_OLD_ALTER_TABLE, true, true},
    {"open_files_limit", MYLITE_EXECUTION_SYSTEM_VARIABLE_OPEN_FILES_LIMIT, true, true},
    {"optimizer_prune_level", MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_PRUNE_LEVEL, true, true},
    {"optimizer_search_depth", MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_SEARCH_DEPTH, true, true},
    {"optimizer_switch", MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_SWITCH, true, true},
    {"optimizer_trace", MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE, true, true},
    {"optimizer_trace_features",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_FEATURES,
     true,
     true},
    {"optimizer_trace_limit", MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_LIMIT, true, true},
    {"optimizer_trace_max_mem_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_MAX_MEM_SIZE,
     true,
     true},
    {"optimizer_trace_offset", MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_OFFSET, true, true},
    {"original_commit_timestamp",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_ORIGINAL_COMMIT_TIMESTAMP,
     true,
     false},
    {"original_server_version",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_ORIGINAL_SERVER_VERSION,
     true,
     false},
    {"parser_max_mem_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_PARSER_MAX_MEM_SIZE, true, true},
    {"partial_revokes", MYLITE_EXECUTION_SYSTEM_VARIABLE_PARTIAL_REVOKES, true, true},
    {"password_history", MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_HISTORY, true, true},
    {"password_require_current",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REQUIRE_CURRENT,
     true,
     true},
    {"password_reuse_interval", MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REUSE_INTERVAL, true, true
    },
    {"performance_schema", MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA, true, true},
    {"performance_schema_accounts_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_ACCOUNTS_SIZE,
     true,
     true},
    {"performance_schema_digests_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_DIGESTS_SIZE,
     true,
     true},
    {"performance_schema_error_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_ERROR_SIZE,
     true,
     true},
    {"performance_schema_events_stages_history_long_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_HISTORY_LONG_SIZE,
     true,
     true},
    {"performance_schema_events_stages_history_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_HISTORY_SIZE,
     true,
     true},
    {"performance_schema_events_statements_history_long_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STATEMENTS_HISTORY_LONG_SIZE,
     true,
     true},
    {"performance_schema_events_statements_history_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STATEMENTS_HISTORY_SIZE,
     true,
     true},
    {"performance_schema_events_transactions_history_long_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_TRANSACTIONS_HISTORY_LONG_SIZE,
     true,
     true},
    {"performance_schema_events_transactions_history_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_TRANSACTIONS_HISTORY_SIZE,
     true,
     true},
    {"performance_schema_events_waits_history_long_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_HISTORY_LONG_SIZE,
     true,
     true},
    {"performance_schema_events_waits_history_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_HISTORY_SIZE,
     true,
     true},
    {"performance_schema_hosts_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_HOSTS_SIZE,
     true,
     true},
    {"performance_schema_max_cond_classes",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_COND_CLASSES,
     true,
     true},
    {"performance_schema_max_cond_instances",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_COND_INSTANCES,
     true,
     true},
    {"performance_schema_max_digest_length",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_DIGEST_LENGTH,
     true,
     true},
    {"performance_schema_max_digest_sample_age",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_DIGEST_SAMPLE_AGE,
     true,
     true},
    {"performance_schema_max_file_classes",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_CLASSES,
     true,
     true},
    {"performance_schema_max_file_handles",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_HANDLES,
     true,
     true},
    {"performance_schema_max_file_instances",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_INSTANCES,
     true,
     true},
    {"performance_schema_max_index_stat",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_INDEX_STAT,
     true,
     true},
    {"performance_schema_max_memory_classes",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MEMORY_CLASSES,
     true,
     true},
    {"performance_schema_max_metadata_locks",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METADATA_LOCKS,
     true,
     true},
    {"performance_schema_max_meter_classes",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METER_CLASSES,
     true,
     true},
    {"performance_schema_max_metric_classes",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METRIC_CLASSES,
     true,
     true},
    {"performance_schema_max_mutex_classes",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MUTEX_CLASSES,
     true,
     true},
    {"performance_schema_max_mutex_instances",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MUTEX_INSTANCES,
     true,
     true},
    {"performance_schema_max_prepared_statements_instances",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_PREPARED_STATEMENTS_INSTANCES,
     true,
     true},
    {"performance_schema_max_program_instances",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_PROGRAM_INSTANCES,
     true,
     true},
    {"performance_schema_max_rwlock_classes",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_RWLOCK_CLASSES,
     true,
     true},
    {"performance_schema_max_rwlock_instances",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_RWLOCK_INSTANCES,
     true,
     true},
    {"performance_schema_max_socket_classes",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SOCKET_CLASSES,
     true,
     true},
    {"performance_schema_max_socket_instances",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SOCKET_INSTANCES,
     true,
     true},
    {"performance_schema_max_sql_text_length",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SQL_TEXT_LENGTH,
     true,
     true},
    {"performance_schema_max_stage_classes",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STAGE_CLASSES,
     true,
     true},
    {"performance_schema_max_statement_classes",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STATEMENT_CLASSES,
     true,
     true},
    {"performance_schema_max_statement_stack",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STATEMENT_STACK,
     true,
     true},
    {"performance_schema_max_table_handles",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_HANDLES,
     true,
     true},
    {"performance_schema_max_table_instances",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_INSTANCES,
     true,
     true},
    {"performance_schema_max_table_lock_stat",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_LOCK_STAT,
     true,
     true},
    {"performance_schema_max_thread_classes",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_THREAD_CLASSES,
     true,
     true},
    {"performance_schema_max_thread_instances",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_THREAD_INSTANCES,
     true,
     true},
    {"performance_schema_session_connect_attrs_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SESSION_CONNECT_ATTRS_SIZE,
     true,
     true},
    {"performance_schema_setup_actors_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SETUP_ACTORS_SIZE,
     true,
     true},
    {"performance_schema_setup_objects_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SETUP_OBJECTS_SIZE,
     true,
     true},
    {"performance_schema_show_processlist",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SHOW_PROCESSLIST,
     true,
     true},
    {"performance_schema_users_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_USERS_SIZE,
     true,
     true},
    {"persist_only_admin_x509_subject",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSIST_ONLY_ADMIN_X509_SUBJECT,
     true,
     true},
    {"persist_sensitive_variables_in_plaintext",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSIST_SENSITIVE_VARIABLES_IN_PLAINTEXT,
     true,
     true},
    {"persisted_globals_load", MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSISTED_GLOBALS_LOAD, true, true},
    {"pid_file", MYLITE_EXECUTION_SYSTEM_VARIABLE_PID_FILE, true, true},
    {"plugin_dir", MYLITE_EXECUTION_SYSTEM_VARIABLE_PLUGIN_DIR, true, true},
    {"port", MYLITE_EXECUTION_SYSTEM_VARIABLE_PORT, true, true},
    {"preload_buffer_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_PRELOAD_BUFFER_SIZE, true, true},
    {"print_identified_with_as_hex",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PRINT_IDENTIFIED_WITH_AS_HEX,
     true,
     true},
    {"profiling", MYLITE_EXECUTION_SYSTEM_VARIABLE_PROFILING, true, true},
    {"profiling_history_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_PROFILING_HISTORY_SIZE, true, true},
    {"protocol_compression_algorithms",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_COMPRESSION_ALGORITHMS,
     true,
     true},
    {"protocol_version", MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_VERSION, true, true},
    {"proxy_user", MYLITE_EXECUTION_SYSTEM_VARIABLE_PROXY_USER, true, false},
    {"pseudo_replica_mode", MYLITE_EXECUTION_SYSTEM_VARIABLE_PSEUDO_REPLICA_MODE, true, false},
    {"pseudo_slave_mode", MYLITE_EXECUTION_SYSTEM_VARIABLE_PSEUDO_SLAVE_MODE, true, false},
    {"pseudo_thread_id", MYLITE_EXECUTION_SYSTEM_VARIABLE_PSEUDO_THREAD_ID, true, false},
    {"query_alloc_block_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_QUERY_ALLOC_BLOCK_SIZE, true, true},
    {"query_prealloc_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_QUERY_PREALLOC_SIZE, true, true},
    {"range_alloc_block_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_RANGE_ALLOC_BLOCK_SIZE, true, true},
    {"range_optimizer_max_mem_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_RANGE_OPTIMIZER_MAX_MEM_SIZE,
     true,
     true},
    {"rand_seed1", MYLITE_EXECUTION_SYSTEM_VARIABLE_RAND_SEED1, true, false},
    {"rand_seed2", MYLITE_EXECUTION_SYSTEM_VARIABLE_RAND_SEED2, true, false},
    {"rbr_exec_mode", MYLITE_EXECUTION_SYSTEM_VARIABLE_RBR_EXEC_MODE, true, true},
    {"read_buffer_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_BUFFER_SIZE, true, true},
    {"read_only", MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_ONLY, true, true},
    {"read_rnd_buffer_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_RND_BUFFER_SIZE, true, true},
    {"relay_log", MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG, true, true},
    {"relay_log_basename", MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_BASENAME, true, true},
    {"relay_log_index", MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_INDEX, true, true},
    {"relay_log_purge", MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_PURGE, true, true},
    {"relay_log_recovery", MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_RECOVERY, true, true},
    {"relay_log_space_limit", MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_SPACE_LIMIT, true, true},
    {"regexp_stack_limit", MYLITE_EXECUTION_SYSTEM_VARIABLE_REGEXP_STACK_LIMIT, true, true},
    {"regexp_time_limit", MYLITE_EXECUTION_SYSTEM_VARIABLE_REGEXP_TIME_LIMIT, true, true},
    {"replication_optimize_for_static_plugin_config",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICATION_OPTIMIZE_FOR_STATIC_PLUGIN_CONFIG,
     true,
     true},
    {"replication_sender_observe_commit_only",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICATION_SENDER_OBSERVE_COMMIT_ONLY,
     true,
     true},
    {"replica_allow_batching", MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_ALLOW_BATCHING, true, true},
    {"replica_checkpoint_group",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_CHECKPOINT_GROUP,
     true,
     true},
    {"replica_checkpoint_period",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_CHECKPOINT_PERIOD,
     true,
     true},
    {"replica_compressed_protocol",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_COMPRESSED_PROTOCOL,
     true,
     true},
    {"replica_exec_mode", MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_EXEC_MODE, true, true},
    {"replica_load_tmpdir", MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_LOAD_TMPDIR, true, true},
    {"replica_max_allowed_packet",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_MAX_ALLOWED_PACKET,
     true,
     true},
    {"replica_net_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_NET_TIMEOUT, true, true},
    {"replica_parallel_type", MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PARALLEL_TYPE, true, true},
    {"replica_parallel_workers",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PARALLEL_WORKERS,
     true,
     true},
    {"replica_pending_jobs_size_max",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PENDING_JOBS_SIZE_MAX,
     true,
     true},
    {"replica_preserve_commit_order",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PRESERVE_COMMIT_ORDER,
     true,
     true},
    {"replica_skip_errors", MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_SKIP_ERRORS, true, true},
    {"replica_sql_verify_checksum",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_SQL_VERIFY_CHECKSUM,
     true,
     true},
    {"replica_transaction_retries",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_TRANSACTION_RETRIES,
     true,
     true},
    {"replica_type_conversions",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_TYPE_CONVERSIONS,
     true,
     true},
    {"report_host", MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_HOST, true, true},
    {"report_password", MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_PASSWORD, true, true},
    {"report_port", MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_PORT, true, true},
    {"report_user", MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_USER, true, true},
    {"require_row_format", MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_ROW_FORMAT, true, false},
    {"require_secure_transport",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_SECURE_TRANSPORT,
     true,
     true},
    {"restrict_fk_on_non_standard_key",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_RESTRICT_FK_ON_NON_STANDARD_KEY,
     true,
     true},
    {"resultset_metadata", MYLITE_EXECUTION_SYSTEM_VARIABLE_RESULTSET_METADATA, true, false},
    {"rpl_read_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_READ_SIZE, true, true},
    {"rpl_stop_replica_timeout",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_STOP_REPLICA_TIMEOUT,
     true,
     true},
    {"rpl_stop_slave_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_STOP_SLAVE_TIMEOUT, true, true},
    {"schema_definition_cache", MYLITE_EXECUTION_SYSTEM_VARIABLE_SCHEMA_DEFINITION_CACHE, true, true
    },
    {"secondary_engine_cost_threshold",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SECONDARY_ENGINE_COST_THRESHOLD,
     true,
     true},
    {"secure_file_priv", MYLITE_EXECUTION_SYSTEM_VARIABLE_SECURE_FILE_PRIV, true, true},
    {"select_into_buffer_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_BUFFER_SIZE, true, true
    },
    {"select_into_disk_sync", MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_DISK_SYNC, true, true},
    {"select_into_disk_sync_delay",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_DISK_SYNC_DELAY,
     true,
     true},
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
    {"session_track_system_variables",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_SYSTEM_VARIABLES,
     true,
     true},
    {"session_track_transaction_info",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_TRANSACTION_INFO,
     true,
     true},
    {"set_operations_buffer_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SET_OPERATIONS_BUFFER_SIZE,
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
    {"show_gipk_in_create_table_and_information_schema",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_GIPK_IN_CREATE_TABLE_AND_INFORMATION_SCHEMA,
     true,
     true},
    {"skip_external_locking", MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_EXTERNAL_LOCKING, true, true},
    {"skip_name_resolve", MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_NAME_RESOLVE, true, true},
    {"skip_networking", MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_NETWORKING, true, true},
    {"skip_replica_start", MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_REPLICA_START, true, true},
    {"skip_show_database", MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SHOW_DATABASE, true, true},
    {"skip_slave_start", MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SLAVE_START, true, true},
    {"slave_allow_batching", MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_ALLOW_BATCHING, true, true},
    {"slave_checkpoint_group", MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_CHECKPOINT_GROUP, true, true},
    {"slave_checkpoint_period", MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_CHECKPOINT_PERIOD, true, true
    },
    {"slave_compressed_protocol",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_COMPRESSED_PROTOCOL,
     true,
     true},
    {"slave_exec_mode", MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_EXEC_MODE, true, true},
    {"slave_load_tmpdir", MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_LOAD_TMPDIR, true, true},
    {"slave_max_allowed_packet",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_MAX_ALLOWED_PACKET,
     true,
     true},
    {"slave_net_timeout", MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_NET_TIMEOUT, true, true},
    {"slave_parallel_type", MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PARALLEL_TYPE, true, true},
    {"slave_parallel_workers", MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PARALLEL_WORKERS, true, true},
    {"slave_pending_jobs_size_max",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PENDING_JOBS_SIZE_MAX,
     true,
     true},
    {"slave_preserve_commit_order",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PRESERVE_COMMIT_ORDER,
     true,
     true},
    {"slave_skip_errors", MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SKIP_ERRORS, true, true},
    {"slave_sql_verify_checksum",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SQL_VERIFY_CHECKSUM,
     true,
     true},
    {"slave_transaction_retries",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_TRANSACTION_RETRIES,
     true,
     true},
    {"slave_type_conversions", MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_TYPE_CONVERSIONS, true, true},
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
    {"source_verify_checksum", MYLITE_EXECUTION_SYSTEM_VARIABLE_SOURCE_VERIFY_CHECKSUM, true, true},
    {"statement_id", MYLITE_EXECUTION_SYSTEM_VARIABLE_STATEMENT_ID, true, false},
    {"stored_program_cache", MYLITE_EXECUTION_SYSTEM_VARIABLE_STORED_PROGRAM_CACHE, true, true},
    {"stored_program_definition_cache",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_STORED_PROGRAM_DEFINITION_CACHE,
     true,
     true},
    {"super_read_only", MYLITE_EXECUTION_SYSTEM_VARIABLE_SUPER_READ_ONLY, true, true},
    {"sync_binlog", MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_BINLOG, true, true},
    {"sync_master_info", MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_MASTER_INFO, true, true},
    {"sync_relay_log", MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_RELAY_LOG, true, true},
    {"sync_relay_log_info", MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_RELAY_LOG_INFO, true, true},
    {"sync_source_info", MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_SOURCE_INFO, true, true},
    {"system_time_zone", MYLITE_EXECUTION_SYSTEM_VARIABLE_SYSTEM_TIME_ZONE, true, true},
    {"table_definition_cache", MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_DEFINITION_CACHE, true, true},
    {"table_encryption_privilege_check",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_ENCRYPTION_PRIVILEGE_CHECK,
     true,
     true},
    {"table_open_cache", MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_OPEN_CACHE, true, true},
    {"table_open_cache_instances",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_OPEN_CACHE_INSTANCES,
     true,
     true},
    {"tablespace_definition_cache",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLESPACE_DEFINITION_CACHE,
     true,
     true},
    {"temptable_max_ram", MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_MAX_RAM, true, true},
    {"temptable_max_mmap", MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_MAX_MMAP, true, true},
    {"temptable_use_mmap", MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_USE_MMAP, true, true},
    {"terminology_use_previous",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_TERMINOLOGY_USE_PREVIOUS,
     true,
     true},
    {"thread_cache_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_CACHE_SIZE, true, true},
    {"thread_handling", MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_HANDLING, true, true},
    {"thread_stack", MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_STACK, true, true},
    {"timestamp", MYLITE_EXECUTION_SYSTEM_VARIABLE_TIMESTAMP, true, false},
    {"time_zone", MYLITE_EXECUTION_SYSTEM_VARIABLE_TIME_ZONE, true, true},
    {"tls_certificates_enforced_validation",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_CERTIFICATES_ENFORCED_VALIDATION,
     true,
     true},
    {"tls_ciphersuites", MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_CIPHERSUITES, true, true},
    {"tls_version", MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_VERSION, true, true},
    {"tmpdir", MYLITE_EXECUTION_SYSTEM_VARIABLE_TMPDIR, true, true},
    {"tmp_table_size", MYLITE_EXECUTION_SYSTEM_VARIABLE_TMP_TABLE_SIZE, true, true},
    {"transaction_alloc_block_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ALLOC_BLOCK_SIZE,
     true,
     true},
    {"transaction_allow_batching",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ALLOW_BATCHING,
     true,
     false},
    {"transaction_isolation", MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ISOLATION, true, true},
    {"transaction_prealloc_size",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_PREALLOC_SIZE,
     true,
     true},
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
    {"windowing_use_high_precision",
     MYLITE_EXECUTION_SYSTEM_VARIABLE_WINDOWING_USE_HIGH_PRECISION,
     true,
     true},
    {"xa_detach_on_prepare", MYLITE_EXECUTION_SYSTEM_VARIABLE_XA_DETACH_ON_PREPARE, true, true},
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING_LWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_HASH_INDEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_HASH_INDEX_PARTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_MAX_SLEEP_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_AUTOEXTEND_INCREMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_AUTOINC_LOCK_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_CHUNK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_AT_SHUTDOWN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_NOW:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_FILENAME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_IN_CORE_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_ABORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_AT_STARTUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_NOW:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFER_MAX_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFERING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHECKSUM_ALGORITHM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CMP_PER_INDEX_ENABLED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMMIT_CONCURRENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_FAILURE_THRESHOLD_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_PAD_PCT_MAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CONCURRENCY_TICKETS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DATA_FILE_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DATA_HOME_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEADLOCK_DETECT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEDICATED_SERVER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEFAULT_ROW_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DIRECTORIES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DISABLE_SORT_FILE_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_BATCH_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_FILES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_EXTEND_AND_INITIALIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FAST_SHUTDOWN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILE_PER_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILL_FACTOR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TRX_COMMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_METHOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_NEIGHBORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_SYNC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSHING_AVG_LOOPS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FORCE_LOAD_CORRUPTED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FORCE_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FSYNC_THRESHOLD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_AUX_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_DIAG_PRINT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_STOPWORD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_MAX_TOKEN_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_MIN_TOKEN_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_NUM_WORD_OPTIMIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_RESULT_CACHE_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_SERVER_STOPWORD_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_SORT_PLL_DEGREE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_TOTAL_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_USER_STOPWORD_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IDLE_FLUSH_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY_MAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOCK_WAIT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_CHECKSUMS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_COMPRESSED_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_FILE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_FILES_IN_GROUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_GROUP_HOME_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_ABS_LWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_PCT_HWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WAIT_FOR_FLUSH_SPIN_HWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITE_AHEAD_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITER_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LRU_SCAN_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT_LWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_UNDO_LOG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_DISABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_ENABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_RESET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_RESET_ALL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OLD_BLOCKS_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OLD_BLOCKS_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ONLINE_ALTER_LOG_MAX_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OPEN_FILES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OPTIMIZE_FULLTEXT_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PAGE_CLEANERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PAGE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PARALLEL_READ_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PRINT_ALL_DEADLOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PRINT_DDL_LOGS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_BATCH_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_RSEG_TRUNCATE_FREQUENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_RANDOM_READ_AHEAD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_AHEAD_THRESHOLD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_IO_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_ARCHIVE_DIRS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_CAPACITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_ENCRYPT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REPLICATION_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ROLLBACK_ON_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ROLLBACK_SEGMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SEGMENT_RESERVE_FACTOR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SORT_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SPIN_WAIT_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SPIN_WAIT_PAUSE_MULTIPLIER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_AUTO_RECALC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_INCLUDE_DELETE_MARKED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_METHOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_ON_METADATA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_PERSISTENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_PERSISTENT_SAMPLE_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_TRANSIENT_SAMPLE_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATUS_OUTPUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATUS_OUTPUT_LOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STRICT_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SYNC_ARRAY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SYNC_SPIN_LOOPS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TABLE_LOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TEMP_DATA_FILE_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TEMP_TABLESPACES_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_THREAD_CONCURRENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_THREAD_SLEEP_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TMPDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_DIRECTORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_LOG_ENCRYPT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_LOG_TRUNCATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_TABLESPACES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_USE_FDATASYNC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_USE_NATIVE_AIO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_VALIDATE_TABLESPACE_PATHS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_WRITE_IO_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERNAL_TMP_MEM_STORAGE_ENGINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_JOIN_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_AGE_THRESHOLD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_BLOCK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_DIVISION_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEYRING_OPERATIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_FILES_SUPPORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_PAGE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LC_MESSAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LC_MESSAGES_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LC_TIME_NAMES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCAL_INFILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCKED_IN_MEMORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MANDATORY_ROLES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MASTER_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_BINLOG_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_BINLOG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_BINLOG_STMT_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_CONNECT_ERRORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_CONNECTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_DELAYED_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_DIGEST_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_PREPARED_STMT_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_EXECUTION_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_HEAP_TABLE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_INSERT_DELAYED_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_JOIN_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_LENGTH_FOR_SORT_DATA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_POINTS_IN_GEOMETRY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_RELAY_LOG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SEEKS_FOR_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SORT_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SP_RECURSION_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_USER_CONNECTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_WRITE_LOCK_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MIN_EXAMINED_ROW_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_DATA_POINTER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_MAX_SORT_FILE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_MMAP_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_RECOVER_OPTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_SORT_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_STATS_METHOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_USE_MMAP:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_BUFFER_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_READ_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_RETRY_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_WRITE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NGRAM_TOKEN_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OFFLINE_MODE:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPEN_FILES_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_PRUNE_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_SEARCH_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_SWITCH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_FEATURES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_MAX_MEM_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_OFFSET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PARSER_MAX_MEM_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PARTIAL_REVOKES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_HISTORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REQUIRE_CURRENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REUSE_INTERVAL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_ACCOUNTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_DIGESTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_ERROR_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STATEMENTS_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STATEMENTS_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_TRANSACTIONS_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_TRANSACTIONS_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_HOSTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_COND_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_COND_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_DIGEST_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_DIGEST_SAMPLE_AGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_HANDLES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_INDEX_STAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MEMORY_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METADATA_LOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METER_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METRIC_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MUTEX_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MUTEX_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_PREPARED_STATEMENTS_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_PROGRAM_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_RWLOCK_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_RWLOCK_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SOCKET_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SOCKET_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SQL_TEXT_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STAGE_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STATEMENT_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STATEMENT_STACK:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_HANDLES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_LOCK_STAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_THREAD_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_THREAD_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SESSION_CONNECT_ATTRS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SETUP_ACTORS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SETUP_OBJECTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SHOW_PROCESSLIST:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_USERS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSIST_ONLY_ADMIN_X509_SUBJECT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSIST_SENSITIVE_VARIABLES_IN_PLAINTEXT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSISTED_GLOBALS_LOAD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PID_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PLUGIN_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PRINT_IDENTIFIED_WITH_AS_HEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_COMPRESSION_ALGORITHMS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROFILING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROFILING_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PRELOAD_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_QUERY_ALLOC_BLOCK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_QUERY_PREALLOC_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RANGE_ALLOC_BLOCK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RANGE_OPTIMIZER_MAX_MEM_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RBR_EXEC_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_RND_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_BASENAME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_INDEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_PURGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_SPACE_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REGEXP_STACK_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REGEXP_TIME_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICATION_OPTIMIZE_FOR_STATIC_PLUGIN_CONFIG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICATION_SENDER_OBSERVE_COMMIT_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_ALLOW_BATCHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_CHECKPOINT_GROUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_CHECKPOINT_PERIOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_COMPRESSED_PROTOCOL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_EXEC_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_LOAD_TMPDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_MAX_ALLOWED_PACKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_NET_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PARALLEL_TYPE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PARALLEL_WORKERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PENDING_JOBS_SIZE_MAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PRESERVE_COMMIT_ORDER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_SKIP_ERRORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_SQL_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_TRANSACTION_RETRIES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_TYPE_CONVERSIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_HOST:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_PASSWORD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_USER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_SECURE_TRANSPORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RESTRICT_FK_ON_NON_STANDARD_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SCHEMA_DEFINITION_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SECONDARY_ENGINE_COST_THRESHOLD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_DISK_SYNC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_DISK_SYNC_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SECURE_FILE_PRIV:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID_BITS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_UUID:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_GTIDS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_STATE_CHANGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_SYSTEM_VARIABLES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_TRANSACTION_INFO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SET_OPERATIONS_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_AUTO_GENERATE_RSA_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PRIVATE_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PUBLIC_KEY_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_GIPK_IN_CREATE_TABLE_AND_INFORMATION_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_VERBOSITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_EXTERNAL_LOCKING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_NAME_RESOLVE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_NETWORKING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_REPLICA_START:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SHOW_DATABASE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SLAVE_START:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_ALLOW_BATCHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_CHECKPOINT_GROUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_CHECKPOINT_PERIOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_COMPRESSED_PROTOCOL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_EXEC_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_LOAD_TMPDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_MAX_ALLOWED_PACKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_NET_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PARALLEL_TYPE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PARALLEL_WORKERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PENDING_JOBS_SIZE_MAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PRESERVE_COMMIT_ORDER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SKIP_ERRORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SQL_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_TRANSACTION_RETRIES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_TYPE_CONVERSIONS:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SOURCE_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_READ_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_STOP_REPLICA_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_STOP_SLAVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_STORED_PROGRAM_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_STORED_PROGRAM_DEFINITION_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SUPER_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_BINLOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_MASTER_INFO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_RELAY_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_RELAY_LOG_INFO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_SOURCE_INFO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_DEFINITION_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_ENCRYPTION_PRIVILEGE_CHECK:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_OPEN_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_OPEN_CACHE_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLESPACE_DEFINITION_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_MAX_RAM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TERMINOLOGY_USE_PREVIOUS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_MAX_MMAP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_USE_MMAP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_HANDLING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_STACK:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_CERTIFICATES_ENFORCED_VALIDATION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_CIPHERSUITES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TMPDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TMP_TABLE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ALLOC_BLOCK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ISOLATION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_PREALLOC_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_MACHINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_OS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_VERSION_COMPILE_ZLIB:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WINDOWING_USE_HIGH_PRECISION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_XA_DETACH_ON_PREPARE:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING_LWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_HASH_INDEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_HASH_INDEX_PARTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_MAX_SLEEP_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_AUTOEXTEND_INCREMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_AUTOINC_LOCK_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_CHUNK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_AT_SHUTDOWN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_NOW:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_FILENAME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_IN_CORE_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_ABORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_AT_STARTUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_NOW:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFER_MAX_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFERING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHECKSUM_ALGORITHM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CMP_PER_INDEX_ENABLED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMMIT_CONCURRENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_FAILURE_THRESHOLD_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_PAD_PCT_MAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CONCURRENCY_TICKETS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DATA_FILE_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DATA_HOME_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEADLOCK_DETECT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEDICATED_SERVER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEFAULT_ROW_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DIRECTORIES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DISABLE_SORT_FILE_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_BATCH_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_FILES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_EXTEND_AND_INITIALIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FAST_SHUTDOWN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILE_PER_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILL_FACTOR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TRX_COMMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_METHOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_NEIGHBORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_SYNC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSHING_AVG_LOOPS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FORCE_LOAD_CORRUPTED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FORCE_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FSYNC_THRESHOLD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_AUX_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_DIAG_PRINT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_MAX_TOKEN_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_MIN_TOKEN_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_NUM_WORD_OPTIMIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_RESULT_CACHE_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_SERVER_STOPWORD_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_SORT_PLL_DEGREE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_TOTAL_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IDLE_FLUSH_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY_MAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_CHECKSUMS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_COMPRESSED_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_FILE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_FILES_IN_GROUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_GROUP_HOME_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_ABS_LWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_PCT_HWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WAIT_FOR_FLUSH_SPIN_HWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITE_AHEAD_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITER_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LRU_SCAN_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT_LWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_UNDO_LOG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_DISABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_ENABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_RESET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MONITOR_RESET_ALL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OLD_BLOCKS_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OLD_BLOCKS_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ONLINE_ALTER_LOG_MAX_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OPEN_FILES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OPTIMIZE_FULLTEXT_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PAGE_CLEANERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PAGE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PRINT_ALL_DEADLOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PRINT_DDL_LOGS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_BATCH_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_RSEG_TRUNCATE_FREQUENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_RANDOM_READ_AHEAD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_AHEAD_THRESHOLD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_IO_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_ARCHIVE_DIRS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_CAPACITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REDO_LOG_ENCRYPT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_REPLICATION_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ROLLBACK_ON_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ROLLBACK_SEGMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SEGMENT_RESERVE_FACTOR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SORT_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SPIN_WAIT_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SPIN_WAIT_PAUSE_MULTIPLIER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_AUTO_RECALC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_INCLUDE_DELETE_MARKED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_METHOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_ON_METADATA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_PERSISTENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_PERSISTENT_SAMPLE_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATS_TRANSIENT_SAMPLE_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATUS_OUTPUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STATUS_OUTPUT_LOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SYNC_ARRAY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SYNC_SPIN_LOOPS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TEMP_DATA_FILE_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TEMP_TABLESPACES_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_THREAD_CONCURRENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_THREAD_SLEEP_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_DIRECTORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_LOG_ENCRYPT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_LOG_TRUNCATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_TABLESPACES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_USE_FDATASYNC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_USE_NATIVE_AIO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_VALIDATE_TABLESPACE_PATHS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_WRITE_IO_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_AGE_THRESHOLD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_BLOCK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_DIVISION_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEYRING_OPERATIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_FILES_SUPPORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_PAGE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LC_MESSAGES_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCAL_INFILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCKED_IN_MEMORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MANDATORY_ROLES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MASTER_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_BINLOG_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_BINLOG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_BINLOG_STMT_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_CONNECT_ERRORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_CONNECTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_DIGEST_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_PREPARED_STMT_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_RELAY_LOG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_WRITE_LOCK_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_DATA_POINTER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_MAX_SORT_FILE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_MMAP_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_RECOVER_OPTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_USE_MMAP:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NGRAM_TOKEN_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OFFLINE_MODE:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PARTIAL_REVOKES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_HISTORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REQUIRE_CURRENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REUSE_INTERVAL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_ACCOUNTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_DIGESTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_ERROR_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STATEMENTS_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STATEMENTS_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_TRANSACTIONS_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_TRANSACTIONS_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_HOSTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_COND_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_COND_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_DIGEST_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_DIGEST_SAMPLE_AGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_HANDLES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_INDEX_STAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MEMORY_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METADATA_LOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METER_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METRIC_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MUTEX_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MUTEX_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_PREPARED_STATEMENTS_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_PROGRAM_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_RWLOCK_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_RWLOCK_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SOCKET_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SOCKET_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SQL_TEXT_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STAGE_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STATEMENT_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STATEMENT_STACK:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_HANDLES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_LOCK_STAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_THREAD_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_THREAD_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SESSION_CONNECT_ATTRS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SETUP_ACTORS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SETUP_OBJECTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SHOW_PROCESSLIST:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_USERS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSIST_ONLY_ADMIN_X509_SUBJECT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSIST_SENSITIVE_VARIABLES_IN_PLAINTEXT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSISTED_GLOBALS_LOAD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_BASENAME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_INDEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_PURGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_SPACE_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REGEXP_STACK_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REGEXP_TIME_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICATION_OPTIMIZE_FOR_STATIC_PLUGIN_CONFIG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICATION_SENDER_OBSERVE_COMMIT_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_ALLOW_BATCHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_CHECKPOINT_GROUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_CHECKPOINT_PERIOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_COMPRESSED_PROTOCOL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_EXEC_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_LOAD_TMPDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_MAX_ALLOWED_PACKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_NET_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PARALLEL_TYPE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PARALLEL_WORKERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PENDING_JOBS_SIZE_MAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PRESERVE_COMMIT_ORDER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_SKIP_ERRORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_SQL_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_TRANSACTION_RETRIES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_TYPE_CONVERSIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_HOST:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_PASSWORD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_USER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_COMPRESSION_ALGORITHMS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_SECURE_TRANSPORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_READ_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_STOP_REPLICA_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_STOP_SLAVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SCHEMA_DEFINITION_CACHE:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_REPLICA_START:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SHOW_DATABASE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SLAVE_START:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_ALLOW_BATCHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_CHECKPOINT_GROUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_CHECKPOINT_PERIOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_COMPRESSED_PROTOCOL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_EXEC_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_LOAD_TMPDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_MAX_ALLOWED_PACKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_NET_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PARALLEL_TYPE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PARALLEL_WORKERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PENDING_JOBS_SIZE_MAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PRESERVE_COMMIT_ORDER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SKIP_ERRORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SQL_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_TRANSACTION_RETRIES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_TYPE_CONVERSIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SOURCE_VERIFY_CHECKSUM:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_STORED_PROGRAM_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_STORED_PROGRAM_DEFINITION_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SUPER_READ_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_BINLOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_MASTER_INFO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_RELAY_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_RELAY_LOG_INFO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_SOURCE_INFO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_DEFINITION_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_ENCRYPTION_PRIVILEGE_CHECK:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_OPEN_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_OPEN_CACHE_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLESPACE_DEFINITION_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_MAX_MMAP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_USE_MMAP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_HANDLING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_STACK:
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
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_STATEMENTS_UNSAFE_FOR_BINLOG ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_DELAYED_THREADS ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_INSERT_DELAYED_THREADS ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_LENGTH_FOR_SORT_DATA ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_MASTER_VERIFY_CHECKSUM ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_PROFILING ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_PROFILING_HISTORY_SIZE ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_PSEUDO_SLAVE_MODE ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_QUERY_PREALLOC_SIZE ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PARALLEL_TYPE ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_STOP_SLAVE_TIMEOUT ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SLAVE_START ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_ALLOW_BATCHING ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_CHECKPOINT_GROUP ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_CHECKPOINT_PERIOD ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_COMPRESSED_PROTOCOL ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_EXEC_MODE ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_LOAD_TMPDIR ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_MAX_ALLOWED_PACKET ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_NET_TIMEOUT ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PARALLEL_TYPE ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PARALLEL_WORKERS ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PENDING_JOBS_SIZE_MAX ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PRESERVE_COMMIT_ORDER ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SKIP_ERRORS ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SQL_VERIFY_CHECKSUM ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_TRANSACTION_RETRIES ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_TYPE_CONVERSIONS ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_MASTER_INFO ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_RELAY_LOG_INFO ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TERMINOLOGY_USE_PREVIOUS ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_PREALLOC_SIZE) != 0;
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_FILES_SUPPORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_PAGE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LC_MESSAGES_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LICENSE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCKED_IN_MEMORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_DIGEST_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NGRAM_TOKEN_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSIST_ONLY_ADMIN_X509_SUBJECT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSIST_SENSITIVE_VARIABLES_IN_PLAINTEXT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSISTED_GLOBALS_LOAD:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_OPEN_CACHE_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_HANDLING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_STACK:
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

bool mylite_execution_system_variable_is_read_only_myisam(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_MMAP_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_RECOVER_OPTIONS:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_read_only_innodb_core(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_HASH_INDEX_PARTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_AUTOINC_LOCK_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_CHUNK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_AT_STARTUP:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_read_only_innodb_storage(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DATA_FILE_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DATA_HOME_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEDICATED_SERVER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DIRECTORIES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_BATCH_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_FILES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_METHOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FORCE_LOAD_CORRUPTED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FORCE_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_MAX_TOKEN_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_MIN_TOKEN_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_SORT_PLL_DEGREE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_TOTAL_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_FILE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_FILES_IN_GROUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_GROUP_HOME_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_OPEN_FILES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PAGE_CLEANERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PAGE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_PURGE_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_IO_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ROLLBACK_ON_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SORT_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_SYNC_ARRAY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TEMP_DATA_FILE_PATH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TEMP_TABLESPACES_DIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_UNDO_DIRECTORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_USE_NATIVE_AIO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_VALIDATE_TABLESPACE_PATHS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_WRITE_IO_THREADS:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_read_only_performance_schema(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_ACCOUNTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_DIGESTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_ERROR_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STAGES_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STATEMENTS_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_STATEMENTS_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_TRANSACTIONS_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_TRANSACTIONS_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_HISTORY_LONG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_EVENTS_WAITS_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_HOSTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_COND_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_COND_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_DIGEST_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_HANDLES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_FILE_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_INDEX_STAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MEMORY_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METADATA_LOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METER_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_METRIC_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MUTEX_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_MUTEX_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_PREPARED_STATEMENTS_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_PROGRAM_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_RWLOCK_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_RWLOCK_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SOCKET_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SOCKET_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_SQL_TEXT_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STAGE_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STATEMENT_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_STATEMENT_STACK:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_HANDLES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_TABLE_LOCK_STAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_THREAD_CLASSES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_THREAD_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SESSION_CONNECT_ATTRS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SETUP_ACTORS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SETUP_OBJECTS_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_USERS_SIZE:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_AGE_THRESHOLD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_BLOCK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_DIVISION_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEYRING_OPERATIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LC_MESSAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCAL_INFILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MANDATORY_ROLES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MASTER_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_BINLOG_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_BINLOG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_BINLOG_STMT_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_CONNECT_ERRORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_CONNECTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_PREPARED_STMT_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_RELAY_LOG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_WRITE_LOCK_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OFFLINE_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_COMPRESSION_ALGORITHMS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SCHEMA_DEFINITION_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_STORED_PROGRAM_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_STORED_PROGRAM_DEFINITION_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_BINLOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_DEFINITION_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_ENCRYPTION_PRIVILEGE_CHECK:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_OPEN_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLESPACE_DEFINITION_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_MAX_MMAP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_USE_MMAP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_CACHE_SIZE:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_timeout(enum mylite_execution_system_variable_kind kind) {
    return (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCK_WAIT_TIMEOUT ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOCK_WAIT_TIMEOUT ||
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

bool mylite_execution_system_variable_is_m_session_limit(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_DELAYED_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_EXECUTION_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_HEAP_TABLE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_INSERT_DELAYED_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_JOIN_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_LENGTH_FOR_SORT_DATA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_POINTS_IN_GEOMETRY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SEEKS_FOR_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SORT_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SP_RECURSION_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_USER_CONNECTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MIN_EXAMINED_ROW_LIMIT:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_o_optimizer(enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_PRUNE_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_SEARCH_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_SWITCH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_FEATURES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_MAX_MEM_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_OFFSET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PARSER_MAX_MEM_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PARTIAL_REVOKES:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_replication_global(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_BASENAME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_INDEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_PURGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_SPACE_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICATION_OPTIMIZE_FOR_STATIC_PLUGIN_CONFIG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICATION_SENDER_OBSERVE_COMMIT_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_ALLOW_BATCHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_CHECKPOINT_GROUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_CHECKPOINT_PERIOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_COMPRESSED_PROTOCOL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_EXEC_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_LOAD_TMPDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_MAX_ALLOWED_PACKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_NET_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PARALLEL_TYPE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PARALLEL_WORKERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PENDING_JOBS_SIZE_MAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PRESERVE_COMMIT_ORDER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_SKIP_ERRORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_SQL_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_TRANSACTION_RETRIES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_TYPE_CONVERSIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_HOST:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_PASSWORD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_USER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_READ_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_STOP_REPLICA_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_STOP_SLAVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_REPLICA_START:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SLAVE_START:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_ALLOW_BATCHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_CHECKPOINT_GROUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_CHECKPOINT_PERIOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_COMPRESSED_PROTOCOL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_EXEC_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_LOAD_TMPDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_MAX_ALLOWED_PACKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_NET_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PARALLEL_TYPE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PARALLEL_WORKERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PENDING_JOBS_SIZE_MAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PRESERVE_COMMIT_ORDER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SKIP_ERRORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SQL_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_TRANSACTION_RETRIES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_TYPE_CONVERSIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SOURCE_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_MASTER_INFO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_RELAY_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_RELAY_LOG_INFO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_SOURCE_INFO:
        return true;
    default:
        return false;
    }
}

bool mylite_execution_system_variable_is_read_only_replication_global(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_BASENAME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_INDEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_SPACE_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_LOAD_TMPDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_SKIP_ERRORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_HOST:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_PASSWORD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_PORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_USER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_REPLICA_START:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SLAVE_START:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_LOAD_TMPDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SKIP_ERRORS:
        return true;
    default:
        return false;
    }
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_STOPWORD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STRICT_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TABLE_LOCKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEYRING_OPERATIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_FILES_SUPPORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEEP_FILES_ON_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCAL_INFILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCKED_IN_MEMORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MASTER_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ENABLE_HELLO_NOTICE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQL_NATIVE_PASSWORD_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OFFLINE_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OLD_ALTER_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PARTIAL_REVOKES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REQUIRE_CURRENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSIST_SENSITIVE_VARIABLES_IN_PLAINTEXT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSISTED_GLOBALS_LOAD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROFILING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PRINT_IDENTIFIED_WITH_AS_HEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PSEUDO_REPLICA_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PSEUDO_SLAVE_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_PURGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_ALLOW_BATCHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_COMPRESSED_PROTOCOL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PRESERVE_COMMIT_ORDER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_SQL_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICATION_OPTIMIZE_FOR_STATIC_PLUGIN_CONFIG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICATION_SENDER_OBSERVE_COMMIT_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_ROW_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RESTRICT_FK_ON_NON_STANDARD_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_DISK_SYNC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_STATE_CHANGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_AUTO_GENERATE_RSA_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_GIPK_IN_CREATE_TABLE_AND_INFORMATION_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_SKIP_SECONDARY_ENGINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_VERBOSITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_REPLICA_START:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SLAVE_START:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_ALLOW_BATCHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_COMPRESSED_PROTOCOL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PRESERVE_COMMIT_ORDER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SQL_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SOURCE_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_ENCRYPTION_PRIVILEGE_CHECK:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_USE_MMAP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UPDATABLE_VIEWS_WITH_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BIG_SELECTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_OFF:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_NOTES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_QUOTE_SHOW_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ALLOW_BATCHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UNIQUE_CHECKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WINDOWING_USE_HIGH_PRECISION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_XA_DETACH_ON_PREPARE:
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ORIGINAL_SERVER_VERSION:
        return "999999";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_CONNECT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_REPLICA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INIT_SLAVE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MANDATORY_ROLES:
        return "";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_STRICT_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_TABLE_LOCKS:
        return "1";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERNAL_TMP_MEM_STORAGE_ENGINE:
        return "TempTable";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_JOIN_BUFFER_SIZE:
        return "262144";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_BUFFER_SIZE:
        return "8388608";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_AGE_THRESHOLD:
        return "300";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_BLOCK_SIZE:
        return "1024";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_DIVISION_LIMIT:
        return "100";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_PAGE_SIZE:
        return "0";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LC_MESSAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LC_TIME_NAMES:
        return "en_US";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LC_MESSAGES_DIR:
        return "/usr/share/mysql-8.4/";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_BINLOG_CACHE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_BINLOG_STMT_CACHE_SIZE:
        return "18446744073709547520";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_BINLOG_SIZE:
        return "1073741824";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_CONNECT_ERRORS:
        return "100";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_CONNECTIONS:
        return "151";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_DELAYED_THREADS:
        return "20";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_DIGEST_LENGTH:
        return "1024";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_EXECUTION_TIME:
        return "0";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_HEAP_TABLE_SIZE:
        return "16777216";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_INSERT_DELAYED_THREADS:
        return "20";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_JOIN_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SEEKS_FOR_KEY:
        return "18446744073709551615";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_LENGTH_FOR_SORT_DATA:
        return "4096";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_POINTS_IN_GEOMETRY:
        return "65536";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_PREPARED_STMT_COUNT:
        return "16382";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SORT_LENGTH:
        return "1024";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SP_RECURSION_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_USER_CONNECTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MIN_EXAMINED_ROW_LIMIT:
        return "0";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_WRITE_LOCK_COUNT:
        return "18446744073709551615";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_DATA_POINTER_SIZE:
        return "6";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_MAX_SORT_FILE_SIZE:
        return "9223372036853727232";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_MMAP_SIZE:
        return "18446744073709551615";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_RECOVER_OPTIONS:
        return "OFF";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_SORT_BUFFER_SIZE:
        return "8388608";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_STATS_METHOD:
        return "nulls_unequal";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_BUFFER_LENGTH:
        return "16384";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PRELOAD_BUFFER_SIZE:
        return "32768";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROFILING:
        return "0";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROFILING_HISTORY_SIZE:
        return "15";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_QUERY_ALLOC_BLOCK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_QUERY_PREALLOC_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ALLOC_BLOCK_SIZE:
        return "8192";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RANGE_ALLOC_BLOCK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_PREALLOC_SIZE:
        return "4096";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RANGE_OPTIMIZER_MAX_MEM_SIZE:
        return "8388608";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_BUFFER_SIZE:
        return "131072";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_RND_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SET_OPERATIONS_BUFFER_SIZE:
        return "262144";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REGEXP_STACK_LIMIT:
        return "8000000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REGEXP_TIME_LIMIT:
        return "32";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RESTRICT_FK_ON_NON_STANDARD_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_GIPK_IN_CREATE_TABLE_AND_INFORMATION_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WINDOWING_USE_HIGH_PRECISION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_XA_DETACH_ON_PREPARE:
        return "1";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SECONDARY_ENGINE_COST_THRESHOLD:
        return "100000.000000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_DISK_SYNC_DELAY:
        return "0";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_SYSTEM_VARIABLES:
        return "time_zone,autocommit,character_set_client,character_set_results,"
               "character_set_connection";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TERMINOLOGY_USE_PREVIOUS:
        return "NONE";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TMP_TABLE_SIZE:
        return "16777216";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING_LWM:
        return "10";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_HASH_INDEX_PARTS:
        return "8";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_MAX_SLEEP_DELAY:
        return "150000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_AUTOEXTEND_INCREMENT:
        return "64";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_AUTOINC_LOCK_MODE:
        return "2";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_CHUNK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_SIZE:
        return "134217728";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFER_MAX_SIZE:
        return "25";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_FILENAME:
        return "ib_buffer_pool";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFERING:
        return "none";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHECKSUM_ALGORITHM:
        return "crc32";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DATA_FILE_PATH:
        return "ibdata1:12M:autoextend";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEFAULT_ROW_FORMAT:
        return "dynamic";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE:
        return "ON";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_FAILURE_THRESHOLD_PCT:
        return "5";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_LEVEL:
        return "6";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMPRESSION_PAD_PCT_MAX:
        return "50";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CONCURRENCY_TICKETS:
        return "5000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_BUFFER_SIZE:
        return "1048576";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DDL_THREADS:
        return "4";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_FILES:
        return "2";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_PAGES:
        return "128";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILL_FACTOR:
        return "100";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_METHOD:
        return "O_DIRECT";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSHING_AVG_LOOPS:
        return "30";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IDLE_FLUSH_PCT:
        return "100";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY:
        return "10000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_IO_CAPACITY_MAX:
        return "20000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_BUFFER_SIZE:
        return "67108864";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_FILE_SIZE:
        return "50331648";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_FILES_IN_GROUP:
        return "2";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_GROUP_HOME_DIR:
        return "./";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_ABS_LWM:
        return "80";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_SPIN_CPU_PCT_HWM:
        return "50";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WAIT_FOR_FLUSH_SPIN_HWM:
        return "400";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITE_AHEAD_SIZE:
        return "8192";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LRU_SCAN_DEPTH:
        return "1024";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT:
        return "90.000000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_DIRTY_PAGES_PCT_LWM:
        return "10.000000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_PURGE_LAG_DELAY:
        return "0";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_MAX_UNDO_LOG_SIZE:
        return "1073741824";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_CACHE_SIZE:
        return "8000000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_MAX_TOKEN_SIZE:
        return "84";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_MIN_TOKEN_SIZE:
        return "3";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_NUM_WORD_OPTIMIZE:
        return "2000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_RESULT_CACHE_LIMIT:
        return "2000000000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_SORT_PLL_DEGREE:
        return "2";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_TOTAL_CACHE_SIZE:
        return "640000000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_PASSWORD_LIFETIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CREATE_ADMIN_LISTENER_THREAD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CORE_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TABLE_ENCRYPTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_WEEK_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FLUSH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OFFLINE_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_HISTORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REUSE_INTERVAL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PSEUDO_REPLICA_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PSEUDO_SLAVE_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_ENCRYPTION_PRIVILEGE_CHECK:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_MAX_MMAP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_USE_MMAP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ALLOW_BATCHING:
        return "0";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ORIGINAL_COMMIT_TIMESTAMP:
        return "36028797018963968";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NGRAM_TOKEN_SIZE:
        return "2";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSIST_ONLY_ADMIN_X509_SUBJECT:
        return "";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSIST_SENSITIVE_VARIABLES_IN_PLAINTEXT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PERSISTED_GLOBALS_LOAD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_BINLOG:
        return "1";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_COMPRESSION_ALGORITHMS:
        return "zlib,zstd,uncompressed";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SCHEMA_DEFINITION_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_STORED_PROGRAM_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_STORED_PROGRAM_DEFINITION_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLESPACE_DEFINITION_CACHE:
        return "256";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_DEFINITION_CACHE:
        return "2000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_OPEN_CACHE:
        return "4000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TABLE_OPEN_CACHE_INSTANCES:
        return "16";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_CACHE_SIZE:
        return "9";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_THREAD_STACK:
        return "1048576";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG:
        return "mylite-relay-bin";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_BASENAME:
        return "/var/lib/mysql/mylite-relay-bin";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_INDEX:
        return "/var/lib/mysql/mylite-relay-bin.index";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_SPACE_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_COMPRESSED_PROTOCOL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_COMPRESSED_PROTOCOL:
        return "0";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_ALLOW_BATCHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PRESERVE_COMMIT_ORDER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_SQL_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_ALLOW_BATCHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PRESERVE_COMMIT_ORDER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SQL_VERIFY_CHECKSUM:
        return "1";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_CHECKPOINT_GROUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_CHECKPOINT_GROUP:
        return "512";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_CHECKPOINT_PERIOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_CHECKPOINT_PERIOD:
        return "300";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_EXEC_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_EXEC_MODE:
        return "STRICT";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_LOAD_TMPDIR:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_LOAD_TMPDIR:
        return "/tmp";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_MAX_ALLOWED_PACKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_MAX_ALLOWED_PACKET:
        return "1073741824";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_NET_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_NET_TIMEOUT:
        return "60";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PARALLEL_TYPE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PARALLEL_TYPE:
        return "LOGICAL_CLOCK";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PARALLEL_WORKERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PARALLEL_WORKERS:
        return "4";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PENDING_JOBS_SIZE_MAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PENDING_JOBS_SIZE_MAX:
        return "134217728";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_SKIP_ERRORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SKIP_ERRORS:
        return "OFF";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_TRANSACTION_RETRIES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_TRANSACTION_RETRIES:
        return "10";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_TYPE_CONVERSIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_TYPE_CONVERSIONS:
        return "";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPORT_PORT:
        return "3306";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RBR_EXEC_MODE:
        return "STRICT";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_READ_SIZE:
        return "8192";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_STOP_REPLICA_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_STOP_SLAVE_TIMEOUT:
        return "31536000";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_MASTER_INFO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_RELAY_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_RELAY_LOG_INFO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_SOURCE_INFO:
        return "10000";
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PARTIAL_REVOKES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REQUIRE_CURRENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PRINT_IDENTIFIED_WITH_AS_HEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICATION_OPTIMIZE_FOR_STATIC_PLUGIN_CONFIG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICATION_SENDER_OBSERVE_COMMIT_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_ROW_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_DISK_SYNC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_STATE_CHANGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_SKIP_SECONDARY_ENGINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_VERBOSITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_REPLICA_START:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SKIP_SLAVE_START:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SOURCE_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHECK_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCAL_INFILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCKED_IN_MEMORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MASTER_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_RELAY_LOG_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_HASH_INDEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_NOW:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_IN_CORE_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_ABORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_NOW:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CMP_PER_INDEX_ENABLED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_COMMIT_CONCURRENCY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEDICATED_SERVER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DISABLE_SORT_FILE_CACHE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DOUBLEWRITE_BATCH_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_NEIGHBORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FORCE_LOAD_CORRUPTED:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FORCE_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FSYNC_THRESHOLD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_DIAG_PRINT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_USE_MMAP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_OFF:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_WARNINGS:
        return "0";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_PRUNE_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_LIMIT:
        return "1";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_SEARCH_DEPTH:
        return "62";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_SWITCH:
        return "index_merge=on,index_merge_union=on,index_merge_sort_union=on,"
               "index_merge_intersection=on,engine_condition_pushdown=on,"
               "index_condition_pushdown=on,mrr=on,mrr_cost_based=on,block_nested_loop=on,"
               "batched_key_access=off,materialization=on,semijoin=on,loosescan=on,"
               "firstmatch=on,duplicateweedout=on,subquery_materialization_cost_based=on,"
               "use_index_extensions=on,condition_fanout_filter=on,derived_merge=on,"
               "use_invisible_indexes=off,skip_scan=on,hash_join=on,subquery_to_derived=off,"
               "prefer_ordering_index=on,hypergraph_optimizer=off,derived_condition_pushdown=on,"
               "hash_set_operations=on";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE:
        return "enabled=off,one_line=off";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_FEATURES:
        return "greedy_search=on,range_optimizer=on,dynamic_range=on,repeated_subselect=on";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_MAX_MEM_SIZE:
        return "1048576";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_OFFSET:
        return "-1";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PARSER_MAX_MEM_SIZE:
        return "18446744073709551615";
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLICIT_DEFAULTS_FOR_TIMESTAMP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_EXPIRE_LOGS_AUTO_PURGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_GTID_SIMPLE_RECOVERY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BINLOG_ORDER_COMMITS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_GENERATE_CERTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOMATIC_SP_PRIVILEGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CACHING_SHA2_PASSWORD_AUTO_GENERATE_RSA_KEYS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DISCONNECT_ON_EXPIRED_PASSWORD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEYRING_OPERATIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LARGE_FILES_SUPPORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_AT_SHUTDOWN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_INSTANCES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_AT_STARTUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_DEADLOCK_DETECT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_EXTEND_AND_INITIALIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FAST_SHUTDOWN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FILE_PER_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_LOG_AT_TRX_COMMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FLUSH_SYNC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_FT_ENABLE_STOPWORD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_CHECKSUMS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_COMPRESSED_PAGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOG_WRITER_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ENABLE_HELLO_NOTICE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_PURGE:
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
