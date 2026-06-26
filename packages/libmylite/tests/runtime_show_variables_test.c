#include <mylite/mylite.h>

#include "runtime/mylite_mysql_server_identity.h"

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

#ifndef P_tmpdir
#  define P_tmpdir "."
#endif

enum {
    test_path_capacity = 1024,
    test_path_suffix_capacity = 16,
    row_count_text_capacity = 32,
    variable_column_count = 2,
    session_variable_row_count = 603,
    global_variable_row_count = 590,
    sql_log_variable_row_count = 2,
    on_variable_row_count = 2,
    gtid_default_variable_row_count = 7,
    gtid_global_variable_row_count = 5,
    gtid_session_variable_row_count = 8,
    empty_gtid_variable_row_count = 3,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
    mysql_error_session_variable_only = 1238,
};

struct expected_variable_row {
    const char *name;
    const char *value;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_scalar_text_query {
    const char *sql;
    const char *expected;
    const char *context;
};

static const char default_sql_mode[] =
    "ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,"
    "ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION";

static const char *const variable_columns[variable_column_count] = {
    "Variable_name",
    "Value",
};

static int test_show_variables_values_scopes_and_filters(void);
static int test_show_variables_state_and_file_safety(void);
static int test_show_variables_diagnostics(void);
static int test_show_variables_independent_handles(void);
static int expect_query_rows(
    mylite_db *database,
    const char *sql,
    const char *const expected_rows[][variable_column_count],
    size_t expected_row_count,
    const char *context
);
static int expect_single_row(
    mylite_db *database,
    const char *sql,
    struct expected_variable_row expected,
    const char *context
);
static int expect_scalar_text(mylite_db *database, struct expected_scalar_text_query query);
static int expect_row_count(mylite_db *database, int64_t expected, const char *context);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_show_variables_values_scopes_and_filters();
    failures += test_show_variables_state_and_file_safety();
    failures += test_show_variables_diagnostics();
    failures += test_show_variables_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_show_variables_values_scopes_and_filters(void) {
    const char *const expected_session_rows[session_variable_row_count][variable_column_count] = {
        {"activate_all_roles_on_login", "OFF"},
        {"admin_address", ""},
        {"admin_port", "33062"},
        {"admin_ssl_ca", ""},
        {"admin_ssl_capath", ""},
        {"admin_ssl_cert", ""},
        {"admin_ssl_cipher", ""},
        {"admin_ssl_crl", ""},
        {"admin_ssl_crlpath", ""},
        {"admin_ssl_key", ""},
        {"admin_tls_ciphersuites", ""},
        {"admin_tls_version", "TLSv1.2,TLSv1.3"},
        {"auto_increment_increment", "1"},
        {"auto_increment_offset", "1"},
        {"auto_generate_certs", "ON"},
        {"automatic_sp_privileges", "ON"},
        {"authentication_policy", "*,,"},
        {"autocommit", "ON"},
        {"back_log", "151"},
        {"basedir", "/usr/"},
        {"big_tables", "OFF"},
        {"bind_address", "*"},
        {"binlog_cache_size", "32768"},
        {"binlog_checksum", "CRC32"},
        {"binlog_direct_non_transactional_updates", "OFF"},
        {"binlog_encryption", "OFF"},
        {"binlog_error_action", "ABORT_SERVER"},
        {"binlog_expire_logs_auto_purge", "ON"},
        {"binlog_expire_logs_seconds", "2592000"},
        {"binlog_format", "ROW"},
        {"binlog_group_commit_sync_delay", "0"},
        {"binlog_group_commit_sync_no_delay_count", "0"},
        {"binlog_gtid_simple_recovery", "ON"},
        {"binlog_max_flush_queue_time", "0"},
        {"binlog_order_commits", "ON"},
        {"binlog_rotate_encryption_master_key_at_startup", "OFF"},
        {"binlog_row_event_max_size", "8192"},
        {"binlog_row_image", "FULL"},
        {"binlog_row_metadata", "MINIMAL"},
        {"binlog_row_value_options", ""},
        {"binlog_rows_query_log_events", "OFF"},
        {"binlog_stmt_cache_size", "32768"},
        {"binlog_transaction_compression", "OFF"},
        {"binlog_transaction_compression_level_zstd", "3"},
        {"binlog_transaction_dependency_history_size", "25000"},
        {"block_encryption_mode", "aes-128-ecb"},
        {"build_id", "66e221b3840955d27f740799b5b2c6eb0baf3283"},
        {"bulk_insert_buffer_size", "8388608"},
        {"caching_sha2_password_auto_generate_rsa_keys", "ON"},
        {"caching_sha2_password_digest_rounds", "5000"},
        {"caching_sha2_password_private_key_path", "private_key.pem"},
        {"caching_sha2_password_public_key_path", "public_key.pem"},
        {"character_set_client", "utf8mb4"},
        {"character_set_connection", "utf8mb4"},
        {"character_set_database", "utf8mb4"},
        {"character_set_filesystem", "binary"},
        {"character_set_results", "utf8mb4"},
        {"character_set_server", "utf8mb4"},
        {"character_set_system", "utf8mb3"},
        {"character_sets_dir", "/usr/share/mysql-8.4/charsets/"},
        {"check_proxy_users", "OFF"},
        {"collation_connection", "utf8mb4_0900_ai_ci"},
        {"collation_database", "utf8mb4_0900_ai_ci"},
        {"collation_server", "utf8mb4_0900_ai_ci"},
        {"completion_type", "NO_CHAIN"},
        {"concurrent_insert", "AUTO"},
        {"connect_timeout", "10"},
        {"connection_control_failed_connections_threshold", "3"},
        {"connection_control_max_connection_delay", "2147483647"},
        {"connection_control_min_connection_delay", "1000"},
        {"connection_memory_chunk_size", "8192"},
        {"connection_memory_limit", "18446744073709551615"},
        {"core_file", "OFF"},
        {"create_admin_listener_thread", "OFF"},
        {"cte_max_recursion_depth", "1000"},
        {"datadir", "/var/lib/mysql/"},
        {"default_collation_for_utf8mb4", "utf8mb4_0900_ai_ci"},
        {"default_password_lifetime", "0"},
        {"default_storage_engine", "InnoDB"},
        {"default_table_encryption", "OFF"},
        {"default_tmp_storage_engine", "InnoDB"},
        {"default_week_format", "0"},
        {"delay_key_write", "ON"},
        {"delayed_insert_limit", "100"},
        {"delayed_insert_timeout", "300"},
        {"delayed_queue_size", "1000"},
        {"disconnect_on_expired_password", "ON"},
        {"disabled_storage_engines", ""},
        {"div_precision_increment", "4"},
        {"end_markers_in_json", "OFF"},
        {"enforce_gtid_consistency", "OFF"},
        {"eq_range_index_dive_limit", "200"},
        {"error_count", "0"},
        {"event_scheduler", "ON"},
        {"explain_format", "TRADITIONAL"},
        {"explain_json_format_version", "1"},
        {"explicit_defaults_for_timestamp", "ON"},
        {"external_user", ""},
        {"flush", "OFF"},
        {"flush_time", "0"},
        {"foreign_key_checks", "ON"},
        {"ft_boolean_syntax", "+ -><()~*:\"\"&|"},
        {"ft_max_word_len", "84"},
        {"ft_min_word_len", "4"},
        {"ft_query_expansion_limit", "20"},
        {"ft_stopword_file", "(built-in)"},
        {"generated_random_password_length", "20"},
        {"general_log", "OFF"},
        {"general_log_file", "/var/lib/mysql/mylite.log"},
        {"group_concat_max_len", "1024"},
        {"group_replication_consistency", "BEFORE_ON_PRIMARY_FAILOVER"},
        {"gtid_executed", ""},
        {"gtid_executed_compression_period", "0"},
        {"gtid_mode", "OFF"},
        {"gtid_next", "AUTOMATIC"},
        {"gtid_owned", ""},
        {"gtid_purged", ""},
        {"global_connection_memory_limit", "18446744073709551615"},
        {"global_connection_memory_tracking", "OFF"},
        {"have_compress", "YES"},
        {"have_dynamic_loading", "YES"},
        {"have_geometry", "YES"},
        {"have_profiling", "YES"},
        {"have_query_cache", "NO"},
        {"have_rtree_keys", "YES"},
        {"have_statement_timeout", "YES"},
        {"have_symlink", "DISABLED"},
        {"histogram_generation_max_mem_size", "20000000"},
        {"host_cache_size", "0"},
        {"hostname", "mylite"},
        {"identity", "0"},
        {"immediate_server_version", "999999"},
        {"information_schema_stats_expiry", "86400"},
        {"init_connect", ""},
        {"init_file", ""},
        {"init_replica", ""},
        {"init_slave", ""},
        {"innodb_adaptive_flushing", "ON"},
        {"innodb_adaptive_flushing_lwm", "10"},
        {"innodb_adaptive_hash_index", "OFF"},
        {"innodb_adaptive_hash_index_parts", "8"},
        {"innodb_adaptive_max_sleep_delay", "150000"},
        {"innodb_autoextend_increment", "64"},
        {"innodb_autoinc_lock_mode", "2"},
        {"innodb_buffer_pool_chunk_size", "134217728"},
        {"innodb_buffer_pool_dump_at_shutdown", "ON"},
        {"innodb_buffer_pool_dump_now", "OFF"},
        {"innodb_buffer_pool_dump_pct", "25"},
        {"innodb_buffer_pool_filename", "ib_buffer_pool"},
        {"innodb_buffer_pool_in_core_file", "OFF"},
        {"innodb_buffer_pool_instances", "1"},
        {"innodb_buffer_pool_load_abort", "OFF"},
        {"innodb_buffer_pool_load_at_startup", "ON"},
        {"innodb_buffer_pool_load_now", "OFF"},
        {"innodb_buffer_pool_size", "134217728"},
        {"innodb_change_buffer_max_size", "25"},
        {"innodb_change_buffering", "none"},
        {"innodb_checksum_algorithm", "crc32"},
        {"innodb_cmp_per_index_enabled", "OFF"},
        {"innodb_commit_concurrency", "0"},
        {"innodb_compression_failure_threshold_pct", "5"},
        {"innodb_compression_level", "6"},
        {"innodb_compression_pad_pct_max", "50"},
        {"innodb_concurrency_tickets", "5000"},
        {"innodb_data_file_path", "ibdata1:12M:autoextend"},
        {"innodb_data_home_dir", ""},
        {"innodb_ddl_buffer_size", "1048576"},
        {"innodb_ddl_threads", "4"},
        {"innodb_deadlock_detect", "ON"},
        {"innodb_dedicated_server", "OFF"},
        {"innodb_default_row_format", "dynamic"},
        {"innodb_directories", ""},
        {"innodb_disable_sort_file_cache", "OFF"},
        {"innodb_doublewrite", "ON"},
        {"innodb_doublewrite_batch_size", "0"},
        {"innodb_doublewrite_dir", ""},
        {"innodb_doublewrite_files", "2"},
        {"innodb_doublewrite_pages", "128"},
        {"innodb_extend_and_initialize", "ON"},
        {"innodb_fast_shutdown", "1"},
        {"innodb_file_per_table", "ON"},
        {"innodb_fill_factor", "100"},
        {"innodb_flush_log_at_timeout", "1"},
        {"innodb_flush_log_at_trx_commit", "1"},
        {"innodb_flush_method", "O_DIRECT"},
        {"innodb_flush_neighbors", "0"},
        {"innodb_flush_sync", "ON"},
        {"innodb_flushing_avg_loops", "30"},
        {"innodb_force_load_corrupted", "OFF"},
        {"innodb_force_recovery", "0"},
        {"innodb_fsync_threshold", "0"},
        {"innodb_ft_aux_table", ""},
        {"innodb_ft_cache_size", "8000000"},
        {"innodb_ft_enable_diag_print", "OFF"},
        {"innodb_ft_enable_stopword", "ON"},
        {"innodb_ft_max_token_size", "84"},
        {"innodb_ft_min_token_size", "3"},
        {"innodb_ft_num_word_optimize", "2000"},
        {"innodb_ft_result_cache_limit", "2000000000"},
        {"innodb_ft_server_stopword_table", ""},
        {"innodb_ft_sort_pll_degree", "2"},
        {"innodb_ft_total_cache_size", "640000000"},
        {"innodb_ft_user_stopword_table", ""},
        {"innodb_idle_flush_pct", "100"},
        {"innodb_io_capacity", "10000"},
        {"innodb_io_capacity_max", "20000"},
        {"innodb_lock_wait_timeout", "50"},
        {"innodb_log_buffer_size", "67108864"},
        {"innodb_log_checksums", "ON"},
        {"innodb_log_compressed_pages", "ON"},
        {"innodb_log_file_size", "50331648"},
        {"innodb_log_files_in_group", "2"},
        {"innodb_log_group_home_dir", "./"},
        {"innodb_log_spin_cpu_abs_lwm", "80"},
        {"innodb_log_spin_cpu_pct_hwm", "50"},
        {"innodb_log_wait_for_flush_spin_hwm", "400"},
        {"innodb_log_write_ahead_size", "8192"},
        {"innodb_log_writer_threads", "ON"},
        {"innodb_lru_scan_depth", "1024"},
        {"innodb_max_dirty_pages_pct", "90.000000"},
        {"innodb_max_dirty_pages_pct_lwm", "10.000000"},
        {"innodb_max_purge_lag", "0"},
        {"innodb_max_purge_lag_delay", "0"},
        {"innodb_max_undo_log_size", "1073741824"},
        {"innodb_monitor_disable", ""},
        {"innodb_monitor_enable", ""},
        {"innodb_monitor_reset", ""},
        {"innodb_monitor_reset_all", ""},
        {"innodb_old_blocks_pct", "37"},
        {"innodb_old_blocks_time", "1000"},
        {"innodb_online_alter_log_max_size", "134217728"},
        {"innodb_open_files", "4000"},
        {"innodb_optimize_fulltext_only", "OFF"},
        {"innodb_page_cleaners", "1"},
        {"innodb_page_size", "16384"},
        {"innodb_parallel_read_threads", "4"},
        {"innodb_print_all_deadlocks", "OFF"},
        {"innodb_print_ddl_logs", "OFF"},
        {"innodb_purge_batch_size", "300"},
        {"innodb_purge_rseg_truncate_frequency", "128"},
        {"innodb_purge_threads", "4"},
        {"innodb_random_read_ahead", "OFF"},
        {"innodb_read_ahead_threshold", "56"},
        {"innodb_read_io_threads", "9"},
        {"innodb_read_only", "OFF"},
        {"innodb_redo_log_archive_dirs", ""},
        {"innodb_redo_log_capacity", "104857600"},
        {"innodb_redo_log_encrypt", "OFF"},
        {"innodb_replication_delay", "0"},
        {"innodb_rollback_on_timeout", "OFF"},
        {"innodb_rollback_segments", "128"},
        {"innodb_segment_reserve_factor", "12.500000"},
        {"innodb_sort_buffer_size", "1048576"},
        {"innodb_spin_wait_delay", "6"},
        {"innodb_spin_wait_pause_multiplier", "50"},
        {"innodb_stats_auto_recalc", "ON"},
        {"innodb_stats_include_delete_marked", "OFF"},
        {"innodb_stats_method", "nulls_equal"},
        {"innodb_stats_on_metadata", "OFF"},
        {"innodb_stats_persistent", "ON"},
        {"innodb_stats_persistent_sample_pages", "20"},
        {"innodb_stats_transient_sample_pages", "8"},
        {"innodb_status_output", "OFF"},
        {"innodb_status_output_locks", "OFF"},
        {"innodb_strict_mode", "ON"},
        {"innodb_sync_array_size", "1"},
        {"innodb_sync_spin_loops", "30"},
        {"innodb_table_locks", "ON"},
        {"innodb_temp_data_file_path", "ibtmp1:12M:autoextend"},
        {"innodb_temp_tablespaces_dir", "./#innodb_temp/"},
        {"innodb_thread_concurrency", "0"},
        {"innodb_thread_sleep_delay", "10000"},
        {"innodb_tmpdir", ""},
        {"innodb_undo_directory", "./"},
        {"innodb_undo_log_encrypt", "OFF"},
        {"innodb_undo_log_truncate", "ON"},
        {"innodb_undo_tablespaces", "2"},
        {"innodb_use_fdatasync", "ON"},
        {"innodb_use_native_aio", "ON"},
        {"innodb_validate_tablespace_paths", "ON"},
        {"innodb_version", "8.4.9"},
        {"innodb_write_io_threads", "4"},
        {"internal_tmp_mem_storage_engine", "TempTable"},
        {"interactive_timeout", "28800"},
        {"join_buffer_size", "262144"},
        {"keep_files_on_create", "OFF"},
        {"key_buffer_size", "8388608"},
        {"key_cache_age_threshold", "300"},
        {"key_cache_block_size", "1024"},
        {"key_cache_division_limit", "100"},
        {"keyring_operations", "ON"},
        {"large_files_support", "ON"},
        {"large_page_size", "0"},
        {"large_pages", "OFF"},
        {"last_insert_id", "0"},
        {"lc_messages", "en_US"},
        {"lc_messages_dir", "/usr/share/mysql-8.4/"},
        {"license", "GPL"},
        {"local_infile", "OFF"},
        {"locked_in_memory", "OFF"},
        {"log_bin", "ON"},
        {"log_bin_basename", "binlog"},
        {"log_bin_index", "binlog.index"},
        {"log_bin_trust_function_creators", "OFF"},
        {"log_error", "stderr"},
        {"log_error_services", "log_filter_internal; log_sink_internal"},
        {"log_error_suppression_list", ""},
        {"log_error_verbosity", "2"},
        {"log_output", "FILE"},
        {"log_queries_not_using_indexes", "OFF"},
        {"log_raw", "OFF"},
        {"log_replica_updates", "ON"},
        {"log_slave_updates", "ON"},
        {"log_slow_admin_statements", "OFF"},
        {"log_slow_extra", "OFF"},
        {"log_slow_replica_statements", "OFF"},
        {"log_slow_slave_statements", "OFF"},
        {"log_statements_unsafe_for_binlog", "ON"},
        {"log_throttle_queries_not_using_indexes", "0"},
        {"log_timestamps", "UTC"},
        {"lock_wait_timeout", "31536000"},
        {"long_query_time", "10.000000"},
        {"low_priority_updates", "OFF"},
        {"lower_case_file_system", "OFF"},
        {"lower_case_table_names", "0"},
        {"mandatory_roles", ""},
        {"master_verify_checksum", "OFF"},
        {"max_allowed_packet", "67108864"},
        {"max_binlog_cache_size", "18446744073709547520"},
        {"max_binlog_size", "1073741824"},
        {"max_binlog_stmt_cache_size", "18446744073709547520"},
        {"max_connect_errors", "100"},
        {"max_connections", "151"},
        {"max_delayed_threads", "20"},
        {"max_digest_length", "1024"},
        {"max_error_count", "1024"},
        {"max_execution_time", "0"},
        {"max_heap_table_size", "16777216"},
        {"max_insert_delayed_threads", "20"},
        {"max_join_size", "18446744073709551615"},
        {"max_length_for_sort_data", "4096"},
        {"max_points_in_geometry", "65536"},
        {"max_prepared_stmt_count", "16382"},
        {"max_relay_log_size", "0"},
        {"max_seeks_for_key", "18446744073709551615"},
        {"max_sort_length", "1024"},
        {"max_sp_recursion_depth", "0"},
        {"max_user_connections", "0"},
        {"max_write_lock_count", "18446744073709551615"},
        {"min_examined_row_limit", "0"},
        {"myisam_data_pointer_size", "6"},
        {"myisam_max_sort_file_size", "9223372036853727232"},
        {"myisam_mmap_size", "18446744073709551615"},
        {"myisam_recover_options", "OFF"},
        {"myisam_sort_buffer_size", "8388608"},
        {"myisam_stats_method", "nulls_unequal"},
        {"myisam_use_mmap", "OFF"},
        {"mysql_native_password_proxy_users", "OFF"},
        {"mysqlx_bind_address", "*"},
        {"mysqlx_compression_algorithms", "DEFLATE_STREAM,LZ4_MESSAGE,ZSTD_STREAM"},
        {"mysqlx_connect_timeout", "30"},
        {"mysqlx_deflate_default_compression_level", "3"},
        {"mysqlx_deflate_max_client_compression_level", "5"},
        {"mysqlx_document_id_unique_prefix", "0"},
        {"mysqlx_enable_hello_notice", "ON"},
        {"mysqlx_idle_worker_thread_timeout", "60"},
        {"mysqlx_interactive_timeout", "28800"},
        {"mysqlx_lz4_default_compression_level", "2"},
        {"mysqlx_lz4_max_client_compression_level", "8"},
        {"mysqlx_max_allowed_packet", "67108864"},
        {"mysqlx_max_connections", "100"},
        {"mysqlx_min_worker_threads", "2"},
        {"mysqlx_port", "33060"},
        {"mysqlx_port_open_timeout", "0"},
        {"mysqlx_read_timeout", "30"},
        {"mysqlx_socket", "/var/run/mysqld/mysqlx.sock"},
        {"mysqlx_ssl_ca", ""},
        {"mysqlx_ssl_capath", ""},
        {"mysqlx_ssl_cert", ""},
        {"mysqlx_ssl_cipher", ""},
        {"mysqlx_ssl_crl", ""},
        {"mysqlx_ssl_crlpath", ""},
        {"mysqlx_ssl_key", ""},
        {"mysqlx_wait_timeout", "28800"},
        {"mysqlx_write_timeout", "60"},
        {"mysqlx_zstd_default_compression_level", "3"},
        {"mysqlx_zstd_max_client_compression_level", "11"},
        {"net_read_timeout", "30"},
        {"net_retry_count", "10"},
        {"net_write_timeout", "60"},
        {"ngram_token_size", "2"},
        {"offline_mode", "OFF"},
        {"old_alter_table", "OFF"},
        {"optimizer_prune_level", "1"},
        {"optimizer_search_depth", "62"},
        {"optimizer_switch",
         "index_merge=on,index_merge_union=on,index_merge_sort_union=on,"
         "index_merge_intersection=on,engine_condition_pushdown=on,index_condition_pushdown=on,"
         "mrr=on,mrr_cost_based=on,block_nested_loop=on,batched_key_access=off,"
         "materialization=on,semijoin=on,loosescan=on,firstmatch=on,duplicateweedout=on,"
         "subquery_materialization_cost_based=on,use_index_extensions=on,"
         "condition_fanout_filter=on,derived_merge=on,use_invisible_indexes=off,skip_scan=on,"
         "hash_join=on,subquery_to_derived=off,prefer_ordering_index=on,"
         "hypergraph_optimizer=off,derived_condition_pushdown=on,hash_set_operations=on"},
        {"optimizer_trace", "enabled=off,one_line=off"},
        {"optimizer_trace_features",
         "greedy_search=on,range_optimizer=on,dynamic_range=on,repeated_subselect=on"},
        {"optimizer_trace_limit", "1"},
        {"optimizer_trace_max_mem_size", "1048576"},
        {"optimizer_trace_offset", "-1"},
        {"parser_max_mem_size", "18446744073709551615"},
        {"partial_revokes", "OFF"},
        {"password_history", "0"},
        {"password_require_current", "OFF"},
        {"password_reuse_interval", "0"},
        {"performance_schema", "ON"},
        {"performance_schema_accounts_size", "-1"},
        {"performance_schema_digests_size", "10000"},
        {"performance_schema_error_size", "5556"},
        {"performance_schema_events_stages_history_long_size", "10000"},
        {"performance_schema_events_stages_history_size", "10"},
        {"performance_schema_events_statements_history_long_size", "10000"},
        {"performance_schema_events_statements_history_size", "10"},
        {"performance_schema_events_transactions_history_long_size", "10000"},
        {"performance_schema_events_transactions_history_size", "10"},
        {"performance_schema_events_waits_history_long_size", "10000"},
        {"performance_schema_events_waits_history_size", "10"},
        {"performance_schema_hosts_size", "-1"},
        {"performance_schema_max_cond_classes", "150"},
        {"performance_schema_max_cond_instances", "-1"},
        {"performance_schema_max_digest_length", "1024"},
        {"performance_schema_max_digest_sample_age", "60"},
        {"performance_schema_max_file_classes", "80"},
        {"performance_schema_max_file_handles", "32768"},
        {"performance_schema_max_file_instances", "-1"},
        {"performance_schema_max_index_stat", "-1"},
        {"performance_schema_max_memory_classes", "470"},
        {"performance_schema_max_metadata_locks", "-1"},
        {"performance_schema_max_meter_classes", "30"},
        {"performance_schema_max_metric_classes", "600"},
        {"performance_schema_max_mutex_classes", "350"},
        {"performance_schema_max_mutex_instances", "-1"},
        {"performance_schema_max_prepared_statements_instances", "-1"},
        {"performance_schema_max_program_instances", "-1"},
        {"performance_schema_max_rwlock_classes", "100"},
        {"performance_schema_max_rwlock_instances", "-1"},
        {"performance_schema_max_socket_classes", "10"},
        {"performance_schema_max_socket_instances", "-1"},
        {"performance_schema_max_sql_text_length", "1024"},
        {"performance_schema_max_stage_classes", "175"},
        {"performance_schema_max_statement_classes", "220"},
        {"performance_schema_max_statement_stack", "10"},
        {"performance_schema_max_table_handles", "-1"},
        {"performance_schema_max_table_instances", "-1"},
        {"performance_schema_max_table_lock_stat", "-1"},
        {"performance_schema_max_thread_classes", "100"},
        {"performance_schema_max_thread_instances", "-1"},
        {"performance_schema_session_connect_attrs_size", "512"},
        {"performance_schema_setup_actors_size", "-1"},
        {"performance_schema_setup_objects_size", "-1"},
        {"performance_schema_show_processlist", "OFF"},
        {"performance_schema_users_size", "-1"},
        {"persist_only_admin_x509_subject", ""},
        {"persist_sensitive_variables_in_plaintext", "ON"},
        {"persisted_globals_load", "ON"},
        {"pid_file", "/var/run/mysqld/mysqld.pid"},
        {"plugin_dir", "/usr/lib64/mysql/plugin/"},
        {"port", "3306"},
        {"print_identified_with_as_hex", "OFF"},
        {"protocol_compression_algorithms", "zlib,zstd,uncompressed"},
        {"protocol_version", "10"},
        {"read_only", "OFF"},
        {"relay_log", "mylite-relay-bin"},
        {"relay_log_basename", "/var/lib/mysql/mylite-relay-bin"},
        {"relay_log_index", "/var/lib/mysql/mylite-relay-bin.index"},
        {"relay_log_purge", "ON"},
        {"relay_log_recovery", "OFF"},
        {"relay_log_space_limit", "0"},
        {"replication_optimize_for_static_plugin_config", "OFF"},
        {"replication_sender_observe_commit_only", "OFF"},
        {"replica_allow_batching", "ON"},
        {"replica_checkpoint_group", "512"},
        {"replica_checkpoint_period", "300"},
        {"replica_compressed_protocol", "OFF"},
        {"replica_exec_mode", "STRICT"},
        {"replica_load_tmpdir", "/tmp"},
        {"replica_max_allowed_packet", "1073741824"},
        {"replica_net_timeout", "60"},
        {"replica_parallel_type", "LOGICAL_CLOCK"},
        {"replica_parallel_workers", "4"},
        {"replica_pending_jobs_size_max", "134217728"},
        {"replica_preserve_commit_order", "ON"},
        {"replica_skip_errors", "OFF"},
        {"replica_sql_verify_checksum", "ON"},
        {"replica_transaction_retries", "10"},
        {"replica_type_conversions", ""},
        {"report_host", ""},
        {"report_password", ""},
        {"report_port", "3306"},
        {"report_user", ""},
        {"require_row_format", "OFF"},
        {"require_secure_transport", "OFF"},
        {"resultset_metadata", "FULL"},
        {"rpl_read_size", "8192"},
        {"rpl_stop_replica_timeout", "31536000"},
        {"rpl_stop_slave_timeout", "31536000"},
        {"schema_definition_cache", "256"},
        {"secure_file_priv", "/var/lib/mysql-files/"},
        {"select_into_disk_sync", "OFF"},
        {"server_id", "1"},
        {"server_id_bits", "32"},
        {"server_uuid", "4d796c69-7465-4000-8000-000000000001"},
        {"socket", "/var/run/mysqld/mysqld.sock"},
        {"session_track_gtids", "OFF"},
        {"session_track_schema", "ON"},
        {"session_track_state_change", "OFF"},
        {"session_track_transaction_info", "OFF"},
        {"sha256_password_auto_generate_rsa_keys", "ON"},
        {"sha256_password_private_key_path", "private_key.pem"},
        {"sha256_password_proxy_users", "OFF"},
        {"sha256_password_public_key_path", "public_key.pem"},
        {"show_create_table_skip_secondary_engine", "OFF"},
        {"show_create_table_verbosity", "OFF"},
        {"skip_external_locking", "ON"},
        {"skip_name_resolve", "ON"},
        {"skip_networking", "OFF"},
        {"skip_replica_start", "OFF"},
        {"skip_show_database", "OFF"},
        {"skip_slave_start", "OFF"},
        {"slave_allow_batching", "ON"},
        {"slave_checkpoint_group", "512"},
        {"slave_checkpoint_period", "300"},
        {"slave_compressed_protocol", "OFF"},
        {"slave_exec_mode", "STRICT"},
        {"slave_load_tmpdir", "/tmp"},
        {"slave_max_allowed_packet", "1073741824"},
        {"slave_net_timeout", "60"},
        {"slave_parallel_type", "LOGICAL_CLOCK"},
        {"slave_parallel_workers", "4"},
        {"slave_pending_jobs_size_max", "134217728"},
        {"slave_preserve_commit_order", "ON"},
        {"slave_skip_errors", "OFF"},
        {"slave_sql_verify_checksum", "ON"},
        {"slave_transaction_retries", "10"},
        {"slave_type_conversions", ""},
        {"sql_auto_is_null", "OFF"},
        {"sql_big_selects", "ON"},
        {"sql_buffer_result", "OFF"},
        {"sql_generate_invisible_primary_key", "OFF"},
        {"sql_log_bin", "ON"},
        {"sql_log_off", "OFF"},
        {"sql_mode", default_sql_mode},
        {"sql_notes", "ON"},
        {"sql_quote_show_create", "ON"},
        {"sql_replica_skip_counter", "0"},
        {"sql_require_primary_key", "OFF"},
        {"sql_safe_updates", "OFF"},
        {"sql_select_limit", "18446744073709551615"},
        {"sql_slave_skip_counter", "0"},
        {"sql_warnings", "OFF"},
        {"ssl_ca", ""},
        {"ssl_capath", ""},
        {"ssl_cert", ""},
        {"ssl_cipher", ""},
        {"ssl_crl", ""},
        {"ssl_crlpath", ""},
        {"ssl_fips_mode", "OFF"},
        {"ssl_key", ""},
        {"ssl_session_cache_mode", "ON"},
        {"ssl_session_cache_timeout", "300"},
        {"slow_launch_time", "2"},
        {"slow_query_log", "OFF"},
        {"slow_query_log_file", "/var/lib/mysql/mylite-slow.log"},
        {"sort_buffer_size", "262144"},
        {"source_verify_checksum", "OFF"},
        {"stored_program_cache", "256"},
        {"stored_program_definition_cache", "256"},
        {"super_read_only", "OFF"},
        {"sync_binlog", "1"},
        {"sync_master_info", "10000"},
        {"sync_relay_log", "10000"},
        {"sync_relay_log_info", "10000"},
        {"sync_source_info", "10000"},
        {"system_time_zone", "UTC"},
        {"table_definition_cache", "2000"},
        {"table_encryption_privilege_check", "OFF"},
        {"table_open_cache", "4000"},
        {"table_open_cache_instances", "16"},
        {"tablespace_definition_cache", "256"},
        {"temptable_max_mmap", "0"},
        {"temptable_use_mmap", "OFF"},
        {"thread_cache_size", "9"},
        {"thread_handling", "one-thread-per-connection"},
        {"thread_stack", "1048576"},
        {"timestamp", "1700000000.000000"},
        {"time_zone", "SYSTEM"},
        {"tls_certificates_enforced_validation", "OFF"},
        {"tls_ciphersuites", ""},
        {"tls_version", "TLSv1.2,TLSv1.3"},
        {"tmpdir", "/tmp"},
        {"transaction_isolation", "REPEATABLE-READ"},
        {"transaction_read_only", "OFF"},
        {"unique_checks", "ON"},
        {"updatable_views_with_limit", "YES"},
        {"use_secondary_engine", "ON"},
        {"version", MYLITE_MYSQL_SERVER_VERSION_STRING},
        {"version_comment", MYLITE_MYSQL_SERVER_VERSION_COMMENT_STRING},
        {"version_compile_machine", "aarch64"},
        {"version_compile_os", "Linux"},
        {"version_compile_zlib", "1.3.2"},
        {"wait_timeout", "28800"},
        {"warning_count", "0"},
    };
    const char *const expected_global_rows[global_variable_row_count][variable_column_count] = {
        {"activate_all_roles_on_login", "OFF"},
        {"admin_address", ""},
        {"admin_port", "33062"},
        {"admin_ssl_ca", ""},
        {"admin_ssl_capath", ""},
        {"admin_ssl_cert", ""},
        {"admin_ssl_cipher", ""},
        {"admin_ssl_crl", ""},
        {"admin_ssl_crlpath", ""},
        {"admin_ssl_key", ""},
        {"admin_tls_ciphersuites", ""},
        {"admin_tls_version", "TLSv1.2,TLSv1.3"},
        {"auto_increment_increment", "1"},
        {"auto_increment_offset", "1"},
        {"auto_generate_certs", "ON"},
        {"automatic_sp_privileges", "ON"},
        {"authentication_policy", "*,,"},
        {"autocommit", "ON"},
        {"back_log", "151"},
        {"basedir", "/usr/"},
        {"big_tables", "OFF"},
        {"bind_address", "*"},
        {"binlog_cache_size", "32768"},
        {"binlog_checksum", "CRC32"},
        {"binlog_direct_non_transactional_updates", "OFF"},
        {"binlog_encryption", "OFF"},
        {"binlog_error_action", "ABORT_SERVER"},
        {"binlog_expire_logs_auto_purge", "ON"},
        {"binlog_expire_logs_seconds", "2592000"},
        {"binlog_format", "ROW"},
        {"binlog_group_commit_sync_delay", "0"},
        {"binlog_group_commit_sync_no_delay_count", "0"},
        {"binlog_gtid_simple_recovery", "ON"},
        {"binlog_max_flush_queue_time", "0"},
        {"binlog_order_commits", "ON"},
        {"binlog_rotate_encryption_master_key_at_startup", "OFF"},
        {"binlog_row_event_max_size", "8192"},
        {"binlog_row_image", "FULL"},
        {"binlog_row_metadata", "MINIMAL"},
        {"binlog_row_value_options", ""},
        {"binlog_rows_query_log_events", "OFF"},
        {"binlog_stmt_cache_size", "32768"},
        {"binlog_transaction_compression", "OFF"},
        {"binlog_transaction_compression_level_zstd", "3"},
        {"binlog_transaction_dependency_history_size", "25000"},
        {"block_encryption_mode", "aes-128-ecb"},
        {"build_id", "66e221b3840955d27f740799b5b2c6eb0baf3283"},
        {"bulk_insert_buffer_size", "8388608"},
        {"caching_sha2_password_auto_generate_rsa_keys", "ON"},
        {"caching_sha2_password_digest_rounds", "5000"},
        {"caching_sha2_password_private_key_path", "private_key.pem"},
        {"caching_sha2_password_public_key_path", "public_key.pem"},
        {"character_set_client", "utf8mb4"},
        {"character_set_connection", "utf8mb4"},
        {"character_set_database", "utf8mb4"},
        {"character_set_filesystem", "binary"},
        {"character_set_results", "utf8mb4"},
        {"character_set_server", "utf8mb4"},
        {"character_set_system", "utf8mb3"},
        {"character_sets_dir", "/usr/share/mysql-8.4/charsets/"},
        {"check_proxy_users", "OFF"},
        {"collation_connection", "utf8mb4_0900_ai_ci"},
        {"collation_database", "utf8mb4_0900_ai_ci"},
        {"collation_server", "utf8mb4_0900_ai_ci"},
        {"completion_type", "NO_CHAIN"},
        {"concurrent_insert", "AUTO"},
        {"connect_timeout", "10"},
        {"connection_control_failed_connections_threshold", "3"},
        {"connection_control_max_connection_delay", "2147483647"},
        {"connection_control_min_connection_delay", "1000"},
        {"connection_memory_chunk_size", "8192"},
        {"connection_memory_limit", "18446744073709551615"},
        {"core_file", "OFF"},
        {"create_admin_listener_thread", "OFF"},
        {"cte_max_recursion_depth", "1000"},
        {"datadir", "/var/lib/mysql/"},
        {"default_collation_for_utf8mb4", "utf8mb4_0900_ai_ci"},
        {"default_password_lifetime", "0"},
        {"default_storage_engine", "InnoDB"},
        {"default_table_encryption", "OFF"},
        {"default_tmp_storage_engine", "InnoDB"},
        {"default_week_format", "0"},
        {"delay_key_write", "ON"},
        {"delayed_insert_limit", "100"},
        {"delayed_insert_timeout", "300"},
        {"delayed_queue_size", "1000"},
        {"disconnect_on_expired_password", "ON"},
        {"disabled_storage_engines", ""},
        {"div_precision_increment", "4"},
        {"end_markers_in_json", "OFF"},
        {"enforce_gtid_consistency", "OFF"},
        {"eq_range_index_dive_limit", "200"},
        {"event_scheduler", "ON"},
        {"explain_format", "TRADITIONAL"},
        {"explain_json_format_version", "1"},
        {"explicit_defaults_for_timestamp", "ON"},
        {"flush", "OFF"},
        {"flush_time", "0"},
        {"foreign_key_checks", "ON"},
        {"ft_boolean_syntax", "+ -><()~*:\"\"&|"},
        {"ft_max_word_len", "84"},
        {"ft_min_word_len", "4"},
        {"ft_query_expansion_limit", "20"},
        {"ft_stopword_file", "(built-in)"},
        {"generated_random_password_length", "20"},
        {"general_log", "OFF"},
        {"general_log_file", "/var/lib/mysql/mylite.log"},
        {"group_concat_max_len", "1024"},
        {"group_replication_consistency", "BEFORE_ON_PRIMARY_FAILOVER"},
        {"gtid_executed", ""},
        {"gtid_executed_compression_period", "0"},
        {"gtid_mode", "OFF"},
        {"gtid_owned", ""},
        {"gtid_purged", ""},
        {"global_connection_memory_limit", "18446744073709551615"},
        {"global_connection_memory_tracking", "OFF"},
        {"have_compress", "YES"},
        {"have_dynamic_loading", "YES"},
        {"have_geometry", "YES"},
        {"have_profiling", "YES"},
        {"have_query_cache", "NO"},
        {"have_rtree_keys", "YES"},
        {"have_statement_timeout", "YES"},
        {"have_symlink", "DISABLED"},
        {"histogram_generation_max_mem_size", "20000000"},
        {"host_cache_size", "0"},
        {"hostname", "mylite"},
        {"information_schema_stats_expiry", "86400"},
        {"init_connect", ""},
        {"init_file", ""},
        {"init_replica", ""},
        {"init_slave", ""},
        {"innodb_adaptive_flushing", "ON"},
        {"innodb_adaptive_flushing_lwm", "10"},
        {"innodb_adaptive_hash_index", "OFF"},
        {"innodb_adaptive_hash_index_parts", "8"},
        {"innodb_adaptive_max_sleep_delay", "150000"},
        {"innodb_autoextend_increment", "64"},
        {"innodb_autoinc_lock_mode", "2"},
        {"innodb_buffer_pool_chunk_size", "134217728"},
        {"innodb_buffer_pool_dump_at_shutdown", "ON"},
        {"innodb_buffer_pool_dump_now", "OFF"},
        {"innodb_buffer_pool_dump_pct", "25"},
        {"innodb_buffer_pool_filename", "ib_buffer_pool"},
        {"innodb_buffer_pool_in_core_file", "OFF"},
        {"innodb_buffer_pool_instances", "1"},
        {"innodb_buffer_pool_load_abort", "OFF"},
        {"innodb_buffer_pool_load_at_startup", "ON"},
        {"innodb_buffer_pool_load_now", "OFF"},
        {"innodb_buffer_pool_size", "134217728"},
        {"innodb_change_buffer_max_size", "25"},
        {"innodb_change_buffering", "none"},
        {"innodb_checksum_algorithm", "crc32"},
        {"innodb_cmp_per_index_enabled", "OFF"},
        {"innodb_commit_concurrency", "0"},
        {"innodb_compression_failure_threshold_pct", "5"},
        {"innodb_compression_level", "6"},
        {"innodb_compression_pad_pct_max", "50"},
        {"innodb_concurrency_tickets", "5000"},
        {"innodb_data_file_path", "ibdata1:12M:autoextend"},
        {"innodb_data_home_dir", ""},
        {"innodb_ddl_buffer_size", "1048576"},
        {"innodb_ddl_threads", "4"},
        {"innodb_deadlock_detect", "ON"},
        {"innodb_dedicated_server", "OFF"},
        {"innodb_default_row_format", "dynamic"},
        {"innodb_directories", ""},
        {"innodb_disable_sort_file_cache", "OFF"},
        {"innodb_doublewrite", "ON"},
        {"innodb_doublewrite_batch_size", "0"},
        {"innodb_doublewrite_dir", ""},
        {"innodb_doublewrite_files", "2"},
        {"innodb_doublewrite_pages", "128"},
        {"innodb_extend_and_initialize", "ON"},
        {"innodb_fast_shutdown", "1"},
        {"innodb_file_per_table", "ON"},
        {"innodb_fill_factor", "100"},
        {"innodb_flush_log_at_timeout", "1"},
        {"innodb_flush_log_at_trx_commit", "1"},
        {"innodb_flush_method", "O_DIRECT"},
        {"innodb_flush_neighbors", "0"},
        {"innodb_flush_sync", "ON"},
        {"innodb_flushing_avg_loops", "30"},
        {"innodb_force_load_corrupted", "OFF"},
        {"innodb_force_recovery", "0"},
        {"innodb_fsync_threshold", "0"},
        {"innodb_ft_aux_table", ""},
        {"innodb_ft_cache_size", "8000000"},
        {"innodb_ft_enable_diag_print", "OFF"},
        {"innodb_ft_enable_stopword", "ON"},
        {"innodb_ft_max_token_size", "84"},
        {"innodb_ft_min_token_size", "3"},
        {"innodb_ft_num_word_optimize", "2000"},
        {"innodb_ft_result_cache_limit", "2000000000"},
        {"innodb_ft_server_stopword_table", ""},
        {"innodb_ft_sort_pll_degree", "2"},
        {"innodb_ft_total_cache_size", "640000000"},
        {"innodb_ft_user_stopword_table", ""},
        {"innodb_idle_flush_pct", "100"},
        {"innodb_io_capacity", "10000"},
        {"innodb_io_capacity_max", "20000"},
        {"innodb_lock_wait_timeout", "50"},
        {"innodb_log_buffer_size", "67108864"},
        {"innodb_log_checksums", "ON"},
        {"innodb_log_compressed_pages", "ON"},
        {"innodb_log_file_size", "50331648"},
        {"innodb_log_files_in_group", "2"},
        {"innodb_log_group_home_dir", "./"},
        {"innodb_log_spin_cpu_abs_lwm", "80"},
        {"innodb_log_spin_cpu_pct_hwm", "50"},
        {"innodb_log_wait_for_flush_spin_hwm", "400"},
        {"innodb_log_write_ahead_size", "8192"},
        {"innodb_log_writer_threads", "ON"},
        {"innodb_lru_scan_depth", "1024"},
        {"innodb_max_dirty_pages_pct", "90.000000"},
        {"innodb_max_dirty_pages_pct_lwm", "10.000000"},
        {"innodb_max_purge_lag", "0"},
        {"innodb_max_purge_lag_delay", "0"},
        {"innodb_max_undo_log_size", "1073741824"},
        {"innodb_monitor_disable", ""},
        {"innodb_monitor_enable", ""},
        {"innodb_monitor_reset", ""},
        {"innodb_monitor_reset_all", ""},
        {"innodb_old_blocks_pct", "37"},
        {"innodb_old_blocks_time", "1000"},
        {"innodb_online_alter_log_max_size", "134217728"},
        {"innodb_open_files", "4000"},
        {"innodb_optimize_fulltext_only", "OFF"},
        {"innodb_page_cleaners", "1"},
        {"innodb_page_size", "16384"},
        {"innodb_parallel_read_threads", "4"},
        {"innodb_print_all_deadlocks", "OFF"},
        {"innodb_print_ddl_logs", "OFF"},
        {"innodb_purge_batch_size", "300"},
        {"innodb_purge_rseg_truncate_frequency", "128"},
        {"innodb_purge_threads", "4"},
        {"innodb_random_read_ahead", "OFF"},
        {"innodb_read_ahead_threshold", "56"},
        {"innodb_read_io_threads", "9"},
        {"innodb_read_only", "OFF"},
        {"innodb_redo_log_archive_dirs", ""},
        {"innodb_redo_log_capacity", "104857600"},
        {"innodb_redo_log_encrypt", "OFF"},
        {"innodb_replication_delay", "0"},
        {"innodb_rollback_on_timeout", "OFF"},
        {"innodb_rollback_segments", "128"},
        {"innodb_segment_reserve_factor", "12.500000"},
        {"innodb_sort_buffer_size", "1048576"},
        {"innodb_spin_wait_delay", "6"},
        {"innodb_spin_wait_pause_multiplier", "50"},
        {"innodb_stats_auto_recalc", "ON"},
        {"innodb_stats_include_delete_marked", "OFF"},
        {"innodb_stats_method", "nulls_equal"},
        {"innodb_stats_on_metadata", "OFF"},
        {"innodb_stats_persistent", "ON"},
        {"innodb_stats_persistent_sample_pages", "20"},
        {"innodb_stats_transient_sample_pages", "8"},
        {"innodb_status_output", "OFF"},
        {"innodb_status_output_locks", "OFF"},
        {"innodb_strict_mode", "ON"},
        {"innodb_sync_array_size", "1"},
        {"innodb_sync_spin_loops", "30"},
        {"innodb_table_locks", "ON"},
        {"innodb_temp_data_file_path", "ibtmp1:12M:autoextend"},
        {"innodb_temp_tablespaces_dir", "./#innodb_temp/"},
        {"innodb_thread_concurrency", "0"},
        {"innodb_thread_sleep_delay", "10000"},
        {"innodb_tmpdir", ""},
        {"innodb_undo_directory", "./"},
        {"innodb_undo_log_encrypt", "OFF"},
        {"innodb_undo_log_truncate", "ON"},
        {"innodb_undo_tablespaces", "2"},
        {"innodb_use_fdatasync", "ON"},
        {"innodb_use_native_aio", "ON"},
        {"innodb_validate_tablespace_paths", "ON"},
        {"innodb_version", "8.4.9"},
        {"innodb_write_io_threads", "4"},
        {"internal_tmp_mem_storage_engine", "TempTable"},
        {"interactive_timeout", "28800"},
        {"join_buffer_size", "262144"},
        {"keep_files_on_create", "OFF"},
        {"key_buffer_size", "8388608"},
        {"key_cache_age_threshold", "300"},
        {"key_cache_block_size", "1024"},
        {"key_cache_division_limit", "100"},
        {"keyring_operations", "ON"},
        {"large_files_support", "ON"},
        {"large_page_size", "0"},
        {"large_pages", "OFF"},
        {"lc_messages", "en_US"},
        {"lc_messages_dir", "/usr/share/mysql-8.4/"},
        {"license", "GPL"},
        {"local_infile", "OFF"},
        {"locked_in_memory", "OFF"},
        {"log_bin", "ON"},
        {"log_bin_basename", "binlog"},
        {"log_bin_index", "binlog.index"},
        {"log_bin_trust_function_creators", "OFF"},
        {"log_error", "stderr"},
        {"log_error_services", "log_filter_internal; log_sink_internal"},
        {"log_error_suppression_list", ""},
        {"log_error_verbosity", "2"},
        {"log_output", "FILE"},
        {"log_queries_not_using_indexes", "OFF"},
        {"log_raw", "OFF"},
        {"log_replica_updates", "ON"},
        {"log_slave_updates", "ON"},
        {"log_slow_admin_statements", "OFF"},
        {"log_slow_extra", "OFF"},
        {"log_slow_replica_statements", "OFF"},
        {"log_slow_slave_statements", "OFF"},
        {"log_statements_unsafe_for_binlog", "ON"},
        {"log_throttle_queries_not_using_indexes", "0"},
        {"log_timestamps", "UTC"},
        {"lock_wait_timeout", "31536000"},
        {"long_query_time", "10.000000"},
        {"low_priority_updates", "OFF"},
        {"lower_case_file_system", "OFF"},
        {"lower_case_table_names", "0"},
        {"mandatory_roles", ""},
        {"master_verify_checksum", "OFF"},
        {"max_allowed_packet", "67108864"},
        {"max_binlog_cache_size", "18446744073709547520"},
        {"max_binlog_size", "1073741824"},
        {"max_binlog_stmt_cache_size", "18446744073709547520"},
        {"max_connect_errors", "100"},
        {"max_connections", "151"},
        {"max_delayed_threads", "20"},
        {"max_digest_length", "1024"},
        {"max_error_count", "1024"},
        {"max_execution_time", "0"},
        {"max_heap_table_size", "16777216"},
        {"max_insert_delayed_threads", "20"},
        {"max_join_size", "18446744073709551615"},
        {"max_length_for_sort_data", "4096"},
        {"max_points_in_geometry", "65536"},
        {"max_prepared_stmt_count", "16382"},
        {"max_relay_log_size", "0"},
        {"max_seeks_for_key", "18446744073709551615"},
        {"max_sort_length", "1024"},
        {"max_sp_recursion_depth", "0"},
        {"max_user_connections", "0"},
        {"max_write_lock_count", "18446744073709551615"},
        {"min_examined_row_limit", "0"},
        {"myisam_data_pointer_size", "6"},
        {"myisam_max_sort_file_size", "9223372036853727232"},
        {"myisam_mmap_size", "18446744073709551615"},
        {"myisam_recover_options", "OFF"},
        {"myisam_sort_buffer_size", "8388608"},
        {"myisam_stats_method", "nulls_unequal"},
        {"myisam_use_mmap", "OFF"},
        {"mysql_native_password_proxy_users", "OFF"},
        {"mysqlx_bind_address", "*"},
        {"mysqlx_compression_algorithms", "DEFLATE_STREAM,LZ4_MESSAGE,ZSTD_STREAM"},
        {"mysqlx_connect_timeout", "30"},
        {"mysqlx_deflate_default_compression_level", "3"},
        {"mysqlx_deflate_max_client_compression_level", "5"},
        {"mysqlx_document_id_unique_prefix", "0"},
        {"mysqlx_enable_hello_notice", "ON"},
        {"mysqlx_idle_worker_thread_timeout", "60"},
        {"mysqlx_interactive_timeout", "28800"},
        {"mysqlx_lz4_default_compression_level", "2"},
        {"mysqlx_lz4_max_client_compression_level", "8"},
        {"mysqlx_max_allowed_packet", "67108864"},
        {"mysqlx_max_connections", "100"},
        {"mysqlx_min_worker_threads", "2"},
        {"mysqlx_port", "33060"},
        {"mysqlx_port_open_timeout", "0"},
        {"mysqlx_read_timeout", "30"},
        {"mysqlx_socket", "/var/run/mysqld/mysqlx.sock"},
        {"mysqlx_ssl_ca", ""},
        {"mysqlx_ssl_capath", ""},
        {"mysqlx_ssl_cert", ""},
        {"mysqlx_ssl_cipher", ""},
        {"mysqlx_ssl_crl", ""},
        {"mysqlx_ssl_crlpath", ""},
        {"mysqlx_ssl_key", ""},
        {"mysqlx_wait_timeout", "28800"},
        {"mysqlx_write_timeout", "60"},
        {"mysqlx_zstd_default_compression_level", "3"},
        {"mysqlx_zstd_max_client_compression_level", "11"},
        {"net_read_timeout", "30"},
        {"net_retry_count", "10"},
        {"net_write_timeout", "60"},
        {"ngram_token_size", "2"},
        {"offline_mode", "OFF"},
        {"old_alter_table", "OFF"},
        {"optimizer_prune_level", "1"},
        {"optimizer_search_depth", "62"},
        {"optimizer_switch",
         "index_merge=on,index_merge_union=on,index_merge_sort_union=on,"
         "index_merge_intersection=on,engine_condition_pushdown=on,index_condition_pushdown=on,"
         "mrr=on,mrr_cost_based=on,block_nested_loop=on,batched_key_access=off,"
         "materialization=on,semijoin=on,loosescan=on,firstmatch=on,duplicateweedout=on,"
         "subquery_materialization_cost_based=on,use_index_extensions=on,"
         "condition_fanout_filter=on,derived_merge=on,use_invisible_indexes=off,skip_scan=on,"
         "hash_join=on,subquery_to_derived=off,prefer_ordering_index=on,"
         "hypergraph_optimizer=off,derived_condition_pushdown=on,hash_set_operations=on"},
        {"optimizer_trace", "enabled=off,one_line=off"},
        {"optimizer_trace_features",
         "greedy_search=on,range_optimizer=on,dynamic_range=on,repeated_subselect=on"},
        {"optimizer_trace_limit", "1"},
        {"optimizer_trace_max_mem_size", "1048576"},
        {"optimizer_trace_offset", "-1"},
        {"parser_max_mem_size", "18446744073709551615"},
        {"partial_revokes", "OFF"},
        {"password_history", "0"},
        {"password_require_current", "OFF"},
        {"password_reuse_interval", "0"},
        {"performance_schema", "ON"},
        {"performance_schema_accounts_size", "-1"},
        {"performance_schema_digests_size", "10000"},
        {"performance_schema_error_size", "5556"},
        {"performance_schema_events_stages_history_long_size", "10000"},
        {"performance_schema_events_stages_history_size", "10"},
        {"performance_schema_events_statements_history_long_size", "10000"},
        {"performance_schema_events_statements_history_size", "10"},
        {"performance_schema_events_transactions_history_long_size", "10000"},
        {"performance_schema_events_transactions_history_size", "10"},
        {"performance_schema_events_waits_history_long_size", "10000"},
        {"performance_schema_events_waits_history_size", "10"},
        {"performance_schema_hosts_size", "-1"},
        {"performance_schema_max_cond_classes", "150"},
        {"performance_schema_max_cond_instances", "-1"},
        {"performance_schema_max_digest_length", "1024"},
        {"performance_schema_max_digest_sample_age", "60"},
        {"performance_schema_max_file_classes", "80"},
        {"performance_schema_max_file_handles", "32768"},
        {"performance_schema_max_file_instances", "-1"},
        {"performance_schema_max_index_stat", "-1"},
        {"performance_schema_max_memory_classes", "470"},
        {"performance_schema_max_metadata_locks", "-1"},
        {"performance_schema_max_meter_classes", "30"},
        {"performance_schema_max_metric_classes", "600"},
        {"performance_schema_max_mutex_classes", "350"},
        {"performance_schema_max_mutex_instances", "-1"},
        {"performance_schema_max_prepared_statements_instances", "-1"},
        {"performance_schema_max_program_instances", "-1"},
        {"performance_schema_max_rwlock_classes", "100"},
        {"performance_schema_max_rwlock_instances", "-1"},
        {"performance_schema_max_socket_classes", "10"},
        {"performance_schema_max_socket_instances", "-1"},
        {"performance_schema_max_sql_text_length", "1024"},
        {"performance_schema_max_stage_classes", "175"},
        {"performance_schema_max_statement_classes", "220"},
        {"performance_schema_max_statement_stack", "10"},
        {"performance_schema_max_table_handles", "-1"},
        {"performance_schema_max_table_instances", "-1"},
        {"performance_schema_max_table_lock_stat", "-1"},
        {"performance_schema_max_thread_classes", "100"},
        {"performance_schema_max_thread_instances", "-1"},
        {"performance_schema_session_connect_attrs_size", "512"},
        {"performance_schema_setup_actors_size", "-1"},
        {"performance_schema_setup_objects_size", "-1"},
        {"performance_schema_show_processlist", "OFF"},
        {"performance_schema_users_size", "-1"},
        {"persist_only_admin_x509_subject", ""},
        {"persist_sensitive_variables_in_plaintext", "ON"},
        {"persisted_globals_load", "ON"},
        {"pid_file", "/var/run/mysqld/mysqld.pid"},
        {"plugin_dir", "/usr/lib64/mysql/plugin/"},
        {"port", "3306"},
        {"print_identified_with_as_hex", "OFF"},
        {"protocol_compression_algorithms", "zlib,zstd,uncompressed"},
        {"protocol_version", "10"},
        {"read_only", "OFF"},
        {"relay_log", "mylite-relay-bin"},
        {"relay_log_basename", "/var/lib/mysql/mylite-relay-bin"},
        {"relay_log_index", "/var/lib/mysql/mylite-relay-bin.index"},
        {"relay_log_purge", "ON"},
        {"relay_log_recovery", "OFF"},
        {"relay_log_space_limit", "0"},
        {"replication_optimize_for_static_plugin_config", "OFF"},
        {"replication_sender_observe_commit_only", "OFF"},
        {"replica_allow_batching", "ON"},
        {"replica_checkpoint_group", "512"},
        {"replica_checkpoint_period", "300"},
        {"replica_compressed_protocol", "OFF"},
        {"replica_exec_mode", "STRICT"},
        {"replica_load_tmpdir", "/tmp"},
        {"replica_max_allowed_packet", "1073741824"},
        {"replica_net_timeout", "60"},
        {"replica_parallel_type", "LOGICAL_CLOCK"},
        {"replica_parallel_workers", "4"},
        {"replica_pending_jobs_size_max", "134217728"},
        {"replica_preserve_commit_order", "ON"},
        {"replica_skip_errors", "OFF"},
        {"replica_sql_verify_checksum", "ON"},
        {"replica_transaction_retries", "10"},
        {"replica_type_conversions", ""},
        {"report_host", ""},
        {"report_password", ""},
        {"report_port", "3306"},
        {"report_user", ""},
        {"require_secure_transport", "OFF"},
        {"rpl_read_size", "8192"},
        {"rpl_stop_replica_timeout", "31536000"},
        {"rpl_stop_slave_timeout", "31536000"},
        {"schema_definition_cache", "256"},
        {"secure_file_priv", "/var/lib/mysql-files/"},
        {"select_into_disk_sync", "OFF"},
        {"server_id", "1"},
        {"server_id_bits", "32"},
        {"server_uuid", "4d796c69-7465-4000-8000-000000000001"},
        {"socket", "/var/run/mysqld/mysqld.sock"},
        {"session_track_gtids", "OFF"},
        {"session_track_schema", "ON"},
        {"session_track_state_change", "OFF"},
        {"session_track_transaction_info", "OFF"},
        {"sha256_password_auto_generate_rsa_keys", "ON"},
        {"sha256_password_private_key_path", "private_key.pem"},
        {"sha256_password_proxy_users", "OFF"},
        {"sha256_password_public_key_path", "public_key.pem"},
        {"show_create_table_verbosity", "OFF"},
        {"skip_external_locking", "ON"},
        {"skip_name_resolve", "ON"},
        {"skip_networking", "OFF"},
        {"skip_replica_start", "OFF"},
        {"skip_show_database", "OFF"},
        {"skip_slave_start", "OFF"},
        {"slave_allow_batching", "ON"},
        {"slave_checkpoint_group", "512"},
        {"slave_checkpoint_period", "300"},
        {"slave_compressed_protocol", "OFF"},
        {"slave_exec_mode", "STRICT"},
        {"slave_load_tmpdir", "/tmp"},
        {"slave_max_allowed_packet", "1073741824"},
        {"slave_net_timeout", "60"},
        {"slave_parallel_type", "LOGICAL_CLOCK"},
        {"slave_parallel_workers", "4"},
        {"slave_pending_jobs_size_max", "134217728"},
        {"slave_preserve_commit_order", "ON"},
        {"slave_skip_errors", "OFF"},
        {"slave_sql_verify_checksum", "ON"},
        {"slave_transaction_retries", "10"},
        {"slave_type_conversions", ""},
        {"sql_auto_is_null", "OFF"},
        {"sql_big_selects", "ON"},
        {"sql_buffer_result", "OFF"},
        {"sql_generate_invisible_primary_key", "OFF"},
        {"sql_log_off", "OFF"},
        {"sql_mode", default_sql_mode},
        {"sql_notes", "ON"},
        {"sql_quote_show_create", "ON"},
        {"sql_replica_skip_counter", "0"},
        {"sql_require_primary_key", "OFF"},
        {"sql_safe_updates", "OFF"},
        {"sql_select_limit", "18446744073709551615"},
        {"sql_slave_skip_counter", "0"},
        {"sql_warnings", "OFF"},
        {"ssl_ca", ""},
        {"ssl_capath", ""},
        {"ssl_cert", ""},
        {"ssl_cipher", ""},
        {"ssl_crl", ""},
        {"ssl_crlpath", ""},
        {"ssl_fips_mode", "OFF"},
        {"ssl_key", ""},
        {"ssl_session_cache_mode", "ON"},
        {"ssl_session_cache_timeout", "300"},
        {"slow_launch_time", "2"},
        {"slow_query_log", "OFF"},
        {"slow_query_log_file", "/var/lib/mysql/mylite-slow.log"},
        {"sort_buffer_size", "262144"},
        {"source_verify_checksum", "OFF"},
        {"stored_program_cache", "256"},
        {"stored_program_definition_cache", "256"},
        {"super_read_only", "OFF"},
        {"sync_binlog", "1"},
        {"sync_master_info", "10000"},
        {"sync_relay_log", "10000"},
        {"sync_relay_log_info", "10000"},
        {"sync_source_info", "10000"},
        {"system_time_zone", "UTC"},
        {"table_definition_cache", "2000"},
        {"table_encryption_privilege_check", "OFF"},
        {"table_open_cache", "4000"},
        {"table_open_cache_instances", "16"},
        {"tablespace_definition_cache", "256"},
        {"temptable_max_mmap", "0"},
        {"temptable_use_mmap", "OFF"},
        {"thread_cache_size", "9"},
        {"thread_handling", "one-thread-per-connection"},
        {"thread_stack", "1048576"},
        {"time_zone", "SYSTEM"},
        {"tls_certificates_enforced_validation", "OFF"},
        {"tls_ciphersuites", ""},
        {"tls_version", "TLSv1.2,TLSv1.3"},
        {"tmpdir", "/tmp"},
        {"transaction_isolation", "REPEATABLE-READ"},
        {"transaction_read_only", "OFF"},
        {"unique_checks", "ON"},
        {"updatable_views_with_limit", "YES"},
        {"version", MYLITE_MYSQL_SERVER_VERSION_STRING},
        {"version_comment", MYLITE_MYSQL_SERVER_VERSION_COMMENT_STRING},
        {"version_compile_machine", "aarch64"},
        {"version_compile_os", "Linux"},
        {"version_compile_zlib", "1.3.2"},
        {"wait_timeout", "28800"},
    };
    const char *const expected_sql_log_rows[sql_log_variable_row_count][variable_column_count] = {
        {"sql_log_bin", "ON"},
        {"sql_log_off", "OFF"},
    };
    const char *const expected_on_rows[on_variable_row_count][variable_column_count] = {
        {"autocommit", "ON"},
        {"sql_log_bin", "ON"},
    };
    const char *const expected_gtid_default_rows[gtid_default_variable_row_count]
                                                [variable_column_count] = {
                                                    {"autocommit", "ON"},
                                                    {"gtid_executed", ""},
                                                    {"gtid_executed_compression_period", "0"},
                                                    {"gtid_mode", "OFF"},
                                                    {"gtid_next", "AUTOMATIC"},
                                                    {"gtid_owned", ""},
                                                    {"gtid_purged", ""},
                                                };
    const char
        *const expected_gtid_global_rows[gtid_global_variable_row_count][variable_column_count] = {
            {"gtid_executed", ""},
            {"gtid_executed_compression_period", "0"},
            {"gtid_mode", "OFF"},
            {"gtid_owned", ""},
            {"gtid_purged", ""},
        };
    const char *const expected_gtid_session_rows[gtid_session_variable_row_count]
                                                [variable_column_count] = {
                                                    {"gtid_executed", ""},
                                                    {"gtid_executed_compression_period", "0"},
                                                    {"gtid_mode", "OFF"},
                                                    {"gtid_next", "AUTOMATIC"},
                                                    {"gtid_owned", ""},
                                                    {"gtid_purged", ""},
                                                    {"sql_log_bin", "ON"},
                                                    {"warning_count", "0"},
                                                };
    const char
        *const expected_empty_gtid_rows[empty_gtid_variable_row_count][variable_column_count] = {
            {"gtid_executed", ""},
            {"gtid_owned", ""},
            {"gtid_purged", ""},
        };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open variables memory");
    failures += execute_statement_ok(database, "SET timestamp = 1700000000");
    failures += expect_query_rows(
        database,
        "SHOW VARIABLES",
        expected_session_rows,
        session_variable_row_count,
        "show variables rows"
    );
    failures += expect_query_rows(
        database,
        "SHOW SESSION VARIABLES",
        expected_session_rows,
        session_variable_row_count,
        "show session variables rows"
    );
    failures += expect_query_rows(
        database,
        "SHOW LOCAL VARIABLES",
        expected_session_rows,
        session_variable_row_count,
        "show local variables rows"
    );
    failures += expect_query_rows(
        database,
        "SHOW GLOBAL VARIABLES",
        expected_global_rows,
        global_variable_row_count,
        "show global variables rows"
    );
    failures += expect_query_rows(
        database,
        "SHOW VARIABLES LIKE 'sql\\_log\\_%'",
        expected_sql_log_rows,
        sql_log_variable_row_count,
        "show variables escaped underscore"
    );
    failures += expect_query_rows(
        database,
        "SHOW VARIABLES LIKE 'SQL\\_LOG\\_%'",
        expected_sql_log_rows,
        sql_log_variable_row_count,
        "show variables case-insensitive like"
    );
    failures += expect_query_rows(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'sql_log_bin'",
        NULL,
        0U,
        "show global omits session-only sql_log_bin"
    );
    failures += expect_single_row(
        database,
        "SHOW SESSION VARIABLES LIKE 'character_set_system'",
        (struct expected_variable_row){
            .name = "character_set_system",
            .value = "utf8mb3",
        },
        "show session includes global system charset"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'lower_case_table_names'",
        (struct expected_variable_row){
            .name = "lower_case_table_names",
            .value = "0",
        },
        "show variables lower case table names"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'lower_case_file_system'",
        (struct expected_variable_row){
            .name = "lower_case_file_system",
            .value = "OFF",
        },
        "show variables lower case file system"
    );
    failures += expect_single_row(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'lower_case_table_names'",
        (struct expected_variable_row){
            .name = "lower_case_table_names",
            .value = "0",
        },
        "show global variables lower case table names"
    );
    failures += expect_single_row(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'lower_case_file_system'",
        (struct expected_variable_row){
            .name = "lower_case_file_system",
            .value = "OFF",
        },
        "show global variables lower case file system"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'max_allowed_packet'",
        (struct expected_variable_row){
            .name = "max_allowed_packet",
            .value = "67108864",
        },
        "show variables max allowed packet"
    );
    failures += expect_single_row(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'max_allowed_packet'",
        (struct expected_variable_row){
            .name = "max_allowed_packet",
            .value = "67108864",
        },
        "show global variables max allowed packet"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'default_tmp_storage_engine'",
        (struct expected_variable_row){
            .name = "default_tmp_storage_engine",
            .value = "InnoDB",
        },
        "show variables default temporary storage engine"
    );
    failures += expect_single_row(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'default_tmp_storage_engine'",
        (struct expected_variable_row){
            .name = "default_tmp_storage_engine",
            .value = "InnoDB",
        },
        "show global variables default temporary storage engine"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'read_only'",
        (struct expected_variable_row){
            .name = "read_only",
            .value = "OFF",
        },
        "show variables read only"
    );
    failures += expect_single_row(
        database,
        "SHOW GLOBAL VARIABLES WHERE Variable_name = 'super_read_only'",
        (struct expected_variable_row){
            .name = "super_read_only",
            .value = "OFF",
        },
        "show global variables super read only"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'innodb_read_only'",
        (struct expected_variable_row){
            .name = "innodb_read_only",
            .value = "OFF",
        },
        "show variables innodb read only"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'explicit_defaults_for_timestamp'",
        (struct expected_variable_row){
            .name = "explicit_defaults_for_timestamp",
            .value = "ON",
        },
        "show variables explicit defaults for timestamp"
    );
    failures += expect_single_row(
        database,
        "SHOW GLOBAL VARIABLES WHERE Variable_name = 'explicit_defaults_for_timestamp'",
        (struct expected_variable_row){
            .name = "explicit_defaults_for_timestamp",
            .value = "ON",
        },
        "show global variables explicit defaults for timestamp"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'transaction_isolation'",
        (struct expected_variable_row){
            .name = "transaction_isolation",
            .value = "REPEATABLE-READ",
        },
        "show variables transaction isolation"
    );
    failures += expect_single_row(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'transaction_read_only'",
        (struct expected_variable_row){
            .name = "transaction_read_only",
            .value = "OFF",
        },
        "show global variables transaction read only"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Value = '0' AND "
        "Variable_name IN ('autocommit','lower_case_table_names')",
        (struct expected_variable_row){
            .name = "lower_case_table_names",
            .value = "0",
        },
        "show variables where lower case table names"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Value = 'OFF' AND "
        "Variable_name IN ('autocommit','lower_case_file_system')",
        (struct expected_variable_row){
            .name = "lower_case_file_system",
            .value = "OFF",
        },
        "show variables where lower case file system"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Value = '67108864' AND "
        "Variable_name IN ('autocommit','max_allowed_packet')",
        (struct expected_variable_row){
            .name = "max_allowed_packet",
            .value = "67108864",
        },
        "show variables where max allowed packet"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Variable_name = 'AUTOCOMMIT'",
        (struct expected_variable_row){
            .name = "autocommit",
            .value = "ON",
        },
        "show variables where case-insensitive equality"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE `Variable_name` <=> 'autocommit'",
        (struct expected_variable_row){
            .name = "autocommit",
            .value = "ON",
        },
        "show variables where null-safe equality"
    );
    failures += expect_query_rows(
        database,
        "SHOW VARIABLES WHERE Value = 'on' AND Variable_name IN "
        "('autocommit','sql_log_bin','sql_log_off')",
        expected_on_rows,
        on_variable_row_count,
        "show variables where value equality"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Variable_name NOT LIKE 'sql\\_%' AND "
        "Variable_name IN ('autocommit','sql_mode','sql_log_bin')",
        (struct expected_variable_row){
            .name = "autocommit",
            .value = "ON",
        },
        "show variables where not like"
    );
    failures += expect_query_rows(
        database,
        "SHOW VARIABLES WHERE (Variable_name = 'autocommit' OR "
        "Variable_name = 'sql_log_bin') AND Value = 'ON'",
        expected_on_rows,
        on_variable_row_count,
        "show variables where or and"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Variable_name <> 'autocommit' AND "
        "Variable_name IN ('autocommit','sql_mode')",
        (struct expected_variable_row){
            .name = "sql_mode",
            .value = default_sql_mode,
        },
        "show variables where not equal"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Variable_name < 'b' AND Variable_name IN ('autocommit','version')",
        (struct expected_variable_row){
            .name = "autocommit",
            .value = "ON",
        },
        "show variables where less than"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Variable_name > 's' AND Variable_name IN ('autocommit','version')",
        (struct expected_variable_row){
            .name = "version",
            .value = MYLITE_MYSQL_SERVER_VERSION_STRING,
        },
        "show variables where greater than"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Variable_name IN (NULL, 'autocommit')",
        (struct expected_variable_row){
            .name = "autocommit",
            .value = "ON",
        },
        "show variables where in null"
    );
    failures += expect_query_rows(
        database,
        "SHOW VARIABLES WHERE Variable_name NOT IN (NULL, 'autocommit') AND "
        "Variable_name IN ('autocommit','sql_mode')",
        NULL,
        0U,
        "show variables where not in null"
    );
    failures += expect_query_rows(
        database,
        "SHOW VARIABLES WHERE Value IS NULL OR Variable_name IS NULL",
        NULL,
        0U,
        "show variables where is null"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Variable_name IS NOT NULL AND Variable_name = 'autocommit'",
        (struct expected_variable_row){
            .name = "autocommit",
            .value = "ON",
        },
        "show variables where is not null"
    );
    failures += expect_query_rows(
        database,
        "SHOW VARIABLES WHERE Variable_name IN "
        "('autocommit','gtid_purged','gtid_executed','gtid_executed_compression_period',"
        "'gtid_next','gtid_owned','gtid_mode')",
        expected_gtid_default_rows,
        gtid_default_variable_row_count,
        "show variables where default gtid rows"
    );
    failures += expect_query_rows(
        database,
        "SHOW GLOBAL VARIABLES WHERE Variable_name IN "
        "('sql_log_bin','warning_count','gtid_purged','gtid_executed',"
        "'gtid_executed_compression_period','gtid_next','gtid_owned','gtid_mode')",
        expected_gtid_global_rows,
        gtid_global_variable_row_count,
        "show global variables where gtid rows"
    );
    failures += expect_query_rows(
        database,
        "SHOW SESSION VARIABLES WHERE Variable_name IN "
        "('sql_log_bin','warning_count','gtid_purged','gtid_executed',"
        "'gtid_executed_compression_period','gtid_next','gtid_owned','gtid_mode')",
        expected_gtid_session_rows,
        gtid_session_variable_row_count,
        "show session variables where gtid rows"
    );
    failures += expect_query_rows(
        database,
        "SHOW VARIABLES WHERE Value = '' AND Variable_name IN "
        "('gtid_purged','gtid_executed','gtid_owned','gtid_mode')",
        expected_empty_gtid_rows,
        empty_gtid_variable_row_count,
        "show variables where empty gtid values"
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@gtid_mode",
            .expected = "OFF",
            .context = "gtid_mode scalar default",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@GLOBAL.gtid_mode",
            .expected = "OFF",
            .context = "gtid_mode scalar global",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@gtid_purged",
            .expected = "",
            .context = "gtid_purged scalar default",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@GLOBAL.gtid_executed",
            .expected = "",
            .context = "gtid_executed scalar global",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@SESSION.gtid_owned",
            .expected = "",
            .context = "gtid_owned scalar session",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@LOCAL.gtid_owned",
            .expected = "",
            .context = "gtid_owned scalar local",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@lower_case_file_system",
            .expected = "0",
            .context = "lower_case_file_system scalar default",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@GLOBAL.lower_case_file_system",
            .expected = "0",
            .context = "lower_case_file_system scalar global",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@LOWER_CASE_FILE_SYSTEM",
            .expected = "0",
            .context = "lower_case_file_system scalar case-insensitive",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@global.`lower_case_file_system`",
            .expected = "0",
            .context = "lower_case_file_system scalar quoted name",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@lower_case_table_names",
            .expected = "0",
            .context = "lower_case_table_names scalar default",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@GLOBAL.lower_case_table_names",
            .expected = "0",
            .context = "lower_case_table_names scalar global",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@LOWER_CASE_TABLE_NAMES",
            .expected = "0",
            .context = "lower_case_table_names scalar case-insensitive",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@global.`lower_case_table_names`",
            .expected = "0",
            .context = "lower_case_table_names scalar quoted name",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@read_only",
            .expected = "0",
            .context = "read_only scalar default",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@GLOBAL.super_read_only",
            .expected = "0",
            .context = "super_read_only scalar global",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@global.`innodb_read_only`",
            .expected = "0",
            .context = "innodb_read_only scalar quoted global",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_show_variables_state_and_file_safety(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "file_safety") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open variables file");
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        catalog_generation = session->catalog_generation;
        sqlite_schema_generation = session->sqlite_schema_generation;
    }

    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'autocommit'",
        (struct expected_variable_row){
            .name = "autocommit",
            .value = "ON",
        },
        "show variables file result"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Variable_name = 'autocommit'",
        (struct expected_variable_row){
            .name = "autocommit",
            .value = "ON",
        },
        "show variables where file result"
    );
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_uint64(
            session->catalog_generation,
            catalog_generation,
            "show variables leaves catalog generation"
        );
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_schema_generation,
            "show variables leaves SQLite schema generation"
        );
    }
    failures +=
        expect_int(read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)), 0, "preamble");
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "show variables leaves preamble"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen variables file");
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'autocommit'",
        (struct expected_variable_row){
            .name = "autocommit",
            .value = "ON",
        },
        "show variables after reopen"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_show_variables_diagnostics(void) {
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostics memory");
    failures += execute_error(
        database,
        "SHOW FULL VARIABLES",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW VARIABLES LIKE 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW VARIABLES LIKE 'sql_%' LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW VARIABLES WHERE missing = 'x'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SHOW VARIABLES WHERE variables.Variable_name = 'autocommit'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'variables.Variable_name' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SHOW VARIABLES WHERE Variable_name = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW VARIABLES WHERE supports only string literal predicates",
        }
    );
    failures += execute_error(
        database,
        "SHOW VARIABLES WHERE (Variable_name = 'autocommit') XOR (Value = 'ON')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW VARIABLES WHERE does not support XOR predicates",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@SESSION.gtid_purged",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'gtid_purged' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@LOCAL.gtid_executed",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'gtid_executed' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@SESSION.gtid_mode",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'gtid_mode' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@SESSION.lower_case_table_names",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_table_names' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@SESSION.lower_case_file_system",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_file_system' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@LOCAL.lower_case_table_names",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_table_names' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@LOCAL.lower_case_file_system",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_file_system' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@SESSION.read_only",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'read_only' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@LOCAL.super_read_only",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'super_read_only' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@SESSION.innodb_read_only",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'innodb_read_only' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SET SESSION gtid_owned = ''",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'gtid_owned' is a read only variable",
        }
    );

    failures += execute_error(
        database,
        "SELECT missing_column",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor-backed table reads",
        }
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'warning_count'",
        (struct expected_variable_row){
            .name = "warning_count",
            .value = "0",
        },
        "show variables warning count clears diagnostics"
    );
    failures += expect_row_count(database, -1, "show variables row count");
    failures += execute_ok(database, "SHOW VARIABLES LIKE 'sql_slave_skip_counter'", &result);
    failures += expect_size(
        mylite_result_warning_count(result),
        0U,
        "show variables deprecated alias warning count"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(database, -1, "deprecated show variables row count");

    mylite_close(database);
    return failures;
}

