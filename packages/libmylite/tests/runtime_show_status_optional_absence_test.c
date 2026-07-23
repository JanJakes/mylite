#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    status_column_count = 2,
};

struct status_absence_query {
    const char *sql;
    const char *context;
};

static const char *const status_columns[status_column_count] = {
    "Variable_name",
    "Value",
};

static const char *const absent_status_variables[] = {
    "Audit_log_current_size",
    "Audit_log_direct_writes",
    "Audit_log_event_max_drop_size",
    "Audit_log_events",
    "Audit_log_events_filtered",
    "Audit_log_events_lost",
    "Audit_log_events_written",
    "Audit_log_total_size",
    "Audit_log_write_waits",
    "Authentication_ldap_sasl_supported_methods",
    "Com_show_authors",
    "Com_show_contributors",
    "Com_show_ndb_status",
    "dragnet.Status",
    "Firewall_access_denied",
    "Firewall_access_granted",
    "Firewall_access_suspicious",
    "Firewall_cached_entries",
    "Gr_all_consensus_proposals_count",
    "Gr_all_consensus_time_sum",
    "Gr_certification_garbage_collector_count",
    "Gr_certification_garbage_collector_time_sum",
    "Gr_consensus_bytes_received_sum",
    "Gr_consensus_bytes_sent_sum",
    "Gr_control_messages_sent_bytes_sum",
    "Gr_control_messages_sent_count",
    "Gr_control_messages_sent_roundtrip_time_sum",
    "Gr_data_messages_sent_bytes_sum",
    "Gr_data_messages_sent_count",
    "Gr_data_messages_sent_roundtrip_time_sum",
    "Gr_empty_consensus_proposals_count",
    "Gr_extended_consensus_count",
    "Gr_flow_control_throttle_active_count",
    "Gr_flow_control_throttle_count",
    "Gr_flow_control_throttle_last_throttle_timestamp",
    "Gr_flow_control_throttle_time_sum",
    "Gr_last_consensus_end_timestamp",
    "Gr_total_messages_sent_count",
    "Gr_transactions_consistency_after_sync_count",
    "Gr_transactions_consistency_after_sync_time_sum",
    "Gr_transactions_consistency_after_termination_count",
    "Gr_transactions_consistency_after_termination_time_sum",
    "Gr_transactions_consistency_before_begin_count",
    "Gr_transactions_consistency_before_begin_time_sum",
    "mecab_charset",
    "Mysqlx_ssl_accept_renegotiates",
    "Ndb_api_adaptive_send_deferred_count",
    "Ndb_api_adaptive_send_deferred_count_replica",
    "Ndb_api_adaptive_send_deferred_count_session",
    "Ndb_api_adaptive_send_deferred_count_slave",
    "Ndb_api_adaptive_send_forced_count",
    "Ndb_api_adaptive_send_forced_count_replica",
    "Ndb_api_adaptive_send_forced_count_session",
    "Ndb_api_adaptive_send_forced_count_slave",
    "Ndb_api_adaptive_send_unforced_count",
    "Ndb_api_adaptive_send_unforced_count_replica",
    "Ndb_api_adaptive_send_unforced_count_session",
    "Ndb_api_adaptive_send_unforced_count_slave",
    "Ndb_api_bytes_received_count",
    "Ndb_api_bytes_received_count_replica",
    "Ndb_api_bytes_received_count_session",
    "Ndb_api_bytes_received_count_slave",
    "Ndb_api_bytes_sent_count",
    "Ndb_api_bytes_sent_count_replica",
    "Ndb_api_bytes_sent_count_session",
    "Ndb_api_bytes_sent_count_slave",
    "Ndb_api_event_bytes_count",
    "Ndb_api_event_bytes_count_injector",
    "Ndb_api_event_data_count",
    "Ndb_api_event_data_count_injector",
    "Ndb_api_event_nondata_count",
    "Ndb_api_event_nondata_count_injector",
    "Ndb_api_pk_op_count",
    "Ndb_api_pk_op_count_replica",
    "Ndb_api_pk_op_count_session",
    "Ndb_api_pk_op_count_slave",
    "Ndb_api_pruned_scan_count",
    "Ndb_api_pruned_scan_count_replica",
    "Ndb_api_pruned_scan_count_session",
    "Ndb_api_pruned_scan_count_slave",
    "Ndb_api_range_scan_count",
    "Ndb_api_range_scan_count_replica",
    "Ndb_api_range_scan_count_session",
    "Ndb_api_range_scan_count_slave",
    "Ndb_api_read_row_count",
    "Ndb_api_read_row_count_replica",
    "Ndb_api_read_row_count_session",
    "Ndb_api_read_row_count_slave",
    "Ndb_api_scan_batch_count",
    "Ndb_api_scan_batch_count_replica",
    "Ndb_api_scan_batch_count_session",
    "Ndb_api_scan_batch_count_slave",
    "Ndb_api_table_scan_count",
    "Ndb_api_table_scan_count_replica",
    "Ndb_api_table_scan_count_session",
    "Ndb_api_table_scan_count_slave",
    "Ndb_api_trans_abort_count",
    "Ndb_api_trans_abort_count_replica",
    "Ndb_api_trans_abort_count_session",
    "Ndb_api_trans_abort_count_slave",
    "Ndb_api_trans_close_count",
    "Ndb_api_trans_close_count_replica",
    "Ndb_api_trans_close_count_session",
    "Ndb_api_trans_close_count_slave",
    "Ndb_api_trans_commit_count",
    "Ndb_api_trans_commit_count_replica",
    "Ndb_api_trans_commit_count_session",
    "Ndb_api_trans_commit_count_slave",
    "Ndb_api_trans_local_read_row_count",
    "Ndb_api_trans_local_read_row_count_replica",
    "Ndb_api_trans_local_read_row_count_session",
    "Ndb_api_trans_local_read_row_count_slave",
    "Ndb_api_trans_start_count",
    "Ndb_api_trans_start_count_replica",
    "Ndb_api_trans_start_count_session",
    "Ndb_api_trans_start_count_slave",
    "Ndb_api_uk_op_count",
    "Ndb_api_uk_op_count_replica",
    "Ndb_api_uk_op_count_session",
    "Ndb_api_uk_op_count_slave",
    "Ndb_api_wait_exec_complete_count",
    "Ndb_api_wait_exec_complete_count_replica",
    "Ndb_api_wait_exec_complete_count_session",
    "Ndb_api_wait_exec_complete_count_slave",
    "Ndb_api_wait_meta_request_count",
    "Ndb_api_wait_meta_request_count_replica",
    "Ndb_api_wait_meta_request_count_session",
    "Ndb_api_wait_meta_request_count_slave",
    "Ndb_api_wait_nanos_count",
    "Ndb_api_wait_nanos_count_replica",
    "Ndb_api_wait_nanos_count_session",
    "Ndb_api_wait_nanos_count_slave",
    "Ndb_api_wait_scan_result_count",
    "Ndb_api_wait_scan_result_count_replica",
    "Ndb_api_wait_scan_result_count_session",
    "Ndb_api_wait_scan_result_count_slave",
    "Ndb_cluster_node_id",
    "Ndb_config_from_host",
    "Ndb_config_from_port",
    "Ndb_config_generation",
    "Ndb_conflict_fn_epoch",
    "Ndb_conflict_fn_epoch_trans",
    "Ndb_conflict_fn_epoch2",
    "Ndb_conflict_fn_epoch2_trans",
    "Ndb_conflict_fn_max",
    "Ndb_conflict_fn_max_del_win",
    "Ndb_conflict_fn_max_del_win_ins",
    "Ndb_conflict_fn_max_ins",
    "Ndb_conflict_fn_old",
    "Ndb_conflict_last_conflict_epoch",
    "Ndb_conflict_last_stable_epoch",
    "Ndb_conflict_reflected_op_discard_count",
    "Ndb_conflict_reflected_op_prepare_count",
    "Ndb_conflict_refresh_op_count",
    "Ndb_conflict_trans_conflict_commit_count",
    "Ndb_conflict_trans_detect_iter_count",
    "Ndb_conflict_trans_reject_count",
    "Ndb_conflict_trans_row_conflict_count",
    "Ndb_conflict_trans_row_reject_count",
    "Ndb_epoch_delete_delete_count",
    "Ndb_execute_count",
    "Ndb_fetch_table_stats",
    "Ndb_last_commit_epoch_server",
    "Ndb_last_commit_epoch_session",
    "Ndb_metadata_detected_count",
    "Ndb_metadata_excluded_count",
    "Ndb_metadata_synced_count",
    "Ndb_number_of_data_nodes",
    "Ndb_pruned_scan_count",
    "Ndb_pushed_queries_defined",
    "Ndb_pushed_queries_dropped",
    "Ndb_pushed_queries_executed",
    "Ndb_pushed_reads",
    "Ndb_scan_count",
    "Ndb_slave_max_replicated_epoch",
    "Ndb_trans_hint_count_session",
    "Ongoing_anonymous_gtid_violating_transaction_count",
    "Ongoing_automatic_gtid_violating_transaction_count",
    "Rewriter_number_loaded_rules",
    "Rewriter_number_reloads",
    "Rewriter_number_rewritten_queries",
    "Rewriter_reload_error",
    "Rpl_semi_sync_master_clients",
    "Rpl_semi_sync_master_net_avg_wait_time",
    "Rpl_semi_sync_master_net_wait_time",
    "Rpl_semi_sync_master_net_waits",
    "Rpl_semi_sync_master_no_times",
    "Rpl_semi_sync_master_no_tx",
    "Rpl_semi_sync_master_status",
    "Rpl_semi_sync_master_timefunc_failures",
    "Rpl_semi_sync_master_tx_avg_wait_time",
    "Rpl_semi_sync_master_tx_wait_time",
    "Rpl_semi_sync_master_tx_waits",
    "Rpl_semi_sync_master_wait_pos_backtraverse",
    "Rpl_semi_sync_master_wait_sessions",
    "Rpl_semi_sync_master_yes_tx",
    "Rpl_semi_sync_replica_status",
    "Rpl_semi_sync_slave_status",
    "Rpl_semi_sync_source_clients",
    "Rpl_semi_sync_source_net_avg_wait_time",
    "Rpl_semi_sync_source_net_wait_time",
    "Rpl_semi_sync_source_net_waits",
    "Rpl_semi_sync_source_no_times",
    "Rpl_semi_sync_source_no_tx",
    "Rpl_semi_sync_source_status",
    "Rpl_semi_sync_source_timefunc_failures",
    "Rpl_semi_sync_source_tx_avg_wait_time",
    "Rpl_semi_sync_source_tx_wait_time",
    "Rpl_semi_sync_source_tx_waits",
    "Rpl_semi_sync_source_wait_pos_backtraverse",
    "Rpl_semi_sync_source_wait_sessions",
    "Rpl_semi_sync_source_yes_tx",
    "Slave_rows_last_search_algorithm_used",
    "telemetry.live_sessions",
    "validate_password_dictionary_file_last_parsed",
    "validate_password_dictionary_file_words_count",
    "validate_password.dictionary_file_last_parsed",
    "validate_password.dictionary_file_words_count",
};

