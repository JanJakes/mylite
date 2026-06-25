#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    variable_column_count = 2,
    mysql_unknown_system_variable_error = 1193,
};

struct variable_absence_query {
    const char *sql;
    const char *context;
};

struct unknown_variable_query {
    const char *sql;
    const char *name;
};

static const char *const variable_columns[variable_column_count] = {
    "Variable_name",
    "Value",
};

static const char *const absent_system_variables[] = {
    "audit_log_buffer_size",
    "audit_log_compression",
    "audit_log_connection_policy",
    "audit_log_current_session",
    "audit_log_database",
    "audit_log_disable",
    "audit_log_encryption",
    "audit_log_exclude_accounts",
    "audit_log_file",
    "audit_log_filter_id",
    "audit_log_flush",
    "audit_log_flush_interval_seconds",
    "audit_log_format",
    "audit_log_format_unix_timestamp",
    "audit_log_include_accounts",
    "audit_log_password_history_keep_days",
    "audit_log_policy",
    "audit_log_prune_seconds",
    "audit_log_read_buffer_size",
    "audit_log_rotate_on_size",
    "audit_log_statement_policy",
    "audit_log_strategy",
    "authentication_kerberos_service_key_tab",
    "authentication_kerberos_service_principal",
    "authentication_ldap_sasl_auth_method_name",
    "authentication_ldap_sasl_bind_base_dn",
    "authentication_ldap_sasl_bind_root_dn",
    "authentication_ldap_sasl_bind_root_pwd",
    "authentication_ldap_sasl_ca_path",
    "authentication_ldap_sasl_connect_timeout",
    "authentication_ldap_sasl_group_search_attr",
    "authentication_ldap_sasl_group_search_filter",
    "authentication_ldap_sasl_init_pool_size",
    "authentication_ldap_sasl_log_status",
    "authentication_ldap_sasl_max_pool_size",
    "authentication_ldap_sasl_referral",
    "authentication_ldap_sasl_response_timeout",
    "authentication_ldap_sasl_server_host",
    "authentication_ldap_sasl_server_port",
    "authentication_ldap_sasl_tls",
    "authentication_ldap_sasl_user_search_attr",
    "authentication_ldap_simple_auth_method_name",
    "authentication_ldap_simple_bind_base_dn",
    "authentication_ldap_simple_bind_root_dn",
    "authentication_ldap_simple_bind_root_pwd",
    "authentication_ldap_simple_ca_path",
    "authentication_ldap_simple_connect_timeout",
    "authentication_ldap_simple_group_search_attr",
    "authentication_ldap_simple_group_search_filter",
    "authentication_ldap_simple_init_pool_size",
    "authentication_ldap_simple_log_status",
    "authentication_ldap_simple_max_pool_size",
    "authentication_ldap_simple_referral",
    "authentication_ldap_simple_response_timeout",
    "authentication_ldap_simple_server_host",
    "authentication_ldap_simple_server_port",
    "authentication_ldap_simple_tls",
    "authentication_ldap_simple_user_search_attr",
    "authentication_webauthn_rp_id",
    "authentication_windows_log_level",
    "authentication_windows_use_principal_name",
    "clone_autotune_concurrency",
    "clone_block_ddl",
    "clone_buffer_size",
    "clone_ddl_timeout",
    "clone_delay_after_data_drop",
    "clone_donor_timeout_after_network_failure",
    "clone_enable_compression",
    "clone_max_concurrency",
    "clone_max_data_bandwidth",
    "clone_max_network_bandwidth",
    "clone_ssl_ca",
    "clone_ssl_cert",
    "clone_ssl_key",
    "clone_valid_donor_list",
    "component_masking.dictionaries_flush_interval_seconds",
    "component_masking.masking_database",
    "component_scheduler.enabled",
    "debug",
    "debug_sync",
    "dragnet.log_error_filter_rules",
    "enterprise_encryption.maximum_rsa_key_size",
    "enterprise_encryption.rsa_support_legacy_padding",
    "group_replication_advertise_recovery_endpoints",
    "group_replication_allow_local_lower_version_join",
    "group_replication_auto_increment_increment",
    "group_replication_autorejoin_tries",
    "group_replication_bootstrap_group",
    "group_replication_clone_threshold",
    "group_replication_communication_debug_options",
    "group_replication_communication_max_message_size",
    "group_replication_communication_stack",
    "group_replication_components_stop_timeout",
    "group_replication_compression_threshold",
    "group_replication_enforce_update_everywhere_checks",
    "group_replication_exit_state_action",
    "group_replication_flow_control_applier_threshold",
    "group_replication_flow_control_certifier_threshold",
    "group_replication_flow_control_hold_percent",
    "group_replication_flow_control_max_quota",
    "group_replication_flow_control_member_quota_percent",
    "group_replication_flow_control_min_quota",
    "group_replication_flow_control_min_recovery_quota",
    "group_replication_flow_control_mode",
    "group_replication_flow_control_period",
    "group_replication_flow_control_release_percent",
    "group_replication_force_members",
    "group_replication_group_name",
    "group_replication_group_seeds",
    "group_replication_gtid_assignment_block_size",
    "group_replication_ip_allowlist",
    "group_replication_local_address",
    "group_replication_member_expel_timeout",
    "group_replication_member_weight",
    "group_replication_message_cache_size",
    "group_replication_paxos_single_leader",
    "group_replication_poll_spin_loops",
    "group_replication_preemptive_garbage_collection",
    "group_replication_preemptive_garbage_collection_rows_threshold",
    "group_replication_recovery_compression_algorithms",
    "group_replication_recovery_get_public_key",
    "group_replication_recovery_public_key_path",
    "group_replication_recovery_reconnect_interval",
    "group_replication_recovery_retry_count",
    "group_replication_recovery_ssl_ca",
    "group_replication_recovery_ssl_capath",
    "group_replication_recovery_ssl_cert",
    "group_replication_recovery_ssl_cipher",
    "group_replication_recovery_ssl_crl",
    "group_replication_recovery_ssl_crlpath",
    "group_replication_recovery_ssl_key",
    "group_replication_recovery_ssl_verify_server_cert",
    "group_replication_recovery_tls_ciphersuites",
    "group_replication_recovery_tls_version",
    "group_replication_recovery_use_ssl",
    "group_replication_recovery_zstd_compression_level",
    "group_replication_single_primary_mode",
    "group_replication_ssl_mode",
    "group_replication_start_on_boot",
    "group_replication_tls_source",
    "group_replication_transaction_size_limit",
    "group_replication_unreachable_majority_timeout",
    "group_replication_view_change_uuid",
    "innodb_background_drop_list_empty",
    "innodb_buffer_pool_debug",
    "innodb_change_buffering_debug",
    "innodb_checkpoint_disabled",
    "innodb_compress_debug",
    "innodb_ddl_log_crash_reset_debug",
    "innodb_fil_make_page_dirty_debug",
    "innodb_limit_optimistic_insert_debug",
    "innodb_log_checkpoint_fuzzy_now",
    "innodb_log_checkpoint_now",
    "innodb_merge_threshold_set_all_debug",
    "innodb_numa_interleave",
    "innodb_saved_page_number_debug",
    "innodb_sync_debug",
    "innodb_trx_purge_view_update_only_debug",
    "innodb_trx_rseg_n_slots_debug",
    "keyring_aws_cmk_id",
    "keyring_aws_conf_file",
    "keyring_aws_data_file",
    "keyring_aws_region",
    "keyring_hashicorp_auth_path",
    "keyring_hashicorp_ca_path",
    "keyring_hashicorp_caching",
    "keyring_hashicorp_commit_auth_path",
    "keyring_hashicorp_commit_ca_path",
    "keyring_hashicorp_commit_caching",
    "keyring_hashicorp_commit_role_id",
    "keyring_hashicorp_commit_server_url",
    "keyring_hashicorp_commit_store_path",
    "keyring_hashicorp_role_id",
    "keyring_hashicorp_secret_id",
    "keyring_hashicorp_server_url",
    "keyring_hashicorp_store_path",
    "keyring_okv_conf_dir",
    "lock_order",
    "lock_order_debug_loop",
    "lock_order_debug_missing_arc",
    "lock_order_debug_missing_key",
    "lock_order_debug_missing_unlock",
    "lock_order_dependencies",
    "lock_order_extra_dependencies",
    "lock_order_output_directory",
    "lock_order_print_txt",
    "lock_order_trace_loop",
    "lock_order_trace_missing_arc",
    "lock_order_trace_missing_key",
    "lock_order_trace_missing_unlock",
    "mecab_rc_file",
    "mysql_firewall_database",
    "mysql_firewall_mode",
    "mysql_firewall_reload_interval_seconds",
    "mysql_firewall_trace",
    "named_pipe",
    "named_pipe_full_access_group",
    "ndb_allow_copying_alter_table",
    "ndb_applier_allow_skip_epoch",
    "ndb_autoincrement_prefetch_sz",
    "ndb_batch_size",
    "ndb_blob_read_batch_bytes",
    "ndb_blob_write_batch_bytes",
    "ndb_clear_apply_status",
    "ndb_cluster_connection_pool",
    "ndb_cluster_connection_pool_nodeids",
    "ndb_conflict_role",
    "ndb_data_node_neighbour",
    "ndb_dbg_check_shares",
    "ndb_default_column_format",
    "ndb_deferred_constraints",
    "ndb_distribution",
    "ndb_eventbuffer_free_percent",
    "ndb_eventbuffer_max_alloc",
    "ndb_extra_logging",
    "ndb_force_send",
    "ndb_fully_replicated",
    "ndb_index_stat_enable",
    "ndb_index_stat_option",
    "ndb_join_pushdown",
    "ndb_log_apply_status",
    "ndb_log_bin",
    "ndb_log_binlog_index",
    "ndb_log_cache_size",
    "ndb_log_empty_epochs",
    "ndb_log_empty_update",
    "ndb_log_exclusive_reads",
    "ndb_log_fail_terminate",
    "ndb_log_orig",
    "ndb_log_transaction_compression",
    "ndb_log_transaction_compression_level_zstd",
    "ndb_log_transaction_dependency",
    "ndb_log_transaction_id",
    "ndb_log_update_as_write",
    "ndb_log_update_minimal",
    "ndb_log_updated_only",
    "ndb_metadata_check",
    "ndb_metadata_check_interval",
    "ndb_metadata_sync",
    "ndb_mgm_tls",
    "ndb_optimization_delay",
    "ndb_optimized_node_selection",
    "ndb_read_backup",
    "ndb_recv_thread_activation_threshold",
    "ndb_recv_thread_cpu_mask",
    "ndb_replica_batch_size",
    "ndb_replica_blob_write_batch_bytes",
    "Ndb_replica_max_replicated_epoch",
    "ndb_report_thresh_binlog_epoch_slip",
    "ndb_report_thresh_binlog_mem_usage",
    "ndb_row_checksum",
    "ndb_schema_dist_lock_wait_timeout",
    "ndb_schema_dist_timeout",
    "ndb_schema_dist_upgrade_allowed",
    "Ndb_schema_participant_count",
    "ndb_show_foreign_key_mock_tables",
    "ndb_slave_conflict_role",
    "Ndb_system_name",
    "ndb_table_no_logging",
    "ndb_table_temporary",
    "ndb_tls_search_path",
    "ndb_use_copying_alter_table",
    "ndb_use_exact_count",
    "ndb_use_transactions",
    "ndb_version",
    "ndb_version_string",
    "ndb_wait_connected",
    "ndb_wait_setup",
    "ndbinfo_database",
    "ndbinfo_max_bytes",
    "ndbinfo_max_rows",
    "ndbinfo_offline",
    "ndbinfo_show_hidden",
    "ndbinfo_table_prefix",
    "ndbinfo_version",
    "rewriter_enabled",
    "rewriter_enabled_for_threads_without_privilege_checks",
    "rewriter_verbose",
    "rpl_semi_sync_master_enabled",
    "rpl_semi_sync_master_timeout",
    "rpl_semi_sync_master_trace_level",
    "rpl_semi_sync_master_wait_for_slave_count",
    "rpl_semi_sync_master_wait_no_slave",
    "rpl_semi_sync_master_wait_point",
    "rpl_semi_sync_replica_enabled",
    "rpl_semi_sync_replica_trace_level",
    "rpl_semi_sync_slave_enabled",
    "rpl_semi_sync_slave_trace_level",
    "rpl_semi_sync_source_enabled",
    "rpl_semi_sync_source_timeout",
    "rpl_semi_sync_source_trace_level",
    "rpl_semi_sync_source_wait_for_replica_count",
    "rpl_semi_sync_source_wait_no_replica",
    "rpl_semi_sync_source_wait_point",
    "shared_memory",
    "shared_memory_base_name",
    "syseventlog.facility",
    "syseventlog.include_pid",
    "syseventlog.tag",
    "telemetry.metrics_enabled",
    "telemetry.metrics_reader_frequency_1",
    "telemetry.metrics_reader_frequency_2",
    "telemetry.metrics_reader_frequency_3",
    "telemetry.otel_bsp_max_export_batch_size",
    "telemetry.otel_bsp_max_queue_size",
    "telemetry.otel_bsp_schedule_delay",
    "telemetry.otel_exporter_otlp_metrics_certificates",
    "telemetry.otel_exporter_otlp_metrics_cipher",
    "telemetry.otel_exporter_otlp_metrics_cipher_suite",
    "telemetry.otel_exporter_otlp_metrics_client_certificates",
    "telemetry.otel_exporter_otlp_metrics_client_key",
    "telemetry.otel_exporter_otlp_metrics_compression",
    "telemetry.otel_exporter_otlp_metrics_endpoint",
    "telemetry.otel_exporter_otlp_metrics_headers",
    "telemetry.otel_exporter_otlp_metrics_max_tls",
    "telemetry.otel_exporter_otlp_metrics_min_tls",
    "telemetry.otel_exporter_otlp_metrics_protocol",
    "telemetry.otel_exporter_otlp_metrics_timeout",
    "telemetry.otel_exporter_otlp_traces_certificates",
    "telemetry.otel_exporter_otlp_traces_cipher",
    "telemetry.otel_exporter_otlp_traces_cipher_suite",
    "telemetry.otel_exporter_otlp_traces_client_certificates",
    "telemetry.otel_exporter_otlp_traces_client_key",
    "telemetry.otel_exporter_otlp_traces_compression",
    "telemetry.otel_exporter_otlp_traces_endpoint",
    "telemetry.otel_exporter_otlp_traces_headers",
    "telemetry.otel_exporter_otlp_traces_max_tls",
    "telemetry.otel_exporter_otlp_traces_min_tls",
    "telemetry.otel_exporter_otlp_traces_protocol",
    "telemetry.otel_exporter_otlp_traces_timeout",
    "telemetry.otel_log_level",
    "telemetry.otel_resource_attributes",
    "telemetry.query_text_enabled",
    "telemetry.trace_enabled",
    "thread_pool_algorithm",
    "thread_pool_dedicated_listeners",
    "thread_pool_high_priority_connection",
    "thread_pool_longrun_trx_limit",
    "thread_pool_max_active_query_threads",
    "thread_pool_max_transactions_limit",
    "thread_pool_max_unused_threads",
    "thread_pool_prio_kickup_timer",
    "thread_pool_query_threads_per_group",
    "thread_pool_size",
    "thread_pool_stall_limit",
    "thread_pool_transaction_delay",
    "validate_password_check_user_name",
    "validate_password_dictionary_file",
    "validate_password_length",
    "validate_password_mixed_case_count",
    "validate_password_number_count",
    "validate_password_policy",
    "validate_password_special_char_count",
    "validate_password.changed_characters_percentage",
    "validate_password.check_user_name",
    "validate_password.dictionary_file",
    "validate_password.length",
    "validate_password.mixed_case_count",
    "validate_password.number_count",
    "validate_password.policy",
    "validate_password.special_char_count",
    "version_tokens_session",
    "version_tokens_session_number",
};

