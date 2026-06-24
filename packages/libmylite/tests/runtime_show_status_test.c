#include <mylite/mylite.h>

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
    status_column_count = 2,
    diagnostics_column_count = 3,
    mysql_error_parse = 1064,
};

struct expected_status_row {
    const char *name;
    const char *value;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static const char *const status_columns[status_column_count] = {
    "Variable_name",
    "Value",
};

static const char *const diagnostics_columns[diagnostics_column_count] = {
    "ROW_COUNT()",
    "@@warning_count",
    "@@error_count",
};

static const struct expected_status_row expected_session_rows[] = {
    {"Aborted_clients", "0"},
    {"Aborted_connects", "0"},
    {"Binlog_cache_disk_use", "0"},
    {"Binlog_cache_use", "0"},
    {"Binlog_stmt_cache_disk_use", "0"},
    {"Binlog_stmt_cache_use", "0"},
    {"Bytes_received", "0"},
    {"Bytes_sent", "0"},
    {"Com_admin_commands", "0"},
    {"Com_assign_to_keycache", "0"},
    {"Com_alter_db", "0"},
    {"Com_alter_event", "0"},
    {"Com_alter_function", "0"},
    {"Com_alter_instance", "0"},
    {"Com_alter_procedure", "0"},
    {"Com_alter_resource_group", "0"},
    {"Com_alter_server", "0"},
    {"Com_alter_table", "0"},
    {"Com_alter_tablespace", "0"},
    {"Com_alter_user", "0"},
    {"Com_alter_user_default_role", "0"},
    {"Com_analyze", "0"},
    {"Com_begin", "0"},
    {"Com_binlog", "0"},
    {"Com_call_procedure", "0"},
    {"Com_change_db", "0"},
    {"Com_change_repl_filter", "0"},
    {"Com_change_replication_source", "0"},
    {"Com_check", "0"},
    {"Com_checksum", "0"},
    {"Com_clone", "0"},
    {"Com_commit", "0"},
    {"Com_create_db", "0"},
    {"Com_create_event", "0"},
    {"Com_create_function", "0"},
    {"Com_create_index", "0"},
    {"Com_create_procedure", "0"},
    {"Com_create_role", "0"},
    {"Com_create_server", "0"},
    {"Com_create_table", "0"},
    {"Com_create_resource_group", "0"},
    {"Com_create_trigger", "0"},
    {"Com_create_udf", "0"},
    {"Com_create_user", "0"},
    {"Com_create_view", "0"},
    {"Com_create_spatial_reference_system", "0"},
    {"Com_dealloc_sql", "0"},
    {"Com_delete", "0"},
    {"Com_delete_multi", "0"},
    {"Com_do", "0"},
    {"Com_drop_db", "0"},
    {"Com_drop_event", "0"},
    {"Com_drop_function", "0"},
    {"Com_drop_index", "0"},
    {"Com_drop_procedure", "0"},
    {"Com_drop_resource_group", "0"},
    {"Com_drop_role", "0"},
    {"Com_drop_server", "0"},
    {"Com_drop_spatial_reference_system", "0"},
    {"Com_drop_table", "0"},
    {"Com_drop_trigger", "0"},
    {"Com_drop_user", "0"},
    {"Com_drop_view", "0"},
    {"Com_empty_query", "0"},
    {"Com_execute_sql", "0"},
    {"Com_explain_other", "0"},
    {"Com_flush", "0"},
    {"Com_get_diagnostics", "0"},
    {"Com_grant", "0"},
    {"Com_grant_roles", "0"},
    {"Com_ha_close", "0"},
    {"Com_ha_open", "0"},
    {"Com_ha_read", "0"},
    {"Com_help", "0"},
    {"Com_import", "0"},
    {"Com_insert", "0"},
    {"Com_insert_select", "0"},
    {"Com_install_component", "0"},
    {"Com_install_plugin", "0"},
    {"Com_kill", "0"},
    {"Com_load", "0"},
    {"Com_lock_instance", "0"},
    {"Com_lock_tables", "0"},
    {"Com_optimize", "0"},
    {"Com_preload_keys", "0"},
    {"Com_prepare_sql", "0"},
    {"Com_purge", "0"},
    {"Com_purge_before_date", "0"},
    {"Com_release_savepoint", "0"},
    {"Com_rename_table", "0"},
    {"Com_rename_user", "0"},
    {"Com_repair", "0"},
    {"Com_replace", "0"},
    {"Com_replace_select", "0"},
    {"Com_reset", "0"},
    {"Com_resignal", "0"},
    {"Com_restart", "0"},
    {"Com_revoke", "0"},
    {"Com_revoke_all", "0"},
    {"Com_revoke_roles", "0"},
    {"Com_rollback", "0"},
    {"Com_rollback_to_savepoint", "0"},
    {"Com_savepoint", "0"},
    {"Com_select", "0"},
    {"Com_set_option", "0"},
    {"Com_set_password", "0"},
    {"Com_set_resource_group", "0"},
    {"Com_set_role", "0"},
    {"Com_signal", "0"},
    {"Com_show_binlog_events", "0"},
    {"Com_show_binlogs", "0"},
    {"Com_show_charsets", "0"},
    {"Com_show_collations", "0"},
    {"Com_show_create_db", "0"},
    {"Com_show_create_event", "0"},
    {"Com_show_create_func", "0"},
    {"Com_show_create_proc", "0"},
    {"Com_show_create_table", "0"},
    {"Com_show_create_trigger", "0"},
    {"Com_show_databases", "0"},
    {"Com_show_engine_logs", "0"},
    {"Com_show_engine_mutex", "0"},
    {"Com_show_engine_status", "0"},
    {"Com_show_events", "0"},
    {"Com_show_errors", "0"},
    {"Com_show_fields", "0"},
    {"Com_show_function_code", "0"},
    {"Com_show_function_status", "0"},
    {"Com_show_grants", "0"},
    {"Com_show_keys", "0"},
    {"Com_show_binary_log_status", "0"},
    {"Com_show_open_tables", "0"},
    {"Com_show_parse_tree", "0"},
    {"Com_show_plugins", "0"},
    {"Com_show_privileges", "0"},
    {"Com_show_procedure_code", "0"},
    {"Com_show_procedure_status", "0"},
    {"Com_show_processlist", "0"},
    {"Com_show_profile", "0"},
    {"Com_show_profiles", "0"},
    {"Com_show_relaylog_events", "0"},
    {"Com_show_replicas", "0"},
    {"Com_show_replica_status", "0"},
    {"Com_show_status", "0"},
    {"Com_show_storage_engines", "0"},
    {"Com_show_table_status", "0"},
    {"Com_show_tables", "0"},
    {"Com_show_triggers", "0"},
    {"Com_show_variables", "0"},
    {"Com_show_warnings", "0"},
    {"Com_show_create_user", "0"},
    {"Com_shutdown", "0"},
    {"Com_replica_start", "0"},
    {"Com_replica_stop", "0"},
    {"Com_group_replication_start", "0"},
    {"Com_group_replication_stop", "0"},
    {"Com_stmt_execute", "0"},
    {"Com_stmt_close", "0"},
    {"Com_stmt_fetch", "0"},
    {"Com_stmt_prepare", "0"},
    {"Com_stmt_reset", "0"},
    {"Com_stmt_send_long_data", "0"},
    {"Com_truncate", "0"},
    {"Com_uninstall_component", "0"},
    {"Com_uninstall_plugin", "0"},
    {"Com_unlock_instance", "0"},
    {"Com_unlock_tables", "0"},
    {"Com_update", "0"},
    {"Com_update_multi", "0"},
    {"Com_xa_commit", "0"},
    {"Com_xa_end", "0"},
    {"Com_xa_prepare", "0"},
    {"Com_xa_recover", "0"},
    {"Com_xa_rollback", "0"},
    {"Com_xa_start", "0"},
    {"Com_stmt_reprepare", "0"},
    {"Compression", "OFF"},
    {"Connection_control_delay_generated", "0"},
    {"Connection_control_exempted_unknown_users", "0"},
    {"Connection_errors_accept", "0"},
    {"Connection_errors_internal", "0"},
    {"Connection_errors_max_connections", "0"},
    {"Connection_errors_peer_address", "0"},
    {"Connection_errors_select", "0"},
    {"Connection_errors_tcpwrap", "0"},
    {"Connections", "1"},
    {"Created_tmp_disk_tables", "0"},
    {"Created_tmp_files", "0"},
    {"Created_tmp_tables", "0"},
    {"Handler_commit", "0"},
    {"Handler_delete", "0"},
    {"Handler_discover", "0"},
    {"Handler_external_lock", "0"},
    {"Handler_mrr_init", "0"},
    {"Handler_prepare", "0"},
    {"Handler_read_first", "0"},
    {"Handler_read_key", "0"},
    {"Handler_read_last", "0"},
    {"Handler_read_next", "0"},
    {"Handler_read_prev", "0"},
    {"Handler_read_rnd", "0"},
    {"Handler_read_rnd_next", "0"},
    {"Handler_rollback", "0"},
    {"Handler_savepoint", "0"},
    {"Handler_savepoint_rollback", "0"},
    {"Handler_update", "0"},
    {"Handler_write", "0"},
    {"Open_files", "0"},
    {"Open_streams", "0"},
    {"Open_table_definitions", "0"},
    {"Open_tables", "0"},
    {"Opened_files", "0"},
    {"Opened_table_definitions", "0"},
    {"Opened_tables", "0"},
    {"Prepared_stmt_count", "0"},
    {"Queries", "0"},
    {"Questions", "0"},
    {"Select_full_join", "0"},
    {"Select_full_range_join", "0"},
    {"Select_range", "0"},
    {"Select_range_check", "0"},
    {"Select_scan", "0"},
    {"Slow_launch_threads", "0"},
    {"Slow_queries", "0"},
    {"Sort_merge_passes", "0"},
    {"Sort_range", "0"},
    {"Sort_rows", "0"},
    {"Sort_scan", "0"},
    {"Ssl_accept_renegotiates", "0"},
    {"Ssl_accepts", "0"},
    {"Ssl_callback_cache_hits", "0"},
    {"Ssl_cipher", ""},
    {"Ssl_cipher_list", ""},
    {"Ssl_client_connects", "0"},
    {"Ssl_connect_renegotiates", "0"},
    {"Ssl_ctx_verify_depth", "0"},
    {"Ssl_ctx_verify_mode", "0"},
    {"Ssl_default_timeout", "0"},
    {"Ssl_finished_accepts", "0"},
    {"Ssl_finished_connects", "0"},
    {"Ssl_server_not_after", ""},
    {"Ssl_server_not_before", ""},
    {"Ssl_session_cache_hits", "0"},
    {"Ssl_session_cache_misses", "0"},
    {"Ssl_session_cache_mode", ""},
    {"Ssl_session_cache_overflows", "0"},
    {"Ssl_session_cache_size", "0"},
    {"Ssl_session_cache_timeout", "0"},
    {"Ssl_session_cache_timeouts", "0"},
    {"Ssl_sessions_reused", "0"},
    {"Ssl_used_session_cache_entries", "0"},
    {"Ssl_verify_depth", "0"},
    {"Ssl_verify_mode", "0"},
    {"Ssl_version", ""},
    {"Table_locks_immediate", "0"},
    {"Table_locks_waited", "0"},
    {"Table_open_cache_hits", "0"},
    {"Table_open_cache_misses", "0"},
    {"Table_open_cache_overflows", "0"},
    {"Tc_log_max_pages_used", "0"},
    {"Tc_log_page_size", "0"},
    {"Tc_log_page_waits", "0"},
    {"Threads_cached", "0"},
    {"Threads_connected", "1"},
    {"Threads_created", "1"},
    {"Threads_running", "1"},
    {"Uptime", "0"},
    {"Uptime_since_flush_status", "0"},
};

static int test_show_status_values_scopes_and_filters(void);
static int test_show_status_state_and_file_safety(void);
static int test_show_status_diagnostics(void);
static int test_show_status_independent_handles(void);
static int expect_status_rows(
    mylite_db *database,
    const char *sql,
    const struct expected_status_row *expected_rows,
    size_t expected_row_count,
    const char *context
);
static int expect_status_row_count(
    mylite_db *database,
    const char *sql,
    size_t expected_row_count,
    const char *context
);
static int expect_single_status_row(
    mylite_db *database,
    const char *sql,
    struct expected_status_row expected,
    const char *context
);
static int expect_diagnostics_row(mylite_db *database, const char *context);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
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

