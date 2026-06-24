# Server status variables

The exact value shape, counter lifetime, session/global visibility, optional plugin/build availability, and SHOW STATUS/performance_schema exposure must be verified per variable.

| Variable | Status | Notes |
| --- | --- | --- |
| `Aborted_clients` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Aborted_connects` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Acl_cache_items_count` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Audit_log_current_size` | ❌ | Counter value or embedded zero/empty |
| `Audit_log_direct_writes` | ❌ | Counter value or embedded zero/empty |
| `Audit_log_event_max_drop_size` | ❌ | Counter value or embedded zero/empty |
| `Audit_log_events` | ❌ | Counter value or embedded zero/empty |
| `Audit_log_events_filtered` | ❌ | Counter value or embedded zero/empty |
| `Audit_log_events_lost` | ❌ | Counter value or embedded zero/empty |
| `Audit_log_events_written` | ❌ | Counter value or embedded zero/empty |
| `Audit_log_total_size` | ❌ | Counter value or embedded zero/empty |
| `Audit_log_write_waits` | ❌ | Counter value or embedded zero/empty |
| `Authentication_ldap_sasl_supported_methods` | ❌ | Counter value or embedded zero/empty |
| `Binlog_cache_disk_use` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Binlog_cache_use` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Binlog_stmt_cache_disk_use` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Binlog_stmt_cache_use` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Bytes_received` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Bytes_sent` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Caching_sha2_password_rsa_public_key` | ❌ | Counter value or embedded zero/empty |
| `Com_admin_commands` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_alter_db` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_alter_event` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_alter_function` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_alter_instance` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_alter_procedure` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_alter_resource_group` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_alter_server` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_alter_table` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_alter_tablespace` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_alter_user` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_alter_user_default_role` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_analyze` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_assign_to_keycache` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_begin` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_binlog` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_call_procedure` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_change_db` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_change_repl_filter` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_change_replication_source` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_check` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_checksum` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_clone` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_commit` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_create_db` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_create_event` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_create_function` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_create_index` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_create_procedure` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_create_resource_group` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_create_role` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_create_server` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_create_table` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_create_trigger` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_create_udf` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_create_user` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_create_view` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_create_spatial_reference_system` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_dealloc_sql` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_delete` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_delete_multi` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_do` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_drop_db` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_drop_event` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_drop_function` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_drop_index` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_drop_procedure` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_drop_resource_group` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_drop_role` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_drop_server` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_drop_spatial_reference_system` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_drop_table` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_drop_trigger` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_drop_user` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_drop_view` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_empty_query` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_execute_sql` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_explain_other` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_flush` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_get_diagnostics` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_grant` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_grant_roles` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_group_replication_start` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_group_replication_stop` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_ha_close` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_ha_open` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_ha_read` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_help` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_import` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_insert` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_insert_select` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_install_component` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_install_plugin` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_kill` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_load` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_lock_instance` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_lock_tables` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_optimize` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_preload_keys` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_prepare_sql` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_purge` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_purge_before_date` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_release_savepoint` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_rename_table` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_rename_user` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_repair` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_replace` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_replace_select` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_replica_start` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_replica_stop` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_reset` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_resignal` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_restart` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_revoke` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_revoke_all` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_revoke_roles` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_rollback` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_rollback_to_savepoint` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_savepoint` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_select` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_set_option` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_set_password` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_set_resource_group` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_set_role` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_authors` | ❌ | Counter value or embedded zero/empty |
| `Com_show_binary_log_status` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_binlog_events` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_binlogs` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_charsets` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_collations` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_contributors` | ❌ | Counter value or embedded zero/empty |
| `Com_show_create_db` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_create_event` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_create_func` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_create_proc` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_create_table` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_create_trigger` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_create_user` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_databases` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_engine_logs` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_engine_mutex` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_engine_status` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_errors` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_events` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_fields` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_function_code` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_function_status` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_grants` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_keys` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_ndb_status` | ❌ | Counter value or embedded zero/empty |
| `Com_show_open_tables` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_parse_tree` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_plugins` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_privileges` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_procedure_code` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_procedure_status` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_processlist` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_profile` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_profiles` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_relaylog_events` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_replica_status` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_replicas` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_status` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_storage_engines` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_table_status` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_tables` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_triggers` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_variables` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_show_warnings` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_shutdown` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_signal` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_stmt_close` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_stmt_execute` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_stmt_fetch` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_stmt_prepare` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_stmt_reprepare` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_stmt_reset` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_stmt_send_long_data` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_truncate` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_uninstall_component` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_uninstall_plugin` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_unlock_instance` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_unlock_tables` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_update` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_update_multi` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_xa_commit` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_xa_end` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_xa_prepare` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_xa_recover` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_xa_rollback` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Com_xa_start` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Compression` | 🟡 | Limited `SHOW STATUS` session/`LOCAL` value `OFF`; omitted from `GLOBAL`; no compression protocol state |
| `Compression_algorithm` | ❌ | Counter value or embedded zero/empty |
| `Compression_level` | ❌ | Counter value or embedded zero/empty |
| `Connection_control_delay_generated` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live connection-control delay lifecycle |
| `Connection_control_exempted_unknown_users` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live connection-control exemption lifecycle |
| `Connection_errors_accept` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live connection error lifecycle |
| `Connection_errors_internal` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live connection error lifecycle |
| `Connection_errors_max_connections` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live connection error lifecycle |
| `Connection_errors_peer_address` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live connection error lifecycle |
| `Connection_errors_select` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live connection error lifecycle |
| `Connection_errors_tcpwrap` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live connection error lifecycle |
| `Connections` | 🟡 | Limited `SHOW STATUS` embedded value `1`; no live counter lifecycle |
| `Created_tmp_disk_tables` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Created_tmp_files` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Created_tmp_tables` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Current_tls_ca` | ❌ | Counter value or embedded zero/empty |
| `Current_tls_capath` | ❌ | Counter value or embedded zero/empty |
| `Current_tls_cert` | ❌ | Counter value or embedded zero/empty |
| `Current_tls_cipher` | ❌ | Counter value or embedded zero/empty |
| `Current_tls_ciphersuites` | ❌ | Counter value or embedded zero/empty |
| `Current_tls_crl` | ❌ | Counter value or embedded zero/empty |
| `Current_tls_crlpath` | ❌ | Counter value or embedded zero/empty |
| `Current_tls_key` | ❌ | Counter value or embedded zero/empty |
| `Current_tls_version` | ❌ | Counter value or embedded zero/empty |
| `Delayed_errors` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live delayed-insert lifecycle |
| `Delayed_insert_threads` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live delayed-insert lifecycle |
| `Delayed_writes` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live delayed-insert lifecycle |
| `Deprecated_use_i_s_processlist_count` | ❌ | Counter value or embedded zero/empty |
| `Deprecated_use_i_s_processlist_last_timestamp` | ❌ | Counter value or embedded zero/empty |
| `dragnet.Status` | ❌ | Counter value or embedded zero/empty |
| `Error_log_buffered_bytes` | ❌ | Counter value or embedded zero/empty |
| `Error_log_buffered_events` | ❌ | Counter value or embedded zero/empty |
| `Error_log_expired_events` | ❌ | Counter value or embedded zero/empty |
| `Error_log_latest_write` | ❌ | Counter value or embedded zero/empty |
| `Firewall_access_denied` | ❌ | Counter value or embedded zero/empty |
| `Firewall_access_granted` | ❌ | Counter value or embedded zero/empty |
| `Firewall_access_suspicious` | ❌ | Counter value or embedded zero/empty |
| `Firewall_cached_entries` | ❌ | Counter value or embedded zero/empty |
| `Flush_commands` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Global_connection_memory` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live memory accounting |
| `Gr_all_consensus_proposals_count` | ❌ | Counter value or embedded zero/empty |
| `Gr_all_consensus_time_sum` | ❌ | Counter value or embedded zero/empty |
| `Gr_certification_garbage_collector_count` | ❌ | Counter value or embedded zero/empty |
| `Gr_certification_garbage_collector_time_sum` | ❌ | Counter value or embedded zero/empty |
| `Gr_consensus_bytes_received_sum` | ❌ | Counter value or embedded zero/empty |
| `Gr_consensus_bytes_sent_sum` | ❌ | Counter value or embedded zero/empty |
| `Gr_control_messages_sent_bytes_sum` | ❌ | Counter value or embedded zero/empty |
| `Gr_control_messages_sent_count` | ❌ | Counter value or embedded zero/empty |
| `Gr_control_messages_sent_roundtrip_time_sum` | ❌ | Counter value or embedded zero/empty |
| `Gr_data_messages_sent_bytes_sum` | ❌ | Counter value or embedded zero/empty |
| `Gr_data_messages_sent_count` | ❌ | Counter value or embedded zero/empty |
| `Gr_data_messages_sent_roundtrip_time_sum` | ❌ | Counter value or embedded zero/empty |
| `Gr_empty_consensus_proposals_count` | ❌ | Counter value or embedded zero/empty |
| `Gr_extended_consensus_count` | ❌ | Counter value or embedded zero/empty |
| `Gr_flow_control_throttle_active_count` | ❌ | Counter value or embedded zero/empty |
| `Gr_flow_control_throttle_count` | ❌ | Counter value or embedded zero/empty |
| `Gr_flow_control_throttle_last_throttle_timestamp` | ❌ | Counter value or embedded zero/empty |
| `Gr_flow_control_throttle_time_sum` | ❌ | Counter value or embedded zero/empty |
| `Gr_last_consensus_end_timestamp` | ❌ | Counter value or embedded zero/empty |
| `Gr_total_messages_sent_count` | ❌ | Counter value or embedded zero/empty |
| `Gr_transactions_consistency_after_sync_count` | ❌ | Counter value or embedded zero/empty |
| `Gr_transactions_consistency_after_sync_time_sum` | ❌ | Counter value or embedded zero/empty |
| `Gr_transactions_consistency_after_termination_count` | ❌ | Counter value or embedded zero/empty |
| `Gr_transactions_consistency_after_termination_time_sum` | ❌ | Counter value or embedded zero/empty |
| `Gr_transactions_consistency_before_begin_count` | ❌ | Counter value or embedded zero/empty |
| `Gr_transactions_consistency_before_begin_time_sum` | ❌ | Counter value or embedded zero/empty |
| `Handler_commit` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Handler_delete` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Handler_discover` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Handler_external_lock` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Handler_mrr_init` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Handler_prepare` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Handler_read_first` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Handler_read_key` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Handler_read_last` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Handler_read_next` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Handler_read_prev` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Handler_read_rnd` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Handler_read_rnd_next` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Handler_rollback` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Handler_savepoint` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Handler_savepoint_rollback` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Handler_update` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Handler_write` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Innodb_buffer_pool_bytes_data` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_bytes_dirty` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_dump_status` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_load_status` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_pages_data` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_pages_dirty` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_pages_flushed` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_pages_free` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_pages_latched` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_pages_misc` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_pages_total` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_read_ahead` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_read_ahead_evicted` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_read_ahead_rnd` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_read_requests` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_reads` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_resize_status` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_resize_status_code` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_resize_status_progress` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_wait_free` | ❌ | Counter value or embedded zero/empty |
| `Innodb_buffer_pool_write_requests` | ❌ | Counter value or embedded zero/empty |
| `Innodb_data_fsyncs` | ❌ | Counter value or embedded zero/empty |
| `Innodb_data_pending_fsyncs` | ❌ | Counter value or embedded zero/empty |
| `Innodb_data_pending_reads` | ❌ | Counter value or embedded zero/empty |
| `Innodb_data_pending_writes` | ❌ | Counter value or embedded zero/empty |
| `Innodb_data_read` | ❌ | Counter value or embedded zero/empty |
| `Innodb_data_reads` | ❌ | Counter value or embedded zero/empty |
| `Innodb_data_writes` | ❌ | Counter value or embedded zero/empty |
| `Innodb_data_written` | ❌ | Counter value or embedded zero/empty |
| `Innodb_dblwr_pages_written` | ❌ | Counter value or embedded zero/empty |
| `Innodb_dblwr_writes` | ❌ | Counter value or embedded zero/empty |
| `Innodb_have_atomic_builtins` | ❌ | Counter value or embedded zero/empty |
| `Innodb_log_waits` | ❌ | Counter value or embedded zero/empty |
| `Innodb_log_write_requests` | ❌ | Counter value or embedded zero/empty |
| `Innodb_log_writes` | ❌ | Counter value or embedded zero/empty |
| `Innodb_num_open_files` | ❌ | Counter value or embedded zero/empty |
| `Innodb_os_log_fsyncs` | ❌ | Counter value or embedded zero/empty |
| `Innodb_os_log_pending_fsyncs` | ❌ | Counter value or embedded zero/empty |
| `Innodb_os_log_pending_writes` | ❌ | Counter value or embedded zero/empty |
| `Innodb_os_log_written` | ❌ | Counter value or embedded zero/empty |
| `Innodb_page_size` | ❌ | Counter value or embedded zero/empty |
| `Innodb_pages_created` | ❌ | Counter value or embedded zero/empty |
| `Innodb_pages_read` | ❌ | Counter value or embedded zero/empty |
| `Innodb_pages_written` | ❌ | Counter value or embedded zero/empty |
| `Innodb_redo_log_capacity_resized` | ❌ | Counter value or embedded zero/empty |
| `Innodb_redo_log_checkpoint_lsn` | ❌ | Counter value or embedded zero/empty |
| `Innodb_redo_log_current_lsn` | ❌ | Counter value or embedded zero/empty |
| `Innodb_redo_log_enabled` | ❌ | Counter value or embedded zero/empty |
| `Innodb_redo_log_flushed_to_disk_lsn` | ❌ | Counter value or embedded zero/empty |
| `Innodb_redo_log_logical_size` | ❌ | Counter value or embedded zero/empty |
| `Innodb_redo_log_physical_size` | ❌ | Counter value or embedded zero/empty |
| `Innodb_redo_log_read_only` | ❌ | Counter value or embedded zero/empty |
| `Innodb_redo_log_resize_status` | ❌ | Counter value or embedded zero/empty |
| `Innodb_redo_log_uuid` | ❌ | Counter value or embedded zero/empty |
| `Innodb_row_lock_current_waits` | ❌ | Counter value or embedded zero/empty |
| `Innodb_row_lock_time` | ❌ | Counter value or embedded zero/empty |
| `Innodb_row_lock_time_avg` | ❌ | Counter value or embedded zero/empty |
| `Innodb_row_lock_time_max` | ❌ | Counter value or embedded zero/empty |
| `Innodb_row_lock_waits` | ❌ | Counter value or embedded zero/empty |
| `Innodb_rows_deleted` | ❌ | Counter value or embedded zero/empty |
| `Innodb_rows_inserted` | ❌ | Counter value or embedded zero/empty |
| `Innodb_rows_read` | ❌ | Counter value or embedded zero/empty |
| `Innodb_rows_updated` | ❌ | Counter value or embedded zero/empty |
| `Innodb_system_rows_deleted` | ❌ | Counter value or embedded zero/empty |
| `Innodb_system_rows_inserted` | ❌ | Counter value or embedded zero/empty |
| `Innodb_system_rows_read` | ❌ | Counter value or embedded zero/empty |
| `Innodb_system_rows_updated` | ❌ | Counter value or embedded zero/empty |
| `Innodb_truncated_status_writes` | ❌ | Counter value or embedded zero/empty |
| `Innodb_undo_tablespaces_active` | ❌ | Counter value or embedded zero/empty |
| `Innodb_undo_tablespaces_explicit` | ❌ | Counter value or embedded zero/empty |
| `Innodb_undo_tablespaces_implicit` | ❌ | Counter value or embedded zero/empty |
| `Innodb_undo_tablespaces_total` | ❌ | Counter value or embedded zero/empty |
| `Key_blocks_not_flushed` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live key-cache lifecycle |
| `Key_blocks_unused` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live key-cache lifecycle |
| `Key_blocks_used` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live key-cache lifecycle |
| `Key_read_requests` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live key-cache lifecycle |
| `Key_reads` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live key-cache lifecycle |
| `Key_write_requests` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live key-cache lifecycle |
| `Key_writes` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live key-cache lifecycle |
| `Last_query_cost` | 🟡 | Limited `SHOW STATUS` embedded value `0.000000`; no live optimizer-cost tracking |
| `Last_query_partial_plans` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live optimizer-cost tracking |
| `Locked_connects` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live locked-connection accounting |
| `Max_execution_time_exceeded` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live execution-time counter lifecycle |
| `Max_execution_time_set` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live execution-time counter lifecycle |
| `Max_execution_time_set_failed` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live execution-time counter lifecycle |
| `Max_used_connections` | 🟡 | Limited `SHOW STATUS` embedded value `1`; no live connection high-water accounting |
| `Max_used_connections_time` | 🟡 | Limited `SHOW STATUS` embedded value `1970-01-01 00:00:00`; no live connection high-water accounting |
| `mecab_charset` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_aborted_clients` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_address` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_bytes_received` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_bytes_received_compressed_payload` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_bytes_received_uncompressed_frame` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_bytes_sent` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_bytes_sent_compressed_payload` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_bytes_sent_uncompressed_frame` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_compression_algorithm` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_compression_level` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_connection_accept_errors` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_connection_errors` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_connections_accepted` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_connections_closed` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_connections_rejected` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_crud_create_view` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_crud_delete` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_crud_drop_view` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_crud_find` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_crud_insert` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_crud_modify_view` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_crud_update` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_cursor_close` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_cursor_fetch` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_cursor_open` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_errors_sent` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_errors_unknown_message_type` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_expect_close` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_expect_open` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_init_error` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_messages_sent` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_notice_global_sent` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_notice_other_sent` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_notice_warning_sent` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_notified_by_group_replication` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_port` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_prep_deallocate` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_prep_execute` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_prep_prepare` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_rows_sent` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_sessions` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_sessions_accepted` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_sessions_closed` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_sessions_fatal_error` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_sessions_killed` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_sessions_rejected` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_socket` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_ssl_accept_renegotiates` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_ssl_accepts` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_ssl_active` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_ssl_cipher` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_ssl_cipher_list` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_ssl_ctx_verify_depth` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_ssl_ctx_verify_mode` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_ssl_finished_accepts` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_ssl_server_not_after` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_ssl_server_not_before` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_ssl_verify_depth` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_ssl_verify_mode` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_ssl_version` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_stmt_create_collection` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_stmt_create_collection_index` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_stmt_disable_notices` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_stmt_drop_collection` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_stmt_drop_collection_index` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_stmt_enable_notices` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_stmt_ensure_collection` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_stmt_execute_mysqlx` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_stmt_execute_sql` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_stmt_execute_xplugin` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_stmt_get_collection_options` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_stmt_kill_client` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_stmt_list_clients` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_stmt_list_notices` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_stmt_list_objects` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_stmt_modify_collection_options` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_stmt_ping` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_worker_threads` | ❌ | Counter value or embedded zero/empty |
| `Mysqlx_worker_threads_active` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_adaptive_send_deferred_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_adaptive_send_deferred_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_adaptive_send_deferred_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_adaptive_send_deferred_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_adaptive_send_forced_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_adaptive_send_forced_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_adaptive_send_forced_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_adaptive_send_forced_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_adaptive_send_unforced_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_adaptive_send_unforced_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_adaptive_send_unforced_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_adaptive_send_unforced_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_bytes_received_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_bytes_received_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_bytes_received_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_bytes_received_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_bytes_sent_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_bytes_sent_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_bytes_sent_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_bytes_sent_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_event_bytes_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_event_bytes_count_injector` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_event_data_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_event_data_count_injector` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_event_nondata_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_event_nondata_count_injector` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_pk_op_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_pk_op_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_pk_op_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_pk_op_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_pruned_scan_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_pruned_scan_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_pruned_scan_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_pruned_scan_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_range_scan_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_range_scan_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_range_scan_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_range_scan_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_read_row_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_read_row_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_read_row_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_read_row_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_scan_batch_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_scan_batch_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_scan_batch_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_scan_batch_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_table_scan_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_table_scan_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_table_scan_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_table_scan_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_abort_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_abort_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_abort_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_abort_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_close_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_close_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_close_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_close_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_commit_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_commit_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_commit_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_commit_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_local_read_row_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_local_read_row_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_local_read_row_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_local_read_row_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_start_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_start_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_start_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_trans_start_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_uk_op_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_uk_op_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_uk_op_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_uk_op_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_wait_exec_complete_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_wait_exec_complete_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_wait_exec_complete_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_wait_exec_complete_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_wait_meta_request_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_wait_meta_request_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_wait_meta_request_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_wait_meta_request_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_wait_nanos_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_wait_nanos_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_wait_nanos_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_wait_nanos_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_wait_scan_result_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_wait_scan_result_count_replica` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_wait_scan_result_count_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_api_wait_scan_result_count_slave` | ❌ | Counter value or embedded zero/empty |
| `Ndb_cluster_node_id` | ❌ | Counter value or embedded zero/empty |
| `Ndb_config_from_host` | ❌ | Counter value or embedded zero/empty |
| `Ndb_config_from_port` | ❌ | Counter value or embedded zero/empty |
| `Ndb_config_generation` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_fn_epoch` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_fn_epoch_trans` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_fn_epoch2` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_fn_epoch2_trans` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_fn_max` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_fn_max_del_win` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_fn_max_del_win_ins` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_fn_max_ins` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_fn_old` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_last_conflict_epoch` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_last_stable_epoch` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_reflected_op_discard_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_reflected_op_prepare_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_refresh_op_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_trans_conflict_commit_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_trans_detect_iter_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_trans_reject_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_trans_row_conflict_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_conflict_trans_row_reject_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_epoch_delete_delete_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_execute_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_fetch_table_stats` | ❌ | Counter value or embedded zero/empty |
| `Ndb_last_commit_epoch_server` | ❌ | Counter value or embedded zero/empty |
| `Ndb_last_commit_epoch_session` | ❌ | Counter value or embedded zero/empty |
| `Ndb_metadata_detected_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_metadata_excluded_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_metadata_synced_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_number_of_data_nodes` | ❌ | Counter value or embedded zero/empty |
| `Ndb_pruned_scan_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_pushed_queries_defined` | ❌ | Counter value or embedded zero/empty |
| `Ndb_pushed_queries_dropped` | ❌ | Counter value or embedded zero/empty |
| `Ndb_pushed_queries_executed` | ❌ | Counter value or embedded zero/empty |
| `Ndb_pushed_reads` | ❌ | Counter value or embedded zero/empty |
| `Ndb_scan_count` | ❌ | Counter value or embedded zero/empty |
| `Ndb_slave_max_replicated_epoch` | ❌ | Counter value or embedded zero/empty |
| `Ndb_trans_hint_count_session` | ❌ | Counter value or embedded zero/empty |
| `Not_flushed_delayed_rows` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live delayed-insert lifecycle |
| `Ongoing_anonymous_gtid_violating_transaction_count` | ❌ | Counter value or embedded zero/empty |
| `Ongoing_anonymous_transaction_count` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live replication transaction lifecycle |
| `Ongoing_automatic_gtid_violating_transaction_count` | ❌ | Counter value or embedded zero/empty |
| `Open_files` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Open_streams` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Open_table_definitions` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Open_tables` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Opened_files` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Opened_table_definitions` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Opened_tables` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Performance_schema_accounts_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_cond_classes_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_cond_instances_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_digest_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_file_classes_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_file_handles_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_file_instances_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_hosts_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_index_stat_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_locker_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_memory_classes_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_metadata_lock_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_meter_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_metric_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_mutex_classes_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_mutex_instances_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_nested_statement_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_prepared_statements_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_program_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_rwlock_classes_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_rwlock_instances_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_session_connect_attrs_longest_seen` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_session_connect_attrs_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_socket_classes_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_socket_instances_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_stage_classes_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_statement_classes_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_table_handles_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_table_instances_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_table_lock_stat_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_thread_classes_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_thread_instances_lost` | ❌ | Counter value or embedded zero/empty |
| `Performance_schema_users_lost` | ❌ | Counter value or embedded zero/empty |
| `Prepared_stmt_count` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Queries` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Questions` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Replica_open_temp_tables` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live replication temp-table lifecycle |
| `Resource_group_supported` | 🟡 | Limited `SHOW STATUS` embedded value `OFF`; no resource group subsystem |
| `Rewriter_number_loaded_rules` | ❌ | Counter value or embedded zero/empty |
| `Rewriter_number_reloads` | ❌ | Counter value or embedded zero/empty |
| `Rewriter_number_rewritten_queries` | ❌ | Counter value or embedded zero/empty |
| `Rewriter_reload_error` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_master_clients` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_master_net_avg_wait_time` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_master_net_wait_time` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_master_net_waits` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_master_no_times` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_master_no_tx` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_master_status` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_master_timefunc_failures` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_master_tx_avg_wait_time` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_master_tx_wait_time` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_master_tx_waits` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_master_wait_pos_backtraverse` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_master_wait_sessions` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_master_yes_tx` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_replica_status` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_slave_status` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_source_clients` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_source_net_avg_wait_time` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_source_net_wait_time` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_source_net_waits` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_source_no_times` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_source_no_tx` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_source_status` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_source_timefunc_failures` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_source_tx_avg_wait_time` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_source_tx_wait_time` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_source_tx_waits` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_source_wait_pos_backtraverse` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_source_wait_sessions` | ❌ | Counter value or embedded zero/empty |
| `Rpl_semi_sync_source_yes_tx` | ❌ | Counter value or embedded zero/empty |
| `Rsa_public_key` | ❌ | Counter value or embedded zero/empty |
| `Secondary_engine_execution_count` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no secondary-engine execution lifecycle |
| `Select_full_join` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Select_full_range_join` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Select_range` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Select_range_check` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Select_scan` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Slave_open_temp_tables` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live replication temp-table lifecycle |
| `Slave_rows_last_search_algorithm_used` | ❌ | Counter value or embedded zero/empty |
| `Slow_launch_threads` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Slow_queries` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Sort_merge_passes` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Sort_range` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Sort_rows` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Sort_scan` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Ssl_accept_renegotiates` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_accepts` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_callback_cache_hits` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_cipher` | 🟡 | Limited `SHOW STATUS` embedded empty string; no TLS state |
| `Ssl_cipher_list` | 🟡 | Limited `SHOW STATUS` embedded empty string; no TLS state |
| `Ssl_client_connects` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_connect_renegotiates` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_ctx_verify_depth` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_ctx_verify_mode` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_default_timeout` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_finished_accepts` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_finished_connects` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_server_not_after` | 🟡 | Limited `SHOW STATUS` embedded empty string; no TLS state |
| `Ssl_server_not_before` | 🟡 | Limited `SHOW STATUS` embedded empty string; no TLS state |
| `Ssl_session_cache_hits` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_session_cache_misses` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_session_cache_mode` | 🟡 | Limited `SHOW STATUS` embedded empty string; no TLS state |
| `Ssl_session_cache_overflows` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_session_cache_size` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_session_cache_timeout` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_session_cache_timeouts` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_sessions_reused` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_used_session_cache_entries` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_verify_depth` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_verify_mode` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no TLS state |
| `Ssl_version` | 🟡 | Limited `SHOW STATUS` embedded empty string; no TLS state |
| `Table_locks_immediate` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Table_locks_waited` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Table_open_cache_hits` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Table_open_cache_misses` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Table_open_cache_overflows` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Tc_log_max_pages_used` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Tc_log_page_size` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Tc_log_page_waits` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live counter lifecycle |
| `Telemetry_logs_supported` | 🟡 | Limited `SHOW STATUS` embedded value `OFF`; no telemetry subsystem |
| `Telemetry_metrics_supported` | 🟡 | Limited `SHOW STATUS` embedded value `OFF`; no telemetry subsystem |
| `Telemetry_traces_supported` | 🟡 | Limited `SHOW STATUS` embedded value `OFF`; no telemetry subsystem |
| `telemetry.live_sessions` | ❌ | Counter value or embedded zero/empty |
| `Threads_cached` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no server thread cache |
| `Threads_connected` | 🟡 | Limited `SHOW STATUS` embedded value `1`; current embedded handle only |
| `Threads_created` | 🟡 | Limited `SHOW STATUS` embedded value `1`; current embedded handle only |
| `Threads_running` | 🟡 | Limited `SHOW STATUS` embedded value `1`; current embedded handle only |
| `Tls_library_version` | ❌ | Counter value or embedded zero/empty |
| `Tls_sni_server_name` | ❌ | Counter value or embedded zero/empty |
| `Uptime` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no live uptime clock |
| `Uptime_since_flush_status` | 🟡 | Limited `SHOW STATUS` embedded value `0`; no `FLUSH STATUS` lifecycle |
| `validate_password_dictionary_file_last_parsed` | ❌ | Counter value or embedded zero/empty |
| `validate_password_dictionary_file_words_count` | ❌ | Counter value or embedded zero/empty |
| `validate_password.dictionary_file_last_parsed` | ❌ | Counter value or embedded zero/empty |
| `validate_password.dictionary_file_words_count` | ❌ | Counter value or embedded zero/empty |

[Back to compatibility overview](../../COMPATIBILITY.md)