static int test_show_variables_independent_handles(void) {
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first variables handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second variables handle");
    failures += execute_statement_ok(first, "SET NAMES utf8mb4");
    failures += expect_single_row(
        first,
        "SHOW VARIABLES LIKE 'character_set_client'",
        (struct expected_variable_row){
            .name = "character_set_client",
            .value = "utf8mb4",
        },
        "first handle character set"
    );
    failures += expect_single_row(
        second,
        "SHOW VARIABLES LIKE 'character_set_client'",
        (struct expected_variable_row){
            .name = "character_set_client",
            .value = "utf8mb4",
        },
        "second handle character set"
    );

    mylite_close(second);
    mylite_close(first);
    return failures;
}

static int expect_query_rows(
    mylite_db *database,
    const char *sql,
    const char *const expected_rows[][variable_column_count],
    size_t expected_row_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), variable_column_count, context);
    for (size_t column = 0U; column < variable_column_count; ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            variable_columns[column],
            context
        );
    }
    failures += expect_size(mylite_result_row_count(result), expected_row_count, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    for (size_t row = 0U; row < expected_row_count; ++row) {
        for (size_t column = 0U; column < variable_column_count; ++column) {
            failures += expect_text_or_null(
                mylite_result_value_text(result, row, column),
                expected_rows[row][column],
                context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_single_row(
    mylite_db *database,
    const char *sql,
    struct expected_variable_row expected,
    const char *context
) {
    const char *const expected_rows[1][variable_column_count] = {{expected.name, expected.value}};

    return expect_query_rows(database, sql, expected_rows, 1U, context);
}

static int expect_scalar_text(mylite_db *database, struct expected_scalar_text_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 1U, query.context);
        failures += expect_size(mylite_result_row_count(result), 1U, query.context);
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, 0U),
            query.expected,
            query.context
        );
    } else {
        failures += 1;
    }

    mylite_result_free(result);
    return failures;
}