    failures += test_show_status_values_scopes_and_filters();
    failures += test_show_status_state_and_file_safety();
    failures += test_show_status_diagnostics();
    failures += test_show_status_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_show_status_values_scopes_and_filters(void) {
    static const struct expected_status_row expected_thread_rows[] = {
        {"Threads_cached", "0"},
        {"Threads_connected", "1"},
        {"Threads_created", "1"},
        {"Threads_running", "1"},
    };
    static const struct expected_status_row expected_ssl_rows[] = {
        {"Ssl_accept_renegotiates", "0"},
        {"Ssl_accepts", "0"},
        {"Ssl_callback_cache_hits", "0"},
        {"Ssl_cipher", ""},
        {"Ssl_cipher_list", ""},
        {"Ssl_client_connects", "0"},
        {"Ssl_connect_renegotiates", "0"},
        {"Ssl_ctx_verify_depth", "0"},
        {"Ssl_ctx_verify_mode", "0"},
        {"Ssl_default_timeout", "0"},
        {"Ssl_finished_accepts", "0"},
        {"Ssl_finished_connects", "0"},
        {"Ssl_server_not_after", ""},
        {"Ssl_server_not_before", ""},
        {"Ssl_session_cache_hits", "0"},
        {"Ssl_session_cache_misses", "0"},
        {"Ssl_session_cache_mode", ""},
        {"Ssl_session_cache_overflows", "0"},
        {"Ssl_session_cache_size", "0"},
        {"Ssl_session_cache_timeout", "0"},
        {"Ssl_session_cache_timeouts", "0"},
        {"Ssl_sessions_reused", "0"},
        {"Ssl_used_session_cache_entries", "0"},
        {"Ssl_verify_depth", "0"},
        {"Ssl_verify_mode", "0"},
        {"Ssl_version", ""},
    };
    static const struct expected_status_row expected_binlog_rows[] = {
        {"Binlog_cache_disk_use", "0"},
        {"Binlog_cache_use", "0"},
        {"Binlog_stmt_cache_disk_use", "0"},
        {"Binlog_stmt_cache_use", "0"},
    };
    static const struct expected_status_row expected_connection_rows[] = {
        {"Connection_control_delay_generated", "0"},
        {"Connection_control_exempted_unknown_users", "0"},
        {"Connection_errors_accept", "0"},
        {"Connection_errors_internal", "0"},
        {"Connection_errors_max_connections", "0"},
        {"Connection_errors_peer_address", "0"},
        {"Connection_errors_select", "0"},
        {"Connection_errors_tcpwrap", "0"},
    };
    static const struct expected_status_row expected_created_rows[] = {
        {"Created_tmp_disk_tables", "0"},
        {"Created_tmp_files", "0"},
        {"Created_tmp_tables", "0"},
    };
    static const struct expected_status_row expected_handler_rows[] = {
        {"Handler_commit", "0"},
        {"Handler_delete", "0"},
        {"Handler_discover", "0"},
        {"Handler_external_lock", "0"},
        {"Handler_mrr_init", "0"},
        {"Handler_prepare", "0"},
        {"Handler_read_first", "0"},
        {"Handler_read_key", "0"},
        {"Handler_read_last", "0"},
        {"Handler_read_next", "0"},
        {"Handler_read_prev", "0"},
        {"Handler_read_rnd", "0"},
        {"Handler_read_rnd_next", "0"},
        {"Handler_rollback", "0"},
        {"Handler_savepoint", "0"},
        {"Handler_savepoint_rollback", "0"},
        {"Handler_update", "0"},
        {"Handler_write", "0"},
    };
    static const struct expected_status_row expected_open_rows[] = {
        {"Open_files", "0"},
        {"Open_streams", "0"},
        {"Open_table_definitions", "0"},
        {"Open_tables", "0"},
        {"Opened_files", "0"},
        {"Opened_table_definitions", "0"},
        {"Opened_tables", "0"},
    };
    static const struct expected_status_row expected_select_rows[] = {
        {"Select_full_join", "0"},
        {"Select_full_range_join", "0"},
        {"Select_range", "0"},
        {"Select_range_check", "0"},
        {"Select_scan", "0"},
    };
    static const struct expected_status_row expected_table_lock_rows[] = {
        {"Table_locks_immediate", "0"},
        {"Table_locks_waited", "0"},
    };
    static const struct expected_status_row expected_sort_rows[] = {
        {"Sort_merge_passes", "0"},
        {"Sort_range", "0"},
        {"Sort_rows", "0"},
        {"Sort_scan", "0"},
    };
    static const struct expected_status_row expected_table_open_cache_rows[] = {
        {"Table_open_cache_hits", "0"},
        {"Table_open_cache_misses", "0"},
        {"Table_open_cache_overflows", "0"},
    };
    static const struct expected_status_row expected_tc_log_rows[] = {
        {"Tc_log_max_pages_used", "0"},
        {"Tc_log_page_size", "0"},
        {"Tc_log_page_waits", "0"},
    };
    static const struct expected_status_row expected_slow_rows[] = {
        {"Slow_launch_threads", "0"},
        {"Slow_queries", "0"},
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open status memory");
    failures += expect_status_rows(
        database,
        "SHOW STATUS",
        expected_session_rows,
        sizeof(expected_session_rows) / sizeof(expected_session_rows[0]),
        "show status rows"
    );
    failures += expect_status_rows(
        database,
        "SHOW SESSION STATUS",
        expected_session_rows,
        sizeof(expected_session_rows) / sizeof(expected_session_rows[0]),
        "show session status rows"
    );
    failures += expect_status_rows(
        database,
        "SHOW LOCAL STATUS",
        expected_session_rows,
        sizeof(expected_session_rows) / sizeof(expected_session_rows[0]),
        "show local status rows"
    );
    failures += expect_status_row_count(
        database,
        "SHOW GLOBAL STATUS",
        (sizeof(expected_session_rows) / sizeof(expected_session_rows[0])) - 1U,
        "show global status rows"
    );
    failures += expect_single_status_row(
        database,
        "SHOW STATUS LIKE 'Compression'",
        (struct expected_status_row){
            .name = "Compression",
            .value = "OFF",
        },
        "show status compression"
    );
    failures += expect_status_rows(
        database,
        "SHOW GLOBAL STATUS LIKE 'Compression'",
        NULL,
        0U,
        "show global status omits compression"
    );
    failures += expect_status_rows(
        database,
        "SHOW STATUS LIKE 'Threads\\_%'",
        expected_thread_rows,
        sizeof(expected_thread_rows) / sizeof(expected_thread_rows[0]),
        "show status threads pattern"
    );
    failures += expect_status_rows(
        database,
        "SHOW STATUS LIKE 'THREADS\\_%'",
        expected_thread_rows,
        sizeof(expected_thread_rows) / sizeof(expected_thread_rows[0]),
        "show status uppercase threads pattern"
    );
    failures += expect_status_rows(
        database,
        "SHOW STATUS LIKE 'Ssl\\_%'",
        expected_ssl_rows,
        sizeof(expected_ssl_rows) / sizeof(expected_ssl_rows[0]),
        "show status empty string values"
    );
    failures += expect_status_rows(
        database,
        "SHOW GLOBAL STATUS LIKE 'Ssl\\_%'",
        expected_ssl_rows,
        sizeof(expected_ssl_rows) / sizeof(expected_ssl_rows[0]),
        "show global status ssl placeholders"
    );
    failures += expect_status_rows(
        database,
        "SHOW LOCAL STATUS LIKE 'Ssl\\_%'",
        expected_ssl_rows,
        sizeof(expected_ssl_rows) / sizeof(expected_ssl_rows[0]),
        "show local status ssl placeholders"
    );
    failures += expect_status_rows(
        database,
        "SHOW STATUS LIKE 'Binlog\\_%'",
        expected_binlog_rows,
        sizeof(expected_binlog_rows) / sizeof(expected_binlog_rows[0]),
        "show status binlog counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW GLOBAL STATUS LIKE 'Binlog\\_%'",
        expected_binlog_rows,
        sizeof(expected_binlog_rows) / sizeof(expected_binlog_rows[0]),
        "show global status binlog counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW STATUS LIKE 'Connection\\_%'",
        expected_connection_rows,
        sizeof(expected_connection_rows) / sizeof(expected_connection_rows[0]),
        "show status connection diagnostics"
    );
    failures += expect_status_rows(
        database,
        "SHOW GLOBAL STATUS LIKE 'Connection\\_%'",
        expected_connection_rows,
        sizeof(expected_connection_rows) / sizeof(expected_connection_rows[0]),
        "show global status connection diagnostics"
    );
    failures += expect_status_rows(
        database,
        "SHOW STATUS LIKE 'Created\\_%'",
        expected_created_rows,
        sizeof(expected_created_rows) / sizeof(expected_created_rows[0]),
        "show status created counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW GLOBAL STATUS LIKE 'Created\\_%'",
        expected_created_rows,
        sizeof(expected_created_rows) / sizeof(expected_created_rows[0]),
        "show global status created counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW LOCAL STATUS LIKE 'Created\\_%'",
        expected_created_rows,
        sizeof(expected_created_rows) / sizeof(expected_created_rows[0]),
        "show local status created counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW STATUS LIKE 'Handler\\_%'",
        expected_handler_rows,
        sizeof(expected_handler_rows) / sizeof(expected_handler_rows[0]),
        "show status handler counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW GLOBAL STATUS LIKE 'Handler\\_%'",
        expected_handler_rows,
        sizeof(expected_handler_rows) / sizeof(expected_handler_rows[0]),
        "show global status handler counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW STATUS LIKE 'Open%'",
        expected_open_rows,
        sizeof(expected_open_rows) / sizeof(expected_open_rows[0]),
        "show status open counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW GLOBAL STATUS LIKE 'Open%'",
        expected_open_rows,
        sizeof(expected_open_rows) / sizeof(expected_open_rows[0]),
        "show global status open counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW LOCAL STATUS LIKE 'Open%'",
        expected_open_rows,
        sizeof(expected_open_rows) / sizeof(expected_open_rows[0]),
        "show local status open counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW STATUS LIKE 'Select\\_%'",
        expected_select_rows,
        sizeof(expected_select_rows) / sizeof(expected_select_rows[0]),
        "show status select counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW GLOBAL STATUS LIKE 'Select\\_%'",
        expected_select_rows,
        sizeof(expected_select_rows) / sizeof(expected_select_rows[0]),
        "show global status select counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW LOCAL STATUS LIKE 'Select\\_%'",
        expected_select_rows,
        sizeof(expected_select_rows) / sizeof(expected_select_rows[0]),
        "show local status select counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW STATUS LIKE 'Table_locks\\_%'",
        expected_table_lock_rows,
        sizeof(expected_table_lock_rows) / sizeof(expected_table_lock_rows[0]),
        "show status table lock counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW GLOBAL STATUS LIKE 'Table_locks\\_%'",
        expected_table_lock_rows,
        sizeof(expected_table_lock_rows) / sizeof(expected_table_lock_rows[0]),
        "show global status table lock counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW LOCAL STATUS LIKE 'Table_locks\\_%'",
        expected_table_lock_rows,
        sizeof(expected_table_lock_rows) / sizeof(expected_table_lock_rows[0]),
        "show local status table lock counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW STATUS LIKE 'Sort\\_%'",
        expected_sort_rows,
        sizeof(expected_sort_rows) / sizeof(expected_sort_rows[0]),
        "show status sort counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW GLOBAL STATUS LIKE 'Sort\\_%'",
        expected_sort_rows,
        sizeof(expected_sort_rows) / sizeof(expected_sort_rows[0]),
        "show global status sort counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW STATUS LIKE 'Table_open_cache\\_%'",
        expected_table_open_cache_rows,
        sizeof(expected_table_open_cache_rows) / sizeof(expected_table_open_cache_rows[0]),
        "show status table open cache counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW GLOBAL STATUS LIKE 'Table_open_cache\\_%'",
        expected_table_open_cache_rows,
        sizeof(expected_table_open_cache_rows) / sizeof(expected_table_open_cache_rows[0]),
        "show global status table open cache counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW STATUS LIKE 'Tc_log\\_%'",
        expected_tc_log_rows,
        sizeof(expected_tc_log_rows) / sizeof(expected_tc_log_rows[0]),
        "show status tc log counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW GLOBAL STATUS LIKE 'Tc_log\\_%'",
        expected_tc_log_rows,
        sizeof(expected_tc_log_rows) / sizeof(expected_tc_log_rows[0]),
        "show global status tc log counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW STATUS LIKE 'Slow\\_%'",
        expected_slow_rows,
        sizeof(expected_slow_rows) / sizeof(expected_slow_rows[0]),
        "show status slow counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW GLOBAL STATUS LIKE 'Slow\\_%'",
        expected_slow_rows,
        sizeof(expected_slow_rows) / sizeof(expected_slow_rows[0]),
        "show global status slow counters"
    );
    failures += expect_status_rows(
        database,
        "SHOW STATUS LIKE 'missing%'",
        NULL,
        0U,
        "show status missing pattern"
    );
    failures += expect_single_status_row(
        database,
        "SHOW STATUS WHERE Variable_name = 'Threads_connected'",
        (struct expected_status_row){
            .name = "Threads_connected",
            .value = "1",
        },
        "show status where name"
    );
    failures += expect_status_rows(
        database,
        "SHOW STATUS WHERE Variable_name LIKE 'Threads\\_%'",
        expected_thread_rows,
        sizeof(expected_thread_rows) / sizeof(expected_thread_rows[0]),
        "show status where name like"
    );
    failures += expect_single_status_row(
        database,
        "SHOW STATUS WHERE Value = 'OFF'",
        (struct expected_status_row){
            .name = "Compression",
            .value = "OFF",
        },
        "show status where value"
    );
    failures += expect_single_status_row(
        database,
        "SHOW STATUS WHERE Variable_name IN ('Threads_connected','missing')",
        (struct expected_status_row){
            .name = "Threads_connected",
            .value = "1",
        },
        "show status where in"
    );
    failures += expect_status_rows(
        database,
        "SHOW GLOBAL STATUS WHERE Variable_name = 'Compression'",
        NULL,
        0U,
        "show global status where omits compression"
    );
    failures += expect_diagnostics_row(database, "show status row count state");

    mylite_close(database);
    return failures;
}

static int test_show_status_state_and_file_safety(void) {
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open status file");
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        catalog_generation = session->catalog_generation;
        sqlite_schema_generation = session->sqlite_schema_generation;
    }

    failures += expect_single_status_row(
        database,
        "SHOW STATUS LIKE 'Threads_connected'",
        (struct expected_status_row){
            .name = "Threads_connected",
            .value = "1",
        },
        "show status file result"
    );
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_uint64(
            session->catalog_generation,
            catalog_generation,
            "show status leaves catalog generation"
        );
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_schema_generation,
            "show status leaves SQLite schema generation"
        );
    }
    failures +=
        expect_int(read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)), 0, "preamble");
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "show status leaves preamble"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen status file");
    failures += expect_single_status_row(
        database,
        "SHOW STATUS LIKE 'Threads_connected'",
        (struct expected_status_row){
            .name = "Threads_connected",
            .value = "1",
        },
        "show status after reopen"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_show_status_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open status diagnostics");
    failures += execute_error(
        database,
        "SHOW FULL STATUS",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW STATUS LIKE 'Threads%' WHERE Variable_name = 'Threads_connected'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW STATUS ORDER BY Variable_name",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW STATUS LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW STATUS LIKE 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW STATUS LIKE NULL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW STATUS LIKE N'Threads%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_show_status_independent_handles(void) {
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first status handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second status handle");
    failures += expect_single_status_row(
        first,
        "SHOW STATUS LIKE 'Threads_connected'",
        (struct expected_status_row){
            .name = "Threads_connected",
            .value = "1",
        },
        "first handle status"
    );
    failures += expect_single_status_row(
        second,
        "SHOW STATUS LIKE 'Threads_connected'",
        (struct expected_status_row){
            .name = "Threads_connected",
            .value = "1",
        },
        "second handle status"
    );
    failures += expect_single_status_row(
        first,
        "SHOW SESSION STATUS LIKE 'Compression'",
        (struct expected_status_row){
            .name = "Compression",
            .value = "OFF",
        },
        "first handle compression"
    );
    failures += expect_single_status_row(
        second,
        "SHOW LOCAL STATUS LIKE 'Compression'",
        (struct expected_status_row){
            .name = "Compression",
            .value = "OFF",
        },
        "second handle compression"
    );

    mylite_close(first);
    mylite_close(second);
    return failures;
}

static int expect_status_rows(
    mylite_db *database,
    const char *sql,
    const struct expected_status_row *expected_rows,
    size_t expected_row_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), status_column_count, context);
    for (size_t column = 0U; column < status_column_count; ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            status_columns[column],
            context
        );
    }
    failures += expect_size(mylite_result_row_count(result), expected_row_count, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    for (size_t row = 0U; row < expected_row_count; ++row) {
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 0U),
            expected_rows[row].name,
            context
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, row, 1U),
            expected_rows[row].value,
            context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int expect_single_status_row(
    mylite_db *database,
    const char *sql,
    struct expected_status_row expected,
    const char *context
) {
    return expect_status_rows(database, sql, &expected, 1U, context);
}