static const struct unknown_variable_query unknown_variable_queries[] = {
    {.sql = "SELECT @@audit_log_buffer_size", .name = "audit_log_buffer_size"},
    {.sql = "SELECT @@component_masking.masking_database",
     .name = "component_masking.masking_database"},
    {.sql = "SELECT @@Ndb_system_name", .name = "Ndb_system_name"},
    {.sql = "SELECT @@validate_password.length", .name = "validate_password.length"},
};

static int test_optional_system_variable_absence(void);
static char *make_absence_query(const char *show_variables_prefix);
static int append_sql(char **cursor, size_t *remaining, const char *text);
static int expect_empty_variables_result(mylite_db *database, struct variable_absence_query query);
static int expect_unknown_system_variable(mylite_db *database, struct unknown_variable_query query);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_optional_system_variable_absence() == 0 ? 0 : 1;
}

static int test_optional_system_variable_absence(void) {
    static const char *const prefixes[] = {
        "SHOW VARIABLES",
        "SHOW SESSION VARIABLES",
        "SHOW LOCAL VARIABLES",
        "SHOW GLOBAL VARIABLES",
    };
    mylite_db *database = NULL;
    int failures = expect_int(mylite_open_memory(&database), MYLITE_OK, "open memory database");

    if (database == NULL) {
        return failures + 1;
    }

    for (size_t index = 0U; index < sizeof(prefixes) / sizeof(prefixes[0]); ++index) {
        char *query = make_absence_query(prefixes[index]);

        if (query == NULL) {
            fprintf(stderr, "%s query allocation failed\n", prefixes[index]);
            failures += 1;
            continue;
        }

        failures += expect_empty_variables_result(
            database,
            (struct variable_absence_query){.sql = query, .context = prefixes[index]}
        );
        free(query);
    }

    for (size_t index = 0U;
         index < sizeof(unknown_variable_queries) / sizeof(unknown_variable_queries[0]);
         ++index) {
        failures += expect_unknown_system_variable(database, unknown_variable_queries[index]);
    }

    mylite_close(database);
    return failures;
}

