#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"

ABSENT_STATUS_NAMES=$(cat <<'NAMES'
Audit_log_current_size
Audit_log_direct_writes
Audit_log_event_max_drop_size
Audit_log_events
Audit_log_events_filtered
Audit_log_events_lost
Audit_log_events_written
Audit_log_total_size
Audit_log_write_waits
Authentication_ldap_sasl_supported_methods
Com_show_authors
Com_show_contributors
Com_show_ndb_status
dragnet.Status
Firewall_access_denied
Firewall_access_granted
Firewall_access_suspicious
Firewall_cached_entries
Gr_all_consensus_proposals_count
Gr_all_consensus_time_sum
Gr_certification_garbage_collector_count
Gr_certification_garbage_collector_time_sum
Gr_consensus_bytes_received_sum
Gr_consensus_bytes_sent_sum
Gr_control_messages_sent_bytes_sum
Gr_control_messages_sent_count
Gr_control_messages_sent_roundtrip_time_sum
Gr_data_messages_sent_bytes_sum
Gr_data_messages_sent_count
Gr_data_messages_sent_roundtrip_time_sum
Gr_empty_consensus_proposals_count
Gr_extended_consensus_count
Gr_flow_control_throttle_active_count
Gr_flow_control_throttle_count
Gr_flow_control_throttle_last_throttle_timestamp
Gr_flow_control_throttle_time_sum
Gr_last_consensus_end_timestamp
Gr_total_messages_sent_count
Gr_transactions_consistency_after_sync_count
Gr_transactions_consistency_after_sync_time_sum
Gr_transactions_consistency_after_termination_count
Gr_transactions_consistency_after_termination_time_sum
Gr_transactions_consistency_before_begin_count
Gr_transactions_consistency_before_begin_time_sum
mecab_charset
Mysqlx_ssl_accept_renegotiates
Ndb_api_adaptive_send_deferred_count
Ndb_api_adaptive_send_deferred_count_replica
Ndb_api_adaptive_send_deferred_count_session
Ndb_api_adaptive_send_deferred_count_slave
Ndb_api_adaptive_send_forced_count
Ndb_api_adaptive_send_forced_count_replica
Ndb_api_adaptive_send_forced_count_session
Ndb_api_adaptive_send_forced_count_slave
Ndb_api_adaptive_send_unforced_count
Ndb_api_adaptive_send_unforced_count_replica
Ndb_api_adaptive_send_unforced_count_session
Ndb_api_adaptive_send_unforced_count_slave
Ndb_api_bytes_received_count
Ndb_api_bytes_received_count_replica
Ndb_api_bytes_received_count_session
Ndb_api_bytes_received_count_slave
Ndb_api_bytes_sent_count
Ndb_api_bytes_sent_count_replica
Ndb_api_bytes_sent_count_session
Ndb_api_bytes_sent_count_slave
Ndb_api_event_bytes_count
Ndb_api_event_bytes_count_injector
Ndb_api_event_data_count
Ndb_api_event_data_count_injector
Ndb_api_event_nondata_count
Ndb_api_event_nondata_count_injector
Ndb_api_pk_op_count
Ndb_api_pk_op_count_replica
Ndb_api_pk_op_count_session
Ndb_api_pk_op_count_slave
Ndb_api_pruned_scan_count
Ndb_api_pruned_scan_count_replica
Ndb_api_pruned_scan_count_session
Ndb_api_pruned_scan_count_slave
Ndb_api_range_scan_count
Ndb_api_range_scan_count_replica
Ndb_api_range_scan_count_session
Ndb_api_range_scan_count_slave
Ndb_api_read_row_count
Ndb_api_read_row_count_replica
Ndb_api_read_row_count_session
Ndb_api_read_row_count_slave
Ndb_api_scan_batch_count
Ndb_api_scan_batch_count_replica
Ndb_api_scan_batch_count_session
Ndb_api_scan_batch_count_slave
Ndb_api_table_scan_count
Ndb_api_table_scan_count_replica
Ndb_api_table_scan_count_session
Ndb_api_table_scan_count_slave
Ndb_api_trans_abort_count
Ndb_api_trans_abort_count_replica
Ndb_api_trans_abort_count_session
Ndb_api_trans_abort_count_slave
Ndb_api_trans_close_count
Ndb_api_trans_close_count_replica
Ndb_api_trans_close_count_session
Ndb_api_trans_close_count_slave
Ndb_api_trans_commit_count
Ndb_api_trans_commit_count_replica
Ndb_api_trans_commit_count_session
Ndb_api_trans_commit_count_slave
Ndb_api_trans_local_read_row_count
Ndb_api_trans_local_read_row_count_replica
Ndb_api_trans_local_read_row_count_session
Ndb_api_trans_local_read_row_count_slave
Ndb_api_trans_start_count
Ndb_api_trans_start_count_replica
Ndb_api_trans_start_count_session
Ndb_api_trans_start_count_slave
Ndb_api_uk_op_count
Ndb_api_uk_op_count_replica
Ndb_api_uk_op_count_session
Ndb_api_uk_op_count_slave
Ndb_api_wait_exec_complete_count
Ndb_api_wait_exec_complete_count_replica
Ndb_api_wait_exec_complete_count_session
Ndb_api_wait_exec_complete_count_slave
Ndb_api_wait_meta_request_count
Ndb_api_wait_meta_request_count_replica
Ndb_api_wait_meta_request_count_session
Ndb_api_wait_meta_request_count_slave
Ndb_api_wait_nanos_count
Ndb_api_wait_nanos_count_replica
Ndb_api_wait_nanos_count_session
Ndb_api_wait_nanos_count_slave
Ndb_api_wait_scan_result_count
Ndb_api_wait_scan_result_count_replica
Ndb_api_wait_scan_result_count_session
Ndb_api_wait_scan_result_count_slave
Ndb_cluster_node_id
Ndb_config_from_host
Ndb_config_from_port
Ndb_config_generation
Ndb_conflict_fn_epoch
Ndb_conflict_fn_epoch_trans
Ndb_conflict_fn_epoch2
Ndb_conflict_fn_epoch2_trans
Ndb_conflict_fn_max
Ndb_conflict_fn_max_del_win
Ndb_conflict_fn_max_del_win_ins
Ndb_conflict_fn_max_ins
Ndb_conflict_fn_old
Ndb_conflict_last_conflict_epoch
Ndb_conflict_last_stable_epoch
Ndb_conflict_reflected_op_discard_count
Ndb_conflict_reflected_op_prepare_count
Ndb_conflict_refresh_op_count
Ndb_conflict_trans_conflict_commit_count
Ndb_conflict_trans_detect_iter_count
Ndb_conflict_trans_reject_count
Ndb_conflict_trans_row_conflict_count
Ndb_conflict_trans_row_reject_count
Ndb_epoch_delete_delete_count
Ndb_execute_count
Ndb_fetch_table_stats
Ndb_last_commit_epoch_server
Ndb_last_commit_epoch_session
Ndb_metadata_detected_count
Ndb_metadata_excluded_count
Ndb_metadata_synced_count
Ndb_number_of_data_nodes
Ndb_pruned_scan_count
Ndb_pushed_queries_defined
Ndb_pushed_queries_dropped
Ndb_pushed_queries_executed
Ndb_pushed_reads
Ndb_scan_count
Ndb_slave_max_replicated_epoch
Ndb_trans_hint_count_session
Ongoing_anonymous_gtid_violating_transaction_count
Ongoing_automatic_gtid_violating_transaction_count
Rewriter_number_loaded_rules
Rewriter_number_reloads
Rewriter_number_rewritten_queries
Rewriter_reload_error
Rpl_semi_sync_master_clients
Rpl_semi_sync_master_net_avg_wait_time
Rpl_semi_sync_master_net_wait_time
Rpl_semi_sync_master_net_waits
Rpl_semi_sync_master_no_times
Rpl_semi_sync_master_no_tx
Rpl_semi_sync_master_status
Rpl_semi_sync_master_timefunc_failures
Rpl_semi_sync_master_tx_avg_wait_time
Rpl_semi_sync_master_tx_wait_time
Rpl_semi_sync_master_tx_waits
Rpl_semi_sync_master_wait_pos_backtraverse
Rpl_semi_sync_master_wait_sessions
Rpl_semi_sync_master_yes_tx
Rpl_semi_sync_replica_status
Rpl_semi_sync_slave_status
Rpl_semi_sync_source_clients
Rpl_semi_sync_source_net_avg_wait_time
Rpl_semi_sync_source_net_wait_time
Rpl_semi_sync_source_net_waits
Rpl_semi_sync_source_no_times
Rpl_semi_sync_source_no_tx
Rpl_semi_sync_source_status
Rpl_semi_sync_source_timefunc_failures
Rpl_semi_sync_source_tx_avg_wait_time
Rpl_semi_sync_source_tx_wait_time
Rpl_semi_sync_source_tx_waits
Rpl_semi_sync_source_wait_pos_backtraverse
Rpl_semi_sync_source_wait_sessions
Rpl_semi_sync_source_yes_tx
Slave_rows_last_search_algorithm_used
telemetry.live_sessions
validate_password_dictionary_file_last_parsed
validate_password_dictionary_file_words_count
validate_password.dictionary_file_last_parsed
validate_password.dictionary_file_words_count
NAMES
)