static int expect_status_row_count(
    mylite_db *database,
    const char *sql,
    size_t expected_row_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), status_column_count, context);
    for (size_t column = 0U; column < status_column_count; ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            status_columns[column],
            context
        );
    }
    failures += expect_size(mylite_result_row_count(result), expected_row_count, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);

    mylite_result_free(result);
    return failures;
}

static int expect_diagnostics_row(mylite_db *database, const char *context) {
    static const char *const expected_values[diagnostics_column_count] = {"-1", "0", "0"};
    mylite_result *result = NULL;
    int failures =
        execute_ok(database, "SELECT ROW_COUNT(), @@warning_count, @@error_count", &result);

    if (result != NULL) {
        failures +=
            expect_size(mylite_result_column_count(result), diagnostics_column_count, context);
        failures += expect_size(mylite_result_row_count(result), 1U, context);
        failures += expect_size(mylite_result_warning_count(result), 0U, context);
        for (size_t column = 0U; column < diagnostics_column_count; ++column) {
            failures += expect_text_or_null(
                mylite_result_column_name(result, column),
                diagnostics_columns[column],
                context
            );
            failures += expect_text_or_null(
                mylite_result_value_text(result, 0U, column),
                expected_values[column],
                context
            );
        }
    } else {
        failures += 1;
    }

    mylite_result_free(result);
    return failures;
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
        "%s/mylite_show_status_%d_%s.mylite",
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