static char *make_absence_query(const char *show_variables_prefix) {
    size_t length = strlen(show_variables_prefix) + strlen(" WHERE Variable_name IN ()") + 1U;

    for (size_t index = 0U;
         index < sizeof(absent_system_variables) / sizeof(absent_system_variables[0]);
         ++index) {
        length += strlen(absent_system_variables[index]) + strlen("'', ");
    }

    char *query = malloc(length);
    if (query == NULL) {
        return NULL;
    }

    char *cursor = query;
    size_t remaining = length;
    if (append_sql(&cursor, &remaining, show_variables_prefix) != 0 ||
        append_sql(&cursor, &remaining, " WHERE Variable_name IN (") != 0) {
        free(query);
        return NULL;
    }

    for (size_t index = 0U;
         index < sizeof(absent_system_variables) / sizeof(absent_system_variables[0]);
         ++index) {
        if (index > 0U && append_sql(&cursor, &remaining, ", ") != 0) {
            free(query);
            return NULL;
        }
        if (append_sql(&cursor, &remaining, "'") != 0 ||
            append_sql(&cursor, &remaining, absent_system_variables[index]) != 0 ||
            append_sql(&cursor, &remaining, "'") != 0) {
            free(query);
            return NULL;
        }
    }

    if (append_sql(&cursor, &remaining, ")") != 0) {
        free(query);
        return NULL;
    }

    return query;
}