fail() {
    printf '%s\n' "mysql_baseline_show_status_optional_absence_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names \
                --default-character-set=utf8mb4 "$@"
        return
    fi

    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql \
            --protocol=TCP \
            -h127.0.0.1 \
            -uroot \
            --batch \
            --raw \
            --skip-column-names \
            --default-character-set=utf8mb4 \
            "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

status_names_sql() {
    printf '%s\n' "$ABSENT_STATUS_NAMES" \
        | awk 'BEGIN { first = 1 } NF {
            if (!first) {
                printf ", "
            }
            gsub(/\047/, "\047\047")
            printf "\047%s\047", $0
            first = 0
        }'
}

expect_absent_status_names() {
    label=$1
    sql=$2
    output=$(run_mysql "$sql")
    expect_value "$label" "" "$output"
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

names_sql=$(status_names_sql)

expect_absent_status_names \
    "default optional status variables absent" \
    "SHOW STATUS WHERE Variable_name IN ($names_sql);"
expect_absent_status_names \
    "session optional status variables absent" \
    "SHOW SESSION STATUS WHERE Variable_name IN ($names_sql);"
expect_absent_status_names \
    "local optional status variables absent" \
    "SHOW LOCAL STATUS WHERE Variable_name IN ($names_sql);"
expect_absent_status_names \
    "global optional status variables absent" \
    "SHOW GLOBAL STATUS WHERE Variable_name IN ($names_sql);"

printf '%s\n' "mysql_baseline_show_status_optional_absence_expectations: ok"