static int expect_row_count(mylite_db *database, int64_t expected, const char *context) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SELECT ROW_COUNT()", &result);

    if (result != NULL) {
        char expected_text[row_count_text_capacity];
        int written = snprintf(expected_text, sizeof(expected_text), "%" PRId64, expected);

        if (written < 0 || (size_t)written >= sizeof(expected_text)) {
            fprintf(stderr, "%s: failed to format expected row count\n", context);
            failures += 1;
        } else {
            failures += expect_text_or_null(
                mylite_result_value_text(result, 0U, 0U),
                expected_text,
                context
            );
        }
    } else {
        failures += 1;
    }

    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return rc;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        const struct mylite_diagnostics *diagnostics = mylite_connection_diagnostics(database);

        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            sql,
            mylite_diagnostics_errcode(diagnostics),
            mylite_diagnostics_sqlstate(diagnostics),
            mylite_diagnostics_errmsg(diagnostics)
        );
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    const struct mylite_diagnostics *diagnostics = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }

    diagnostics = mylite_connection_diagnostics(database);
    failures += expect_int(mylite_diagnostics_errcode(diagnostics), expected.code, sql);
    failures +=
        expect_text_or_null(mylite_diagnostics_sqlstate(diagnostics), expected.sqlstate, sql);
    failures += expect_contains(mylite_diagnostics_errmsg(diagnostics), expected.message_part, sql);
    failures += expect_int(result == NULL, 1, "failed statement returned no result");

    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "%s/mylite_show_variables_%d_%s.mylite",
        P_tmpdir,
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path\n");
        return 1;
    }

    return 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %" PRId64 ", got %" PRId64 "\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %" PRIu64 ", got %" PRIu64 "\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "NULL" : actual,
            needle == NULL ? "NULL" : needle
        );
        return 1;
    }

    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }

    return 0;
}