static int append_sql(char **cursor, size_t *remaining, const char *text) {
    int written = snprintf(*cursor, *remaining, "%s", text);

    if (written < 0 || (size_t)written >= *remaining) {
        return 1;
    }

    *cursor += written;
    *remaining -= (size_t)written;
    return 0;
}

static int expect_empty_variables_result(mylite_db *database, struct variable_absence_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (result == NULL) {
        return failures + 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), variable_column_count, query.context);
    for (size_t column = 0U; column < variable_column_count; ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            variable_columns[column],
            query.context
        );
    }
    failures += expect_size(mylite_result_row_count(result), 0U, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);

    mylite_result_free(result);
    return failures;
}

static int expect_unknown_system_variable(
    mylite_db *database,
    struct unknown_variable_query query
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);

    if (result != NULL) {
        mylite_result_free(result);
    }
    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected unknown system variable error\n", query.sql);
        return 1;
    }

    const struct mylite_diagnostics *diagnostics = mylite_connection_diagnostics(database);
    int failures = expect_int(
        mylite_diagnostics_errcode(diagnostics),
        mysql_unknown_system_variable_error,
        query.sql
    );

    failures += expect_text_or_null(mylite_diagnostics_sqlstate(diagnostics), "HY000", query.sql);
    failures += expect_text_contains(mylite_diagnostics_errmsg(diagnostics), query.name, query.sql);
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

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
        );
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
            "%s: expected text [%s], got [%s]\n",
            context,
            expected != NULL ? expected : "(null)",
            actual != NULL ? actual : "(null)"
        );
        return 1;
    }

    return 0;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected text containing [%s], got [%s]\n",
            context,
            needle,
            actual != NULL ? actual : "(null)"
        );
        return 1;
    }

    return 0;
}