static int test_optional_status_absence(void);
static char *make_absence_query(const char *show_status_prefix);
static int append_sql(char **cursor, size_t *remaining, const char *text);
static int expect_empty_status_result(mylite_db *database, struct status_absence_query query);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);

int main(void) {
    return test_optional_status_absence() == 0 ? 0 : 1;
}

static int test_optional_status_absence(void) {
    static const char *const prefixes[] = {
        "SHOW STATUS",
        "SHOW SESSION STATUS",
        "SHOW LOCAL STATUS",
        "SHOW GLOBAL STATUS",
    };
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open memory database");

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

        failures += expect_empty_status_result(
            database,
            (struct status_absence_query){.sql = query, .context = prefixes[index]}
        );
        free(query);
    }

    mylite_close(database);
    return failures;
}

static char *make_absence_query(const char *show_status_prefix) {
    size_t length = strlen(show_status_prefix) + strlen(" WHERE Variable_name IN ()") + 1U;

    for (size_t index = 0U;
         index < sizeof(absent_status_variables) / sizeof(absent_status_variables[0]);
         ++index) {
        length += strlen(absent_status_variables[index]) + strlen("'', ");
    }

    char *query = malloc(length);
    if (query == NULL) {
        return NULL;
    }

    char *cursor = query;
    size_t remaining = length;
    if (append_sql(&cursor, &remaining, show_status_prefix) != 0 ||
        append_sql(&cursor, &remaining, " WHERE Variable_name IN (") != 0) {
        free(query);
        return NULL;
    }

    for (size_t index = 0U;
         index < sizeof(absent_status_variables) / sizeof(absent_status_variables[0]);
         ++index) {
        if (index > 0U && append_sql(&cursor, &remaining, ", ") != 0) {
            free(query);
            return NULL;
        }
        if (append_sql(&cursor, &remaining, "'") != 0 ||
            append_sql(&cursor, &remaining, absent_status_variables[index]) != 0 ||
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

static int expect_empty_status_result(mylite_db *database, struct status_absence_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (result == NULL) {
        return failures + 1;
    }

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        status_column_count,
        query.context
    );
    for (size_t column = 0U; column < status_column_count; ++column) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column),
            status_columns[column],
            query.context
        );
    }
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, query.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, query.context);
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, query.context);

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
