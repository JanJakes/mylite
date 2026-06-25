# Server system variables

The exact value, scope, mutability, privilege requirement, persisted-variable behavior, optional plugin/build availability, and `SHOW VARIABLES` / Performance Schema exposure must be verified per variable.

MyLite currently exposes a limited `SHOW VARIABLES` row set, including optional
`GLOBAL`, `SESSION`, `LOCAL`, `LIKE 'pattern'`, and limited `WHERE` forms, only
for the fixed variables in the runtime registry defined by
[baseline SHOW VARIABLES](../specs/baseline-show-variables/specs.md) and
[baseline SHOW VARIABLES WHERE](../specs/baseline-show-variables-where/specs.md).
Variables outside that registry remain absent from `SHOW VARIABLES`, and
Performance Schema variable tables remain unsupported.

Variables marked as target-runtime optional absence are MySQL 8.4.9
target-build-absent names verified by
[baseline SHOW VARIABLES optional absence](../specs/baseline-show-variables-optional-absence/specs.md):
`SHOW VARIABLES` returns no rows for default, session, local, and global
scopes, and representative scalar reads return `1193 / HY000`.

Persisted-variable server management forms such as `SET PERSIST`,
`SET PERSIST_ONLY`, `SET @@PERSIST.name = value`, `SET @@PERSIST_ONLY.name =
value`, and `RESET PERSIST` are accepted as embedded admin no-ops with warning
`1105`; MyLite does not maintain a persisted system-variable file or shared
server-global mutable state.

Unsupported broad server-global, session, and local `SET` variants that are not
handled by the per-variable runtime are accepted as embedded utility no-ops
with warning `1105`; MyLite does not mutate absent variables or shared server
state through this fallback path.

| Variable | Status | Notes |
| --- | --- | --- |
| `activate_all_roles_on_login` | ❌ | Value, scope, SET, diagnostics |
| `admin_address` | ✅ | Fixed global `NULL`/blank readback/SHOW and read-only SET diagnostics |
| `admin_port` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `admin_ssl_ca` | ✅ | Fixed global `NULL`/blank readback/SHOW and `DEFAULT` global no-op SET |
| `admin_ssl_capath` | ✅ | Fixed global `NULL`/blank readback/SHOW and `DEFAULT` global no-op SET |
| `admin_ssl_cert` | ✅ | Fixed global `NULL`/blank readback/SHOW and `DEFAULT` global no-op SET |
| `admin_ssl_cipher` | ✅ | Fixed global `NULL`/blank readback/SHOW and `DEFAULT` global no-op SET |
| `admin_ssl_crl` | ✅ | Fixed global `NULL`/blank readback/SHOW and `DEFAULT` global no-op SET |
| `admin_ssl_crlpath` | ✅ | Fixed global `NULL`/blank readback/SHOW and `DEFAULT` global no-op SET |
| `admin_ssl_key` | ✅ | Fixed global `NULL`/blank readback/SHOW and `DEFAULT` global no-op SET |
| `admin_tls_ciphersuites` | ✅ | Fixed global `NULL`/blank readback/SHOW and `DEFAULT` global no-op SET |
| `admin_tls_version` | ✅ | Fixed global readback/SHOW and exact global no-op SET |
| `audit_log_buffer_size` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_compression` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_connection_policy` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_current_session` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_database` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_disable` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_encryption` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_exclude_accounts` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_file` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_filter_id` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_flush` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_flush_interval_seconds` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_format` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_format_unix_timestamp` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_include_accounts` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_password_history_keep_days` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_policy` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_prune_seconds` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_read_buffer_size` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_rotate_on_size` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_statement_policy` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `audit_log_strategy` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_kerberos_service_key_tab` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_kerberos_service_principal` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_sasl_auth_method_name` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_sasl_bind_base_dn` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_sasl_bind_root_dn` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_sasl_bind_root_pwd` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_sasl_ca_path` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_sasl_connect_timeout` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_sasl_group_search_attr` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_sasl_group_search_filter` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_sasl_init_pool_size` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_sasl_log_status` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_sasl_max_pool_size` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_sasl_referral` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_sasl_response_timeout` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_sasl_server_host` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_sasl_server_port` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_sasl_tls` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_sasl_user_search_attr` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_simple_auth_method_name` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_simple_bind_base_dn` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_simple_bind_root_dn` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_simple_bind_root_pwd` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_simple_ca_path` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_simple_connect_timeout` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_simple_group_search_attr` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_simple_group_search_filter` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_simple_init_pool_size` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_simple_log_status` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_simple_max_pool_size` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_simple_referral` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_simple_response_timeout` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_simple_server_host` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_simple_server_port` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_simple_tls` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_ldap_simple_user_search_attr` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_policy` | ✅ | Fixed global readback/SHOW and exact global no-op SET |
| `authentication_webauthn_rp_id` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_windows_log_level` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `authentication_windows_use_principal_name` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `auto_generate_certs` | ❌ | Value, scope, SET, diagnostics |
| `auto_increment_increment` | ✅ | Session/local/unscoped reads, fixed global reads, `SHOW VARIABLES`, session `SET`, clamp diagnostics, and descriptor-owned generated `AUTO_INCREMENT` allocation are MySQL-runtime verified for values `1..65535` when `auto_increment_offset <= auto_increment_increment`; no mutable global state, persisted/startup values, `SET_VAR`, replication behavior, concurrency lock-mode, or offset-greater-than-increment allocation |
| `auto_increment_offset` | ✅ | Session/local/unscoped reads, fixed global reads, `SHOW VARIABLES`, session `SET`, clamp diagnostics, and descriptor-owned generated `AUTO_INCREMENT` allocation are MySQL-runtime verified for values `1..65535` when `auto_increment_offset <= auto_increment_increment`; no mutable global state, persisted/startup values, `SET_VAR`, replication behavior, concurrency lock-mode, or offset-greater-than-increment allocation |
| `autocommit` | 🟡 | Session/local/unscoped scalar reads, `SHOW VARIABLES`, and `SET autocommit` are supported for the documented baseline, including current DML commit/rollback effects, `SET autocommit=1` commit, `START TRANSACTION` interaction, and savepoints while disabled; global readback is fixed `ON`, and mutable global/persisted state, protocol status flags, MVCC snapshot parity, lock semantics, privileges, and Performance Schema variable tables remain unsupported |
| `automatic_sp_privileges` | ❌ | Value, scope, SET, diagnostics |
| `back_log` | ❌ | Value, scope, SET, diagnostics |
| `basedir` | 🟡 | Limited fixed global read-only scalar and `SHOW VARIABLES` placeholder `/usr/`; default/global scalar reads and `SHOW VARIABLES` rows are supported, session/local scalar reads return MySQL-style global-variable diagnostics, and assignment returns MySQL-style read-only diagnostics. No host installation discovery, startup option handling, path probing, or filesystem behavior |
| `big_tables` | 🟡 | Limited scalar `SELECT @@big_tables` with no scope, `session`, `local`, or fixed `global`; limited `SHOW VARIABLES` rows; and handle-local session `SET` assignment for no scope, `SESSION`, `LOCAL`, direct `@@variable`, `@@session`, and `@@local` forms using `DEFAULT`, boolean tokens, integer `0`/`1` with supported unary signs, string `ON`/`OFF`, and supported integer/string user variables, with decimal user variables rejected using MySQL-compatible diagnostics. Session/local/unscoped reads report the handle-local value; global reads remain fixed at `0`, and mutable global assignment is limited to exact no-op forms. No actual temporary-table storage, optimizer planning, row materialization, startup/persisted values, `SET_VAR` hints, privileges, or Performance Schema variable tables |
| `bind_address` | ❌ | Value, scope, SET, diagnostics |
| `binlog_cache_size` | ❌ | Value, scope, SET, diagnostics |
| `binlog_checksum` | ❌ | Value, scope, SET, diagnostics |
| `binlog_direct_non_transactional_updates` | ❌ | Value, scope, SET, diagnostics |
| `binlog_encryption` | ❌ | Value, scope, SET, diagnostics |
| `binlog_error_action` | ❌ | Value, scope, SET, diagnostics |
| `binlog_expire_logs_auto_purge` | ❌ | Value, scope, SET, diagnostics |
| `binlog_expire_logs_seconds` | ❌ | Value, scope, SET, diagnostics |
| `binlog_format` | ❌ | Value, scope, SET, diagnostics |
| `binlog_group_commit_sync_delay` | ❌ | Value, scope, SET, diagnostics |
| `binlog_group_commit_sync_no_delay_count` | ❌ | Value, scope, SET, diagnostics |
| `binlog_gtid_simple_recovery` | ❌ | Value, scope, SET, diagnostics |
| `binlog_max_flush_queue_time` | ❌ | Value, scope, SET, diagnostics |
| `binlog_order_commits` | ❌ | Value, scope, SET, diagnostics |
| `binlog_rotate_encryption_master_key_at_startup` | ❌ | Value, scope, SET, diagnostics |
| `binlog_row_event_max_size` | ❌ | Value, scope, SET, diagnostics |
| `binlog_row_image` | ❌ | Value, scope, SET, diagnostics |
| `binlog_row_metadata` | ❌ | Value, scope, SET, diagnostics |
| `binlog_row_value_options` | ❌ | Value, scope, SET, diagnostics |
| `binlog_rows_query_log_events` | ❌ | Value, scope, SET, diagnostics |
| `binlog_stmt_cache_size` | ❌ | Value, scope, SET, diagnostics |
| `binlog_transaction_compression` | ❌ | Value, scope, SET, diagnostics |
| `binlog_transaction_compression_level_zstd` | ❌ | Value, scope, SET, diagnostics |
| `binlog_transaction_dependency_history_size` | ❌ | Value, scope, SET, diagnostics |
| `block_encryption_mode` | ❌ | Value, scope, SET, diagnostics |
| `build_id` | ❌ | Value, scope, SET, diagnostics |
| `bulk_insert_buffer_size` | ❌ | Value, scope, SET, diagnostics |
| `caching_sha2_password_auto_generate_rsa_keys` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `caching_sha2_password_digest_rounds` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `caching_sha2_password_private_key_path` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `caching_sha2_password_public_key_path` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `character_set_client` | 🟡 | Limited scalar `SELECT @@character_set_client` with no scope, `session`, `local`, or `global`; returns the current session connection charset readback after admitted `SET NAMES` / `SET CHARACTER SET` forms for `utf8mb4`, `utf8mb3` aliases, focused WordPress legacy charsets, and `binary`, including forms with atomic tail assignments; no mutable conversion, client negotiation, or protocol charset metadata |
| `character_set_connection` | 🟡 | Limited scalar `SELECT @@character_set_connection` with no scope, `session`, `local`, or `global`; returns the current session connection charset readback after admitted `SET NAMES` / `SET CHARACTER SET` forms for `utf8mb4`, `utf8mb3` aliases, focused WordPress legacy charsets, and `binary`, including forms with atomic tail assignments; no mutable conversion, string literal semantics, or protocol charset metadata |
| `character_set_database` | 🟡 | Limited read-only scalar `SELECT @@character_set_database` with no scope, `session`, `local`, or `global`; session/local reads return the selected MyLite schema descriptor default, fixed server fallback default, or synthetic `information_schema` value, while global reads return the fixed server default; no `SET`, conversion, or protocol charset metadata |
| `character_set_filesystem` | 🟡 | Limited read-only scalar `SELECT @@character_set_filesystem` with no scope, `session`, `local`, or `global`; returns MyLite's fixed file-name charset placeholder `binary`; no `SET`, mutable state, file-name conversion, server-side file operations, or protocol charset metadata |
| `character_set_results` | 🟡 | Limited scalar `SELECT @@character_set_results` with no scope, `session`, `local`, or `global`; returns the current session result charset readback after admitted `SET NAMES` / `SET CHARACTER SET` forms for `utf8mb4`, `utf8mb3` aliases, focused WordPress legacy charsets, and `binary`, including forms with atomic tail assignments; no mutable result conversion, `NULL` results mode, or protocol charset metadata |
| `character_set_server` | 🟡 | Limited read-only scalar `SELECT @@character_set_server` with no scope, `session`, `local`, or `global`; returns MyLite's fixed `utf8mb4` embedded server default; no `SET`, startup options, mutable global state, database defaults, or protocol charset metadata |
| `character_set_system` | 🟡 | Limited read-only scalar `SELECT @@character_set_system` with no scope or `global`; returns MyLite's fixed identifier-system charset placeholder `utf8mb3`; no `session`/`local`, `SET`, mutable state, identifier conversion, string conversion, or protocol charset metadata |
| `character_sets_dir` | ❌ | Value, scope, SET, diagnostics |
| `check_proxy_users` | ❌ | Value, scope, SET, diagnostics |
| `clone_autotune_concurrency` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `clone_block_ddl` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `clone_buffer_size` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `clone_ddl_timeout` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `clone_delay_after_data_drop` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `clone_donor_timeout_after_network_failure` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `clone_enable_compression` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `clone_max_concurrency` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `clone_max_data_bandwidth` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `clone_max_network_bandwidth` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `clone_ssl_ca` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `clone_ssl_cert` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `clone_ssl_key` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `clone_valid_donor_list` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `collation_connection` | 🟡 | Limited scalar `SELECT @@collation_connection` with no scope, `session`, `local`, or `global`; returns the current session connection collation readback, supports admitted `SET NAMES ... COLLATE ...` collations for `utf8mb4`, `utf8mb3` aliases, focused WordPress legacy defaults, and `binary`, supports atomic tail assignments after connection charset statements, and resets to the charset default for `SET CHARACTER SET`; no mutable collation coercibility, string comparison semantics, or protocol collation metadata |
| `collation_database` | 🟡 | Limited read-only scalar `SELECT @@collation_database` with no scope, `session`, `local`, or `global`; session/local reads return the selected MyLite schema descriptor default, fixed server fallback default, or synthetic `information_schema` value, while global reads return the fixed server default; no `SET`, string comparison semantics, conversion, or protocol collation metadata |
| `collation_server` | 🟡 | Limited read-only scalar `SELECT @@collation_server` with no scope, `session`, `local`, or `global`; returns MyLite's fixed `utf8mb4_0900_ai_ci` embedded server default; no `SET`, startup options, mutable global state, database defaults, string comparison semantics, or protocol collation metadata |
| `completion_type` | ❌ | Value, scope, SET, diagnostics |
| `component_masking.dictionaries_flush_interval_seconds` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `component_masking.masking_database` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `component_scheduler.enabled` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `concurrent_insert` | ❌ | Value, scope, SET, diagnostics |
| `connect_timeout` | ❌ | Value, scope, SET, diagnostics |
| `connection_control_failed_connections_threshold` | ❌ | Value, scope, SET, diagnostics |
| `connection_control_max_connection_delay` | ❌ | Value, scope, SET, diagnostics |
| `connection_control_min_connection_delay` | ❌ | Value, scope, SET, diagnostics |
| `connection_memory_chunk_size` | ❌ | Value, scope, SET, diagnostics |
| `connection_memory_limit` | ❌ | Value, scope, SET, diagnostics |
| `core_file` | ❌ | Value, scope, SET, diagnostics |
| `create_admin_listener_thread` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `cte_max_recursion_depth` | ❌ | Value, scope, SET, diagnostics |
| `datadir` | 🟡 | Limited fixed global read-only scalar and `SHOW VARIABLES` placeholder `/var/lib/mysql/`; default/global scalar reads and `SHOW VARIABLES` rows are supported, session/local scalar reads return MySQL-style global-variable diagnostics, and assignment returns MySQL-style read-only diagnostics. No MySQL data-directory layout, startup option handling, path probing, or file placement behavior |
| `debug` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `debug_sync` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `default_collation_for_utf8mb4` | ✅ | Fixed default/session/global scalar and `SHOW VARIABLES` value `utf8mb4_0900_ai_ci`; session placeholder `SET` readback and fixed global no-op assignments are supported. No mutable collation policy, startup options, persisted state, or Performance Schema variable tables |
| `default_password_lifetime` | ✅ | Fixed global readback/SHOW and exact global no-op SET |
| `default_storage_engine` | 🟡 | Limited read-only scalar `SELECT @@default_storage_engine` with no scope, `session`, `local`, or `global`, plus `SHOW VARIABLES` rows; returns MyLite's fixed embedded permanent-table default `InnoDB`; current unavailable-engine substitution is controlled only by `@@sql_mode` during explicit `CREATE TABLE` / `CREATE TEMPORARY TABLE` `ENGINE` validation. No `SET`, mutable global/session state, alternate engines, plugins |
| `default_table_encryption` | ❌ | Value, scope, SET, diagnostics |
| `default_tmp_storage_engine` | 🟡 | Limited read-only scalar `SELECT @@default_tmp_storage_engine` with no scope, `session`, `local`, or `global`, plus `SHOW VARIABLES` rows; returns MyLite's fixed embedded temporary-table default `InnoDB`, and implicit temporary table creation continues to render `ENGINE=InnoDB`. No `SET`, mutable global/session state, MEMORY/MyISAM temporary engines, default-engine routing, startup/persisted values, or plugins |
| `default_week_format` | ❌ | Value, scope, SET, diagnostics; the limited one-argument `WEEK()` function currently uses MySQL mode 0 rather than a mutable session value |
| `delay_key_write` | ❌ | Value, scope, SET, diagnostics |
| `delayed_insert_limit` | ❌ | Value, scope, SET, diagnostics |
| `delayed_insert_timeout` | ❌ | Value, scope, SET, diagnostics |
| `delayed_queue_size` | ❌ | Value, scope, SET, diagnostics |
| `disabled_storage_engines` | ❌ | Value, scope, SET, diagnostics |
| `disconnect_on_expired_password` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `div_precision_increment` | ❌ | Value, scope, SET, diagnostics |
| `dragnet.log_error_filter_rules` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `end_markers_in_json` | ✅ | Fixed default/session/global scalar `0` and `SHOW VARIABLES` `OFF`; session placeholder `SET` readback and fixed global no-op assignments are supported. No JSON output end-marker behavior, startup options, persisted state, or Performance Schema variable tables |
| `enforce_gtid_consistency` | ❌ | Value, scope, SET, diagnostics |
| `enterprise_encryption.maximum_rsa_key_size` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `enterprise_encryption.rsa_support_legacy_padding` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `eq_range_index_dive_limit` | ❌ | Value, scope, SET, diagnostics |
| `error_count` | ✅ | Read-only session/local/unscoped scalar reads and simple expression reads over the previous diagnostics snapshot are MySQL-runtime verified, including retained error rows, nondiagnostic `SELECT` clearing, and current `max_error_count=0` parse-error snapshot behavior; global scope and `SET` are rejected as in MySQL, with no diagnostics stacks or broader stored-program diagnostics |
| `event_scheduler` | ❌ | Value, scope, SET, diagnostics |
| `explain_format` | ❌ | Value, scope, SET, diagnostics |
| `explain_json_format_version` | ❌ | Value, scope, SET, diagnostics |
| `explicit_defaults_for_timestamp` | ✅ | Fixed modern timestamp-default value `1`/`ON` for session/local/unscoped/global scalar reads and `SHOW VARIABLES`; no-op `SET` forms accepting `DEFAULT`, `ON`, `TRUE`, and `1` preserve the fixed value. Deprecated `OFF` timestamp semantics, mutable global/session state, startup options, implicit first-`TIMESTAMP` defaults, automatic legacy timestamp updates, changed temporal DDL/DML behavior, and Performance Schema variable tables remain unsupported |
| `external_user` | ❌ | Value, scope, SET, diagnostics |
| `flush` | ❌ | Value, scope, SET, diagnostics |
| `flush_time` | ❌ | Value, scope, SET, diagnostics |
| `foreign_key_checks` | ✅ | Session/local/unscoped scalar and expression reads, fixed global reads, `SHOW VARIABLES`, and handle-local session `SET` forms are MySQL-runtime verified for boolean/default values. Disabled checks bypass descriptor-owned FK DML checks/actions and `INSERT IGNORE` FK warnings without retrovalidation. No mutable global state, privileges, startup/persisted values, `SET_VAR`, Performance Schema variable tables, parent-drop bypass, malformed-definition bypass, required-index drop bypass, or broader dependency toggling |
| `ft_boolean_syntax` | ❌ | Value, scope, SET, diagnostics |
| `ft_max_word_len` | ❌ | Value, scope, SET, diagnostics |
| `ft_min_word_len` | ❌ | Value, scope, SET, diagnostics |
| `ft_query_expansion_limit` | ❌ | Value, scope, SET, diagnostics |
| `ft_stopword_file` | ❌ | Value, scope, SET, diagnostics |
| `general_log` | ❌ | Value, scope, SET, diagnostics |
| `general_log_file` | ❌ | Value, scope, SET, diagnostics |
| `generated_random_password_length` | ❌ | Mutable session/global value, range SET, diagnostics |
| `global_connection_memory_limit` | ❌ | Value, scope, SET, diagnostics |
| `global_connection_memory_tracking` | ❌ | Value, scope, SET, diagnostics |
| `group_concat_max_len` | ✅ | Session/local/unscoped scalar reads, fixed global reads, `SHOW VARIABLES`, and handle-local session `SET` forms are MySQL-runtime verified, including `DEFAULT`, integer/user-variable assignment, minimum clamp warning `1292`, unsupported-value diagnostics, byte-capped output, UTF-8-safe truncation, and warning `1260` for supported `GROUP_CONCAT()` forms. No mutable global state, startup/persisted values, `SET_VAR`, Performance Schema variable tables, binary metadata threshold behavior, or broader aggregate syntax |
| `group_replication_advertise_recovery_endpoints` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_allow_local_lower_version_join` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_auto_increment_increment` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_autorejoin_tries` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_bootstrap_group` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_clone_threshold` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_communication_debug_options` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_communication_max_message_size` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_communication_stack` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_components_stop_timeout` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_compression_threshold` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_consistency` | ❌ | Value, scope, SET, diagnostics |
| `group_replication_enforce_update_everywhere_checks` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_exit_state_action` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_flow_control_applier_threshold` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_flow_control_certifier_threshold` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_flow_control_hold_percent` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_flow_control_max_quota` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_flow_control_member_quota_percent` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_flow_control_min_quota` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_flow_control_min_recovery_quota` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_flow_control_mode` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_flow_control_period` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_flow_control_release_percent` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_force_members` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_group_name` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_group_seeds` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_gtid_assignment_block_size` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_ip_allowlist` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_local_address` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_member_expel_timeout` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_member_weight` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_message_cache_size` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_paxos_single_leader` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_poll_spin_loops` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_preemptive_garbage_collection` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_preemptive_garbage_collection_rows_threshold` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_recovery_compression_algorithms` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_recovery_get_public_key` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_recovery_public_key_path` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_recovery_reconnect_interval` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_recovery_retry_count` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_recovery_ssl_ca` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_recovery_ssl_capath` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_recovery_ssl_cert` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_recovery_ssl_cipher` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_recovery_ssl_crl` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_recovery_ssl_crlpath` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_recovery_ssl_key` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_recovery_ssl_verify_server_cert` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_recovery_tls_ciphersuites` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_recovery_tls_version` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_recovery_use_ssl` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_recovery_zstd_compression_level` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_single_primary_mode` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_ssl_mode` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_start_on_boot` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_tls_source` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_transaction_size_limit` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_unreachable_majority_timeout` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `group_replication_view_change_uuid` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `gtid_executed` | 🟡 | Limited empty-string scalar placeholder for default/global reads and `SHOW VARIABLES`; session/local scalar reads return MySQL-style global-variable diagnostics; no GTID set tracking, replication, privileges, or mutable state |
| `gtid_executed_compression_period` | ❌ | Value, scope, SET, diagnostics |
| `gtid_mode` | 🟡 | Limited fixed `OFF` scalar placeholder for default/global reads and `SHOW VARIABLES`; session/local scalar reads return MySQL-style global-variable diagnostics; no GTID enforcement, mode changes, replication, privileges, or mutable state |
| `gtid_next` | ❌ | Value, scope, SET, diagnostics |
| `gtid_owned` | 🟡 | Limited empty-string scalar placeholder for default/global/session/local reads and `SHOW VARIABLES`; read-only; no owned-GTID tracking, replication, privileges, or mutable state |
| `gtid_purged` | 🟡 | Limited empty-string scalar placeholder for default/global reads and `SHOW VARIABLES`; session/local scalar reads return MySQL-style global-variable diagnostics; read-only; no GTID set parsing, `SET GLOBAL gtid_purged`, replication, privileges, or mutable state |
| `have_compress` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `have_dynamic_loading` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `have_geometry` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `have_profiling` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `have_query_cache` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `have_rtree_keys` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `have_statement_timeout` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `have_symlink` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `histogram_generation_max_mem_size` | ❌ | Value, scope, SET, diagnostics |
| `host_cache_size` | ❌ | Value, scope, SET, diagnostics |
| `hostname` | 🟡 | Limited fixed global read-only scalar and `SHOW VARIABLES` placeholder `mylite`; default/global scalar reads and `SHOW VARIABLES` rows are supported, session/local scalar reads return MySQL-style global-variable diagnostics, and assignment returns MySQL-style read-only diagnostics. No host-name probing, DNS behavior, startup option handling, or replication identity behavior |
| `identity` | ❌ | Value, scope, SET, diagnostics |
| `immediate_server_version` | ❌ | Value, scope, SET, diagnostics |
| `information_schema_stats_expiry` | 🟡 | Limited handle-local session scalar reads and `SHOW VARIABLES` rows; session/local/unqualified `SET` assignment for `DEFAULT`, integer literals with optional unary sign, booleans, and integer user variables; values below `0` clamp to `0`, values above `31536000` clamp to `31536000`, and clamping emits warning `1292`. Global reads expose fixed `86400` and mutable global assignment is limited to exact no-op `DEFAULT`/`86400` forms; no actual information-schema statistics cache, shared cross-session statistics state, `ANALYZE TABLE` interaction, startup options, persisted variables, privileges, `SET_VAR` hints, or Performance Schema variable tables |
| `init_connect` | ❌ | Value, scope, SET, diagnostics |
| `init_file` | ❌ | Value, scope, SET, diagnostics |
| `init_replica` | ❌ | Value, scope, SET, diagnostics |
| `init_slave` | ❌ | Value, scope, SET, diagnostics |
| `innodb_adaptive_flushing` | ❌ | Value, scope, SET, diagnostics |
| `innodb_adaptive_flushing_lwm` | ❌ | Value, scope, SET, diagnostics |
| `innodb_adaptive_hash_index` | ❌ | Value, scope, SET, diagnostics |
| `innodb_adaptive_hash_index_parts` | ❌ | Value, scope, SET, diagnostics |
| `innodb_adaptive_max_sleep_delay` | ❌ | Value, scope, SET, diagnostics |
| `innodb_autoextend_increment` | ❌ | Value, scope, SET, diagnostics |
| `innodb_autoinc_lock_mode` | ❌ | Value, scope, SET, diagnostics |
| `innodb_background_drop_list_empty` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `innodb_buffer_pool_chunk_size` | ❌ | Value, scope, SET, diagnostics |
| `innodb_buffer_pool_debug` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `innodb_buffer_pool_dump_at_shutdown` | ❌ | Value, scope, SET, diagnostics |
| `innodb_buffer_pool_dump_now` | ❌ | Value, scope, SET, diagnostics |
| `innodb_buffer_pool_dump_pct` | ❌ | Value, scope, SET, diagnostics |
| `innodb_buffer_pool_filename` | ❌ | Value, scope, SET, diagnostics |
| `innodb_buffer_pool_in_core_file` | ❌ | Value, scope, SET, diagnostics |
| `innodb_buffer_pool_instances` | ❌ | Value, scope, SET, diagnostics |
| `innodb_buffer_pool_load_abort` | ❌ | Value, scope, SET, diagnostics |
| `innodb_buffer_pool_load_at_startup` | ❌ | Value, scope, SET, diagnostics |
| `innodb_buffer_pool_load_now` | ❌ | Value, scope, SET, diagnostics |
| `innodb_buffer_pool_size` | ❌ | Value, scope, SET, diagnostics |
| `innodb_change_buffer_max_size` | ❌ | Value, scope, SET, diagnostics |
| `innodb_change_buffering` | ❌ | Value, scope, SET, diagnostics |
| `innodb_change_buffering_debug` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `innodb_checkpoint_disabled` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `innodb_checksum_algorithm` | ❌ | Value, scope, SET, diagnostics |
| `innodb_cmp_per_index_enabled` | ❌ | Value, scope, SET, diagnostics |
| `innodb_commit_concurrency` | ❌ | Value, scope, SET, diagnostics |
| `innodb_compress_debug` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `innodb_compression_failure_threshold_pct` | ❌ | Value, scope, SET, diagnostics |
| `innodb_compression_level` | ❌ | Value, scope, SET, diagnostics |
| `innodb_compression_pad_pct_max` | ❌ | Value, scope, SET, diagnostics |
| `innodb_concurrency_tickets` | ❌ | Value, scope, SET, diagnostics |
| `innodb_data_file_path` | ❌ | Value, scope, SET, diagnostics |
| `innodb_data_home_dir` | ❌ | Value, scope, SET, diagnostics |
| `innodb_ddl_buffer_size` | ❌ | Value, scope, SET, diagnostics |
| `innodb_ddl_log_crash_reset_debug` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `innodb_ddl_threads` | ❌ | Value, scope, SET, diagnostics |
| `innodb_deadlock_detect` | ❌ | Value, scope, SET, diagnostics |
| `innodb_dedicated_server` | ❌ | Value, scope, SET, diagnostics |
| `innodb_default_row_format` | ❌ | Value, scope, SET, diagnostics |
| `innodb_directories` | ❌ | Value, scope, SET, diagnostics |
| `innodb_disable_sort_file_cache` | ❌ | Value, scope, SET, diagnostics |
| `innodb_doublewrite` | ❌ | Value, scope, SET, diagnostics |
| `innodb_doublewrite_batch_size` | ❌ | Value, scope, SET, diagnostics |
| `innodb_doublewrite_dir` | ❌ | Value, scope, SET, diagnostics |
| `innodb_doublewrite_files` | ❌ | Value, scope, SET, diagnostics |
| `innodb_doublewrite_pages` | ❌ | Value, scope, SET, diagnostics |
| `innodb_extend_and_initialize` | ❌ | Value, scope, SET, diagnostics |
| `innodb_fast_shutdown` | ❌ | Value, scope, SET, diagnostics |
| `innodb_fil_make_page_dirty_debug` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `innodb_file_per_table` | ❌ | Value, scope, SET, diagnostics |
| `innodb_fill_factor` | ❌ | Value, scope, SET, diagnostics |
| `innodb_flush_log_at_timeout` | ❌ | Value, scope, SET, diagnostics |
| `innodb_flush_log_at_trx_commit` | ❌ | Value, scope, SET, diagnostics |
| `innodb_flush_method` | ❌ | Value, scope, SET, diagnostics |
| `innodb_flush_neighbors` | ❌ | Value, scope, SET, diagnostics |
| `innodb_flush_sync` | ❌ | Value, scope, SET, diagnostics |
| `innodb_flushing_avg_loops` | ❌ | Value, scope, SET, diagnostics |
| `innodb_force_load_corrupted` | ❌ | Value, scope, SET, diagnostics |
| `innodb_force_recovery` | ❌ | Value, scope, SET, diagnostics |
| `innodb_fsync_threshold` | ❌ | Value, scope, SET, diagnostics |
| `innodb_ft_aux_table` | ❌ | Value, scope, SET, diagnostics |
| `innodb_ft_cache_size` | ❌ | Value, scope, SET, diagnostics |
| `innodb_ft_enable_diag_print` | ❌ | Value, scope, SET, diagnostics |
| `innodb_ft_enable_stopword` | ❌ | Value, scope, SET, diagnostics |
| `innodb_ft_max_token_size` | ❌ | Value, scope, SET, diagnostics |
| `innodb_ft_min_token_size` | ❌ | Value, scope, SET, diagnostics |
| `innodb_ft_num_word_optimize` | ❌ | Value, scope, SET, diagnostics |
| `innodb_ft_result_cache_limit` | ❌ | Value, scope, SET, diagnostics |
| `innodb_ft_server_stopword_table` | ❌ | Value, scope, SET, diagnostics |
| `innodb_ft_sort_pll_degree` | ❌ | Value, scope, SET, diagnostics |
| `innodb_ft_total_cache_size` | ❌ | Value, scope, SET, diagnostics |
| `innodb_ft_user_stopword_table` | ❌ | Value, scope, SET, diagnostics |
| `innodb_idle_flush_pct` | ❌ | Value, scope, SET, diagnostics |
| `innodb_io_capacity` | ❌ | Value, scope, SET, diagnostics |
| `innodb_io_capacity_max` | ❌ | Value, scope, SET, diagnostics |
| `innodb_limit_optimistic_insert_debug` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `innodb_lock_wait_timeout` | ❌ | Value, scope, SET, diagnostics |
| `innodb_log_buffer_size` | ❌ | Value, scope, SET, diagnostics |
| `innodb_log_checkpoint_fuzzy_now` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `innodb_log_checkpoint_now` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `innodb_log_checksums` | ❌ | Value, scope, SET, diagnostics |
| `innodb_log_compressed_pages` | ❌ | Value, scope, SET, diagnostics |
| `innodb_log_file_size` | ❌ | Value, scope, SET, diagnostics |
| `innodb_log_files_in_group` | ❌ | Value, scope, SET, diagnostics |
| `innodb_log_group_home_dir` | ❌ | Value, scope, SET, diagnostics |
| `innodb_log_spin_cpu_abs_lwm` | ❌ | Value, scope, SET, diagnostics |
| `innodb_log_spin_cpu_pct_hwm` | ❌ | Value, scope, SET, diagnostics |
| `innodb_log_wait_for_flush_spin_hwm` | ❌ | Value, scope, SET, diagnostics |
| `innodb_log_write_ahead_size` | ❌ | Value, scope, SET, diagnostics |
| `innodb_log_writer_threads` | ❌ | Value, scope, SET, diagnostics |
| `innodb_lru_scan_depth` | ❌ | Value, scope, SET, diagnostics |
| `innodb_max_dirty_pages_pct` | ❌ | Value, scope, SET, diagnostics |
| `innodb_max_dirty_pages_pct_lwm` | ❌ | Value, scope, SET, diagnostics |
| `innodb_max_purge_lag` | ❌ | Value, scope, SET, diagnostics |
| `innodb_max_purge_lag_delay` | ❌ | Value, scope, SET, diagnostics |
| `innodb_max_undo_log_size` | ❌ | Value, scope, SET, diagnostics |
| `innodb_merge_threshold_set_all_debug` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `innodb_monitor_disable` | ❌ | Value, scope, SET, diagnostics |
| `innodb_monitor_enable` | ❌ | Value, scope, SET, diagnostics |
| `innodb_monitor_reset` | ❌ | Value, scope, SET, diagnostics |
| `innodb_monitor_reset_all` | ❌ | Value, scope, SET, diagnostics |
| `innodb_numa_interleave` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `innodb_old_blocks_pct` | ❌ | Value, scope, SET, diagnostics |
| `innodb_old_blocks_time` | ❌ | Value, scope, SET, diagnostics |
| `innodb_online_alter_log_max_size` | ❌ | Value, scope, SET, diagnostics |
| `innodb_open_files` | ❌ | Value, scope, SET, diagnostics |
| `innodb_optimize_fulltext_only` | ❌ | Value, scope, SET, diagnostics |
| `innodb_page_cleaners` | ❌ | Value, scope, SET, diagnostics |
| `innodb_page_size` | ❌ | Value, scope, SET, diagnostics |
| `innodb_parallel_read_threads` | ❌ | Value, scope, SET, diagnostics |
| `innodb_print_all_deadlocks` | ❌ | Value, scope, SET, diagnostics |
| `innodb_print_ddl_logs` | ❌ | Value, scope, SET, diagnostics |
| `innodb_purge_batch_size` | ❌ | Value, scope, SET, diagnostics |
| `innodb_purge_rseg_truncate_frequency` | ❌ | Value, scope, SET, diagnostics |
| `innodb_purge_threads` | ❌ | Value, scope, SET, diagnostics |
| `innodb_random_read_ahead` | ❌ | Value, scope, SET, diagnostics |
| `innodb_read_ahead_threshold` | ❌ | Value, scope, SET, diagnostics |
| `innodb_read_io_threads` | ❌ | Value, scope, SET, diagnostics |
| `innodb_read_only` | 🟡 | Limited fixed global read-only disabled placeholder: scalar default/global reads return `0`, `SHOW VARIABLES` rows report `OFF`, session/local scalar reads return MySQL-style global-variable diagnostics, and every `SET` form returns MySQL-style read-only diagnostics; no startup option, mutable value, InnoDB read-only mode, write blocking, or data-dictionary effects |
| `innodb_redo_log_archive_dirs` | ❌ | Value, scope, SET, diagnostics |
| `innodb_redo_log_capacity` | ❌ | Value, scope, SET, diagnostics |
| `innodb_redo_log_encrypt` | ❌ | Value, scope, SET, diagnostics |
| `innodb_replication_delay` | ❌ | Value, scope, SET, diagnostics |
| `innodb_rollback_on_timeout` | ❌ | Value, scope, SET, diagnostics |
| `innodb_rollback_segments` | ❌ | Value, scope, SET, diagnostics |
| `innodb_saved_page_number_debug` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `innodb_segment_reserve_factor` | ❌ | Value, scope, SET, diagnostics |
| `innodb_sort_buffer_size` | ❌ | Value, scope, SET, diagnostics |
| `innodb_spin_wait_delay` | ❌ | Value, scope, SET, diagnostics |
| `innodb_spin_wait_pause_multiplier` | ❌ | Value, scope, SET, diagnostics |
| `innodb_stats_auto_recalc` | ❌ | Value, scope, SET, diagnostics |
| `innodb_stats_include_delete_marked` | ❌ | Value, scope, SET, diagnostics |
| `innodb_stats_method` | ❌ | Value, scope, SET, diagnostics |
| `innodb_stats_on_metadata` | ❌ | Value, scope, SET, diagnostics |
| `innodb_stats_persistent` | ❌ | Value, scope, SET, diagnostics |
| `innodb_stats_persistent_sample_pages` | ❌ | Value, scope, SET, diagnostics |
| `innodb_stats_transient_sample_pages` | ❌ | Value, scope, SET, diagnostics |
| `innodb_status_output` | ❌ | Value, scope, SET, diagnostics |
| `innodb_status_output_locks` | ❌ | Value, scope, SET, diagnostics |
| `innodb_strict_mode` | ❌ | Value, scope, SET, diagnostics |
| `innodb_sync_array_size` | ❌ | Value, scope, SET, diagnostics |
| `innodb_sync_debug` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `innodb_sync_spin_loops` | ❌ | Value, scope, SET, diagnostics |
| `innodb_table_locks` | ❌ | Value, scope, SET, diagnostics |
| `innodb_temp_data_file_path` | ❌ | Value, scope, SET, diagnostics |
| `innodb_temp_tablespaces_dir` | ❌ | Value, scope, SET, diagnostics |
| `innodb_thread_concurrency` | ❌ | Value, scope, SET, diagnostics |
| `innodb_thread_sleep_delay` | ❌ | Value, scope, SET, diagnostics |
| `innodb_tmpdir` | ❌ | Value, scope, SET, diagnostics |
| `innodb_trx_purge_view_update_only_debug` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `innodb_trx_rseg_n_slots_debug` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `innodb_undo_directory` | ❌ | Value, scope, SET, diagnostics |
| `innodb_undo_log_encrypt` | ❌ | Value, scope, SET, diagnostics |
| `innodb_undo_log_truncate` | ❌ | Value, scope, SET, diagnostics |
| `innodb_undo_tablespaces` | ❌ | Value, scope, SET, diagnostics |
| `innodb_use_fdatasync` | ❌ | Value, scope, SET, diagnostics |
| `innodb_use_native_aio` | ❌ | Value, scope, SET, diagnostics |
| `innodb_validate_tablespace_paths` | ❌ | Value, scope, SET, diagnostics |
| `innodb_version` | ❌ | Value, scope, SET, diagnostics |
| `innodb_write_io_threads` | ❌ | Value, scope, SET, diagnostics |
| `insert_id` | ❌ | Value, scope, SET, diagnostics |
| `interactive_timeout` | 🟡 | Limited handle-local session scalar reads, `SHOW VARIABLES` rows, and session/local/unqualified `SET` assignment with MySQL-compatible integer range `1..31536000`, `DEFAULT = 28800`, boolean conversion, clamp warnings, and integer user-variable assignment. Global reads expose fixed `28800` and mutable global assignment is limited to exact no-op `DEFAULT`/`28800` forms; no idle timeout enforcement, protocol behavior, startup options, persisted state, privileges, or Performance Schema rows |
| `internal_tmp_mem_storage_engine` | ❌ | Value, scope, SET, diagnostics |
| `join_buffer_size` | ❌ | Value, scope, SET, diagnostics |
| `keep_files_on_create` | ✅ | Fixed default/session/global scalar `0` and `SHOW VARIABLES` `OFF`; session placeholder `SET` readback and fixed global no-op assignments are supported. No temporary-file retention behavior, startup options, persisted state, or Performance Schema variable tables |
| `key_buffer_size` | ❌ | Value, scope, SET, diagnostics |
| `key_cache_age_threshold` | ❌ | Value, scope, SET, diagnostics |
| `key_cache_block_size` | ❌ | Value, scope, SET, diagnostics |
| `key_cache_division_limit` | ❌ | Value, scope, SET, diagnostics |
| `keyring_aws_cmk_id` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `keyring_aws_conf_file` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `keyring_aws_data_file` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `keyring_aws_region` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `keyring_hashicorp_auth_path` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `keyring_hashicorp_ca_path` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `keyring_hashicorp_caching` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `keyring_hashicorp_commit_auth_path` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `keyring_hashicorp_commit_ca_path` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `keyring_hashicorp_commit_caching` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `keyring_hashicorp_commit_role_id` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `keyring_hashicorp_commit_server_url` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `keyring_hashicorp_commit_store_path` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `keyring_hashicorp_role_id` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `keyring_hashicorp_secret_id` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `keyring_hashicorp_server_url` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `keyring_hashicorp_store_path` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `keyring_okv_conf_dir` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `keyring_operations` | ❌ | Value, scope, SET, diagnostics |
| `large_files_support` | ❌ | Value, scope, SET, diagnostics |
| `large_page_size` | ❌ | Value, scope, SET, diagnostics |
| `large_pages` | ❌ | Value, scope, SET, diagnostics |
| `last_insert_id` | ❌ | Value, scope, SET, diagnostics |
| `lc_messages` | ❌ | Value, scope, SET, diagnostics |
| `lc_messages_dir` | ❌ | Value, scope, SET, diagnostics |
| `lc_time_names` | ❌ | Value, scope, SET, diagnostics |
| `license` | 🟡 | Limited fixed global read-only scalar and `SHOW VARIABLES` compatibility placeholder `GPL`; default/global scalar reads and `SHOW VARIABLES` rows are supported, session/local scalar reads return MySQL-style global-variable diagnostics, and assignment returns MySQL-style read-only diagnostics. The placeholder is not a project license notice; no server-build license discovery or startup option behavior |
| `local_infile` | ❌ | Value, scope, SET, diagnostics |
| `lock_order` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `lock_order_debug_loop` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `lock_order_debug_missing_arc` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `lock_order_debug_missing_key` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `lock_order_debug_missing_unlock` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `lock_order_dependencies` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `lock_order_extra_dependencies` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `lock_order_output_directory` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `lock_order_print_txt` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `lock_order_trace_loop` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `lock_order_trace_missing_arc` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `lock_order_trace_missing_key` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `lock_order_trace_missing_unlock` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `lock_wait_timeout` | ❌ | Value, scope, SET, diagnostics |
| `locked_in_memory` | ❌ | Value, scope, SET, diagnostics |
| `log_bin` | 🟡 | Limited fixed global scalar value `1`; `SHOW VARIABLES` displays `ON`; default/global scalar reads and `SHOW VARIABLES` rows are supported; session/local scalar reads return MySQL-style global-variable diagnostics; assignment returns MySQL-style read-only diagnostics; no binary log files, GTID recovery, replication side effects, startup option handling, or mutable state |
| `log_bin_basename` | 🟡 | Limited fixed global scalar and `SHOW VARIABLES` value `binlog`; default/global scalar reads and `SHOW VARIABLES` rows are supported; session/local scalar reads return MySQL-style global-variable diagnostics; assignment returns MySQL-style read-only diagnostics; no configured data directory, binary log files, path expansion, rotation, or startup option handling |
| `log_bin_index` | 🟡 | Limited fixed global scalar and `SHOW VARIABLES` value `binlog.index`; default/global scalar reads and `SHOW VARIABLES` rows are supported; session/local scalar reads return MySQL-style global-variable diagnostics; assignment returns MySQL-style read-only diagnostics; no binary log index file, configured path, rotation, or startup option handling |
| `log_bin_trust_function_creators` | 🟡 | Limited fixed global scalar value `0`; `SHOW VARIABLES` displays `OFF`; default/global scalar reads and `SHOW VARIABLES` rows are supported; scalar reads and successful exact no-op global `SET` forms emit MySQL-style deprecation warning `1287`; session/local scalar reads and non-global `SET` return MySQL-style global-variable diagnostics; no mutable global state, stored-function privilege behavior, trigger behavior, binary logging, or startup option handling |
| `log_error` | ❌ | Value, scope, SET, diagnostics |
| `log_error_services` | ❌ | Value, scope, SET, diagnostics |
| `log_error_suppression_list` | ❌ | Value, scope, SET, diagnostics |
| `log_error_verbosity` | ❌ | Value, scope, SET, diagnostics |
| `log_output` | ❌ | Value, scope, SET, diagnostics |
| `log_queries_not_using_indexes` | ❌ | Value, scope, SET, diagnostics |
| `log_raw` | ❌ | Value, scope, SET, diagnostics |
| `log_replica_updates` | ❌ | Value, scope, SET, diagnostics |
| `log_slave_updates` | ❌ | Value, scope, SET, diagnostics |
| `log_slow_admin_statements` | ❌ | Value, scope, SET, diagnostics |
| `log_slow_extra` | ❌ | Value, scope, SET, diagnostics |
| `log_slow_replica_statements` | ❌ | Value, scope, SET, diagnostics |
| `log_slow_slave_statements` | ❌ | Value, scope, SET, diagnostics |
| `log_statements_unsafe_for_binlog` | ❌ | Value, scope, SET, diagnostics |
| `log_throttle_queries_not_using_indexes` | ❌ | Value, scope, SET, diagnostics |
| `log_timestamps` | ❌ | Value, scope, SET, diagnostics |
| `long_query_time` | ❌ | Value, scope, SET, diagnostics |
| `low_priority_updates` | ❌ | Value, scope, SET, diagnostics |
| `lower_case_file_system` | 🟡 | Limited fixed global read-only scalar value `0`; `SHOW VARIABLES` displays `OFF`; default/global scalar reads and `SHOW VARIABLES` rows are supported; session/local scalar reads return MySQL-style global-variable diagnostics; no host filesystem probing, startup option handling, value `1`, or changed identifier behavior |
| `lower_case_table_names` | 🟡 | Limited fixed global read-only scalar value `0`; default/global scalar reads and `SHOW VARIABLES` rows are supported; session/local scalar reads return MySQL-style global-variable diagnostics; no startup option handling, values `1`/`2`, case-insensitive schema/table lookup, catalog collation changes, or coupling to `lower_case_file_system` |
| `mandatory_roles` | ❌ | Value, scope, SET, diagnostics |
| `master_verify_checksum` | ❌ | Value, scope, SET, diagnostics |
| `max_allowed_packet` | 🟡 | Limited fixed scalar value `67108864`; default/global/session/local scalar reads and `SHOW VARIABLES` rows are supported; non-global `SET` targets return MySQL-style session-read-only diagnostics; exact no-op global `SET` forms may preserve the fixed value; no mutable state, packet-size enforcement, string/result-buffer limits, rounding warnings, or privilege semantics |
| `max_binlog_cache_size` | ❌ | Value, scope, SET, diagnostics |
| `max_binlog_size` | ❌ | Value, scope, SET, diagnostics |
| `max_binlog_stmt_cache_size` | ❌ | Value, scope, SET, diagnostics |
| `max_connect_errors` | ❌ | Value, scope, SET, diagnostics |
| `max_connections` | ❌ | Value, scope, SET, diagnostics |
| `max_delayed_threads` | ❌ | Value, scope, SET, diagnostics |
| `max_digest_length` | ❌ | Value, scope, SET, diagnostics |
| `max_error_count` | 🟡 | Limited scalar `SELECT @@max_error_count` with no scope, `session`, `local`, or fixed `global`; `SHOW VARIABLES` rows; handle-local session `SET` assignment for no scope, `SESSION`, `LOCAL`, direct `@@variable`, `@@session`, and `@@local` forms using `DEFAULT`, unsigned integer, unary-signed integer, `TRUE`/`FALSE`, and integer user-variable values; values clamp to `0..65535` with warning `1292`; retained diagnostics rows are capped while total warning counts are preserved for current warning/note producers. Global reads remain fixed at `1024`, exact default global no-op assignments may be accepted, and there is no mutable shared global state, persisted/startup value, privilege model, Performance Schema variable table, diagnostics stack, or broader warning producer coverage |
| `max_execution_time` | ❌ | Value, scope, SET, diagnostics |
| `max_heap_table_size` | ❌ | Value, scope, SET, diagnostics |
| `max_insert_delayed_threads` | ❌ | Value, scope, SET, diagnostics |
| `max_join_size` | ❌ | Value, scope, SET, diagnostics |
| `max_length_for_sort_data` | ❌ | Value, scope, SET, diagnostics |
| `max_points_in_geometry` | ❌ | Value, scope, SET, diagnostics |
| `max_prepared_stmt_count` | ❌ | Value, scope, SET, diagnostics |
| `max_relay_log_size` | ❌ | Value, scope, SET, diagnostics |
| `max_seeks_for_key` | ❌ | Value, scope, SET, diagnostics |
| `max_sort_length` | ❌ | Value, scope, SET, diagnostics |
| `max_sp_recursion_depth` | ❌ | Value, scope, SET, diagnostics |
| `max_user_connections` | ❌ | Value, scope, SET, diagnostics |
| `max_write_lock_count` | ❌ | Value, scope, SET, diagnostics |
| `mecab_rc_file` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `min_examined_row_limit` | ❌ | Value, scope, SET, diagnostics |
| `myisam_data_pointer_size` | ❌ | Value, scope, SET, diagnostics |
| `myisam_max_sort_file_size` | ❌ | Value, scope, SET, diagnostics |
| `myisam_mmap_size` | ❌ | Value, scope, SET, diagnostics |
| `myisam_recover_options` | ❌ | Value, scope, SET, diagnostics |
| `myisam_sort_buffer_size` | ❌ | Value, scope, SET, diagnostics |
| `myisam_stats_method` | ❌ | Value, scope, SET, diagnostics |
| `myisam_use_mmap` | ❌ | Value, scope, SET, diagnostics |
| `mysql_firewall_database` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `mysql_firewall_mode` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `mysql_firewall_reload_interval_seconds` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `mysql_firewall_trace` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `mysql_native_password_proxy_users` | ✅ | Fixed global readback/SHOW and exact global no-op SET |
| `mysqlx_bind_address` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `mysqlx_compression_algorithms` | ✅ | Fixed global readback/SHOW and `DEFAULT` global no-op SET |
| `mysqlx_connect_timeout` | ✅ | Fixed global readback/SHOW and `DEFAULT` global no-op SET |
| `mysqlx_deflate_default_compression_level` | ✅ | Fixed global readback/SHOW and `DEFAULT` global no-op SET |
| `mysqlx_deflate_max_client_compression_level` | ✅ | Fixed global readback/SHOW and `DEFAULT` global no-op SET |
| `mysqlx_document_id_unique_prefix` | ✅ | Fixed global readback/SHOW and `DEFAULT` global no-op SET |
| `mysqlx_enable_hello_notice` | ✅ | Fixed global scalar `1`/SHOW `ON` and `DEFAULT` global no-op SET |
| `mysqlx_idle_worker_thread_timeout` | ✅ | Fixed global readback/SHOW and `DEFAULT` global no-op SET |
| `mysqlx_interactive_timeout` | ✅ | Fixed global readback/SHOW and `DEFAULT` global no-op SET |
| `mysqlx_lz4_default_compression_level` | ✅ | Fixed global readback/SHOW and `DEFAULT` global no-op SET |
| `mysqlx_lz4_max_client_compression_level` | ✅ | Fixed global readback/SHOW and `DEFAULT` global no-op SET |
| `mysqlx_max_allowed_packet` | ✅ | Fixed global readback/SHOW and `DEFAULT` global no-op SET |
| `mysqlx_max_connections` | ✅ | Fixed global readback/SHOW and `DEFAULT` global no-op SET |
| `mysqlx_min_worker_threads` | ✅ | Fixed global readback/SHOW and `DEFAULT` global no-op SET |
| `mysqlx_port` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `mysqlx_port_open_timeout` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `mysqlx_read_timeout` | ✅ | Fixed global readback plus session/local readback and handle-local SET subset |
| `mysqlx_socket` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `mysqlx_ssl_ca` | ✅ | Fixed global `NULL`/blank readback/SHOW and read-only SET diagnostics |
| `mysqlx_ssl_capath` | ✅ | Fixed global `NULL`/blank readback/SHOW and read-only SET diagnostics |
| `mysqlx_ssl_cert` | ✅ | Fixed global `NULL`/blank readback/SHOW and read-only SET diagnostics |
| `mysqlx_ssl_cipher` | ✅ | Fixed global `NULL`/blank readback/SHOW and read-only SET diagnostics |
| `mysqlx_ssl_crl` | ✅ | Fixed global `NULL`/blank readback/SHOW and read-only SET diagnostics |
| `mysqlx_ssl_crlpath` | ✅ | Fixed global `NULL`/blank readback/SHOW and read-only SET diagnostics |
| `mysqlx_ssl_key` | ✅ | Fixed global `NULL`/blank readback/SHOW and read-only SET diagnostics |
| `mysqlx_wait_timeout` | ✅ | Fixed global readback plus session/local readback and handle-local SET subset |
| `mysqlx_write_timeout` | ✅ | Fixed global readback plus session/local readback and handle-local SET subset |
| `mysqlx_zstd_default_compression_level` | ✅ | Fixed global readback/SHOW and `DEFAULT` global no-op SET |
| `mysqlx_zstd_max_client_compression_level` | ✅ | Fixed global readback/SHOW and `DEFAULT` global no-op SET |
| `named_pipe` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `named_pipe_full_access_group` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_allow_copying_alter_table` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_applier_allow_skip_epoch` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_autoincrement_prefetch_sz` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_batch_size` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_blob_read_batch_bytes` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_blob_write_batch_bytes` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_clear_apply_status` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_cluster_connection_pool` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_cluster_connection_pool_nodeids` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_conflict_role` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_data_node_neighbour` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_dbg_check_shares` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_default_column_format` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_deferred_constraints` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_distribution` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_eventbuffer_free_percent` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_eventbuffer_max_alloc` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_extra_logging` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_force_send` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_fully_replicated` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_index_stat_enable` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_index_stat_option` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_join_pushdown` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_log_apply_status` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_log_bin` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_log_binlog_index` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_log_cache_size` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_log_empty_epochs` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_log_empty_update` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_log_exclusive_reads` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_log_fail_terminate` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_log_orig` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_log_transaction_compression` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_log_transaction_compression_level_zstd` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_log_transaction_dependency` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_log_transaction_id` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_log_update_as_write` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_log_update_minimal` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_log_updated_only` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_metadata_check` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_metadata_check_interval` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_metadata_sync` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_mgm_tls` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_optimization_delay` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_optimized_node_selection` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_read_backup` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_recv_thread_activation_threshold` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_recv_thread_cpu_mask` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_replica_batch_size` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_replica_blob_write_batch_bytes` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `Ndb_replica_max_replicated_epoch` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_report_thresh_binlog_epoch_slip` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_report_thresh_binlog_mem_usage` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_row_checksum` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_schema_dist_lock_wait_timeout` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_schema_dist_timeout` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_schema_dist_upgrade_allowed` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `Ndb_schema_participant_count` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_show_foreign_key_mock_tables` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_slave_conflict_role` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `Ndb_system_name` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_table_no_logging` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_table_temporary` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_tls_search_path` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_use_copying_alter_table` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_use_exact_count` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_use_transactions` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_version` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_version_string` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_wait_connected` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndb_wait_setup` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndbinfo_database` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndbinfo_max_bytes` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndbinfo_max_rows` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndbinfo_offline` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndbinfo_show_hidden` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndbinfo_table_prefix` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `ndbinfo_version` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `net_buffer_length` | ❌ | Value, scope, SET, diagnostics |
| `net_read_timeout` | ❌ | Value, scope, SET, diagnostics |
| `net_retry_count` | ❌ | Value, scope, SET, diagnostics |
| `net_write_timeout` | ❌ | Value, scope, SET, diagnostics |
| `ngram_token_size` | ❌ | Value, scope, SET, diagnostics |
| `offline_mode` | ❌ | Value, scope, SET, diagnostics |
| `old_alter_table` | ✅ | Fixed default/session/global scalar `0` and `SHOW VARIABLES` `OFF`; session placeholder `SET` readback and fixed global no-op assignments are supported. No alternate ALTER TABLE algorithm routing, startup options, persisted state, or Performance Schema variable tables |
| `open_files_limit` | ❌ | Value, scope, SET, diagnostics |
| `optimizer_prune_level` | ❌ | Value, scope, SET, diagnostics |
| `optimizer_search_depth` | ❌ | Value, scope, SET, diagnostics |
| `optimizer_switch` | ❌ | Value, scope, SET, diagnostics |
| `optimizer_trace` | ❌ | Value, scope, SET, diagnostics |
| `optimizer_trace_features` | ❌ | Value, scope, SET, diagnostics |
| `optimizer_trace_limit` | ❌ | Value, scope, SET, diagnostics |
| `optimizer_trace_max_mem_size` | ❌ | Value, scope, SET, diagnostics |
| `optimizer_trace_offset` | ❌ | Value, scope, SET, diagnostics |
| `original_commit_timestamp` | ❌ | Value, scope, SET, diagnostics |
| `original_server_version` | ❌ | Value, scope, SET, diagnostics |
| `parser_max_mem_size` | ❌ | Value, scope, SET, diagnostics |
| `partial_revokes` | ❌ | Value, scope, SET, diagnostics |
| `password_history` | ✅ | Fixed global readback/SHOW and exact global no-op SET |
| `password_require_current` | ✅ | Fixed global readback/SHOW and exact global no-op SET |
| `password_reuse_interval` | ✅ | Fixed global readback/SHOW and exact global no-op SET |
| `performance_schema` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_accounts_size` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_digests_size` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_error_size` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_events_stages_history_long_size` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_events_stages_history_size` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_events_statements_history_long_size` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_events_statements_history_size` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_events_transactions_history_long_size` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_events_transactions_history_size` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_events_waits_history_long_size` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_events_waits_history_size` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_hosts_size` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_cond_classes` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_cond_instances` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_digest_length` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_digest_sample_age` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_file_classes` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_file_handles` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_file_instances` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_index_stat` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_memory_classes` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_metadata_locks` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_meter_classes` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_metric_classes` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_mutex_classes` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_mutex_instances` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_prepared_statements_instances` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_program_instances` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_rwlock_classes` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_rwlock_instances` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_socket_classes` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_socket_instances` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_sql_text_length` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_stage_classes` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_statement_classes` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_statement_stack` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_table_handles` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_table_instances` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_table_lock_stat` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_thread_classes` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_max_thread_instances` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_session_connect_attrs_size` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_setup_actors_size` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_setup_objects_size` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_show_processlist` | ❌ | Value, scope, SET, diagnostics |
| `performance_schema_users_size` | ❌ | Value, scope, SET, diagnostics |
| `persist_only_admin_x509_subject` | ❌ | Value, scope, SET, diagnostics |
| `persist_sensitive_variables_in_plaintext` | ❌ | Value, scope, SET, diagnostics |
| `persisted_globals_load` | ❌ | Value, scope, SET, diagnostics |
| `pid_file` | 🟡 | Limited fixed global read-only scalar and `SHOW VARIABLES` placeholder `/var/run/mysqld/mysqld.pid`; default/global scalar reads and `SHOW VARIABLES` rows are supported, session/local scalar reads return MySQL-style global-variable diagnostics, and assignment returns MySQL-style read-only diagnostics. No PID file is created or read |
| `plugin_dir` | 🟡 | Limited fixed global read-only scalar and `SHOW VARIABLES` placeholder `/usr/lib64/mysql/plugin/`; default/global scalar reads and `SHOW VARIABLES` rows are supported, session/local scalar reads return MySQL-style global-variable diagnostics, and assignment returns MySQL-style read-only diagnostics. No plugin loading or path probing |
| `port` | 🟡 | Limited fixed global read-only numeric scalar and `SHOW VARIABLES` placeholder `3306`; default/global scalar reads and numeric scalar contexts such as `HEX(@@port)` are supported, `SHOW VARIABLES` rows are supported, session/local scalar reads return MySQL-style global-variable diagnostics, and assignment returns MySQL-style read-only diagnostics. No network listener, socket binding, startup option handling, or mutable port state |
| `preload_buffer_size` | ❌ | Value, scope, SET, diagnostics |
| `print_identified_with_as_hex` | ✅ | Fixed default/session/global scalar `0` and `SHOW VARIABLES` `OFF`; session placeholder `SET` readback and fixed global no-op assignments are supported. No account-authentication formatting behavior, startup options, persisted state, or Performance Schema variable tables |
| `profiling` | ❌ | Value, scope, SET, diagnostics |
| `profiling_history_size` | ❌ | Value, scope, SET, diagnostics |
| `protocol_compression_algorithms` | ❌ | Value, scope, SET, diagnostics |
| `protocol_version` | 🟡 | Limited fixed global read-only numeric scalar and `SHOW VARIABLES` placeholder `10`; default/global scalar reads and numeric scalar contexts such as `HEX(@@protocol_version)` are supported, `SHOW VARIABLES` rows are supported, session/local scalar reads return MySQL-style global-variable diagnostics, and assignment returns MySQL-style read-only diagnostics. No wire-protocol handshake metadata, startup option handling, or mutable protocol state |
| `proxy_user` | ❌ | Value, scope, SET, diagnostics |
| `pseudo_replica_mode` | ❌ | Value, scope, SET, diagnostics |
| `pseudo_slave_mode` | ❌ | Value, scope, SET, diagnostics |
| `pseudo_thread_id` | ❌ | Value, scope, SET, diagnostics |
| `query_alloc_block_size` | ❌ | Value, scope, SET, diagnostics |
| `query_prealloc_size` | ❌ | Value, scope, SET, diagnostics |
| `rand_seed1` | ❌ | Value, scope, SET, diagnostics |
| `rand_seed2` | ❌ | Value, scope, SET, diagnostics |
| `range_alloc_block_size` | ❌ | Value, scope, SET, diagnostics |
| `range_optimizer_max_mem_size` | ❌ | Value, scope, SET, diagnostics |
| `rbr_exec_mode` | ❌ | Value, scope, SET, diagnostics |
| `read_buffer_size` | ❌ | Value, scope, SET, diagnostics |
| `read_only` | 🟡 | Limited fixed global disabled placeholder: scalar default/global reads return `0`, `SHOW VARIABLES` rows report `OFF`, session/local scalar reads and non-global `SET` forms use MySQL-shaped global-variable diagnostics, and exact no-op global `SET` forms may preserve `OFF`; no mutable global state, write blocking, privileges, replication exceptions, lock waits, startup option, or interaction with `super_read_only` |
| `read_rnd_buffer_size` | ❌ | Value, scope, SET, diagnostics |
| `regexp_stack_limit` | ❌ | Value, scope, SET, diagnostics |
| `regexp_time_limit` | ❌ | Value, scope, SET, diagnostics |
| `relay_log` | ❌ | Value, scope, SET, diagnostics |
| `relay_log_basename` | ❌ | Value, scope, SET, diagnostics |
| `relay_log_index` | ❌ | Value, scope, SET, diagnostics |
| `relay_log_purge` | ❌ | Value, scope, SET, diagnostics |
| `relay_log_recovery` | ❌ | Value, scope, SET, diagnostics |
| `relay_log_space_limit` | ❌ | Value, scope, SET, diagnostics |
| `replica_allow_batching` | ❌ | Value, scope, SET, diagnostics |
| `replica_checkpoint_group` | ❌ | Value, scope, SET, diagnostics |
| `replica_checkpoint_period` | ❌ | Value, scope, SET, diagnostics |
| `replica_compressed_protocol` | ❌ | Value, scope, SET, diagnostics |
| `replica_exec_mode` | ❌ | Value, scope, SET, diagnostics |
| `replica_load_tmpdir` | ❌ | Value, scope, SET, diagnostics |
| `replica_max_allowed_packet` | ❌ | Value, scope, SET, diagnostics |
| `replica_net_timeout` | ❌ | Value, scope, SET, diagnostics |
| `replica_parallel_type` | ❌ | Value, scope, SET, diagnostics |
| `replica_parallel_workers` | ❌ | Value, scope, SET, diagnostics |
| `replica_pending_jobs_size_max` | ❌ | Value, scope, SET, diagnostics |
| `replica_preserve_commit_order` | ❌ | Value, scope, SET, diagnostics |
| `replica_skip_errors` | ❌ | Value, scope, SET, diagnostics |
| `replica_sql_verify_checksum` | ❌ | Value, scope, SET, diagnostics |
| `replica_transaction_retries` | ❌ | Value, scope, SET, diagnostics |
| `replica_type_conversions` | ❌ | Value, scope, SET, diagnostics |
| `replication_optimize_for_static_plugin_config` | ❌ | Value, scope, SET, diagnostics |
| `replication_sender_observe_commit_only` | ❌ | Value, scope, SET, diagnostics |
| `report_host` | ❌ | Value, scope, SET, diagnostics |
| `report_password` | ❌ | Value, scope, SET, diagnostics |
| `report_port` | ❌ | Value, scope, SET, diagnostics |
| `report_user` | ❌ | Value, scope, SET, diagnostics |
| `require_row_format` | ✅ | Fixed default/session/local scalar `0`, `SHOW VARIABLES` `OFF`, and MySQL-style global-scope diagnostics; session placeholder `SET` readback is supported. No row-format enforcement, global value, startup options, persisted state, or Performance Schema variable tables |
| `require_secure_transport` | ✅ | Fixed global scalar `0`, `SHOW VARIABLES` `OFF`, global-only diagnostics, and exact no-op global `SET`; no transport enforcement, TLS state, startup options, persisted state, or Performance Schema variable tables |
| `restrict_fk_on_non_standard_key` | ❌ | Value, scope, SET, diagnostics |
| `resultset_metadata` | ✅ | Fixed default/session/local scalar and `SHOW VARIABLES` value `FULL`, plus MySQL-style global-scope diagnostics; session placeholder `SET` readback is supported. No protocol metadata negotiation, global value, persisted state, or Performance Schema variable tables |
| `rewriter_enabled` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rewriter_enabled_for_threads_without_privilege_checks` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rewriter_verbose` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rpl_read_size` | ❌ | Value, scope, SET, diagnostics |
| `rpl_semi_sync_master_enabled` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rpl_semi_sync_master_timeout` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rpl_semi_sync_master_trace_level` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rpl_semi_sync_master_wait_for_slave_count` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rpl_semi_sync_master_wait_no_slave` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rpl_semi_sync_master_wait_point` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rpl_semi_sync_replica_enabled` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rpl_semi_sync_replica_trace_level` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rpl_semi_sync_slave_enabled` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rpl_semi_sync_slave_trace_level` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rpl_semi_sync_source_enabled` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rpl_semi_sync_source_timeout` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rpl_semi_sync_source_trace_level` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rpl_semi_sync_source_wait_for_replica_count` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rpl_semi_sync_source_wait_no_replica` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rpl_semi_sync_source_wait_point` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `rpl_stop_replica_timeout` | ❌ | Value, scope, SET, diagnostics |
| `rpl_stop_slave_timeout` | ❌ | Value, scope, SET, diagnostics |
| `schema_definition_cache` | ❌ | Value, scope, SET, diagnostics |
| `secondary_engine_cost_threshold` | ❌ | Value, scope, SET, diagnostics |
| `secure_file_priv` | ✅ | Fixed global read-only scalar and `SHOW VARIABLES` value `/var/lib/mysql-files/`; no file import/export enforcement, path probing, startup options, persisted state, or Performance Schema variable tables |
| `select_into_buffer_size` | ❌ | Value, scope, SET, diagnostics |
| `select_into_disk_sync` | ✅ | Fixed default/session/global scalar `0` and `SHOW VARIABLES` `OFF`; session placeholder `SET` readback and fixed global no-op assignments are supported. No server-side file output fsync behavior, startup options, persisted state, or Performance Schema variable tables |
| `select_into_disk_sync_delay` | ❌ | Value, scope, SET, diagnostics |
| `server_id` | 🟡 | Limited fixed global scalar and `SHOW VARIABLES` value `1`; default/global scalar reads and `SHOW VARIABLES` rows are supported; session/local scalar reads and non-global `SET` return MySQL-style global-variable diagnostics; exact no-op global `SET` forms may preserve `1`; no mutable global state, configured replication identity, startup option handling, or replication side effects |
| `server_id_bits` | 🟡 | Limited fixed global scalar and `SHOW VARIABLES` value `32`; default/global scalar reads and `SHOW VARIABLES` rows are supported; session/local scalar reads and non-global `SET` return MySQL-style global-variable diagnostics; exact no-op global `SET` forms may preserve `32`; no mutable global state, server ID bit-width changes, startup option handling, or replication side effects |
| `server_uuid` | 🟡 | Limited fixed global scalar and `SHOW VARIABLES` UUID-shaped MyLite placeholder `4d796c69-7465-4000-8000-000000000001`; default/global scalar reads and `SHOW VARIABLES` rows are supported; session/local scalar reads return MySQL-style global-variable diagnostics; assignment returns MySQL-style read-only diagnostics; no persisted server UUID, auto.cnf, replication identity, startup option handling, or binary log interaction |
| `session_track_gtids` | ✅ | Fixed default/session/global scalar and `SHOW VARIABLES` value `OFF`; session placeholder `SET` readback and fixed global no-op assignments are supported. No GTID state tracking, protocol session-state packets, startup options, persisted state, or Performance Schema variable tables |
| `session_track_schema` | ✅ | Fixed default/session/global scalar `1` and `SHOW VARIABLES` `ON`; session placeholder `SET` readback and fixed global no-op assignments are supported. No protocol session-state packets, startup options, persisted state, or Performance Schema variable tables |
| `session_track_state_change` | ✅ | Fixed default/session/global scalar `0` and `SHOW VARIABLES` `OFF`; session placeholder `SET` readback and fixed global no-op assignments are supported. No protocol session-state packets, startup options, persisted state, or Performance Schema variable tables |
| `session_track_system_variables` | ❌ | Value, scope, SET, diagnostics |
| `session_track_transaction_info` | ✅ | Fixed default/session/global scalar and `SHOW VARIABLES` value `OFF`; session placeholder `SET` readback and fixed global no-op assignments are supported. No transaction session-state packets, startup options, persisted state, or Performance Schema variable tables |
| `set_operations_buffer_size` | ❌ | Value, scope, SET, diagnostics |
| `sha256_password_auto_generate_rsa_keys` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `sha256_password_private_key_path` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `sha256_password_proxy_users` | ✅ | Fixed global readback/SHOW and exact global no-op SET |
| `sha256_password_public_key_path` | ✅ | Fixed global readback/SHOW and read-only SET diagnostics |
| `shared_memory` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `shared_memory_base_name` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `show_create_table_skip_secondary_engine` | ✅ | Fixed default/session/local scalar `0`, `SHOW VARIABLES` `OFF`, and MySQL-style global-scope diagnostics; session placeholder `SET` readback is supported. No secondary-engine SHOW CREATE rewriting, global value, persisted state, or Performance Schema variable tables |
| `show_create_table_verbosity` | ✅ | Fixed default/session/global scalar `0` and `SHOW VARIABLES` `OFF`; session placeholder `SET` readback and fixed global no-op assignments are supported. No extended SHOW CREATE verbosity changes, startup options, persisted state, or Performance Schema variable tables |
| `show_gipk_in_create_table_and_information_schema` | ❌ | Value, scope, SET, diagnostics |
| `skip_external_locking` | ✅ | Fixed global read-only scalar `1`, `SHOW VARIABLES` `ON`, and MySQL-style scope/SET diagnostics; no external-locking behavior or startup/persisted state |
| `skip_name_resolve` | ✅ | Fixed global read-only scalar `1`, `SHOW VARIABLES` `ON`, and MySQL-style scope/SET diagnostics; no DNS or host-cache behavior |
| `skip_networking` | ✅ | Fixed global read-only scalar `0`, `SHOW VARIABLES` `OFF`, and MySQL-style scope/SET diagnostics; no TCP listener behavior |
| `skip_replica_start` | ❌ | Value, scope, SET, diagnostics |
| `skip_show_database` | ✅ | Fixed global read-only scalar `0`, `SHOW VARIABLES` `OFF`, and MySQL-style scope/SET diagnostics; no privilege-driven `SHOW DATABASES` filtering |
| `skip_slave_start` | ❌ | Value, scope, SET, diagnostics |
| `slave_allow_batching` | ❌ | Value, scope, SET, diagnostics |
| `slave_checkpoint_group` | ❌ | Value, scope, SET, diagnostics |
| `slave_checkpoint_period` | ❌ | Value, scope, SET, diagnostics |
| `slave_compressed_protocol` | ❌ | Value, scope, SET, diagnostics |
| `slave_exec_mode` | ❌ | Value, scope, SET, diagnostics |
| `slave_load_tmpdir` | ❌ | Value, scope, SET, diagnostics |
| `slave_max_allowed_packet` | ❌ | Value, scope, SET, diagnostics |
| `slave_net_timeout` | ❌ | Value, scope, SET, diagnostics |
| `slave_parallel_type` | ❌ | Value, scope, SET, diagnostics |
| `slave_parallel_workers` | ❌ | Value, scope, SET, diagnostics |
| `slave_pending_jobs_size_max` | ❌ | Value, scope, SET, diagnostics |
| `slave_preserve_commit_order` | ❌ | Value, scope, SET, diagnostics |
| `slave_skip_errors` | ❌ | Value, scope, SET, diagnostics |
| `slave_sql_verify_checksum` | ❌ | Value, scope, SET, diagnostics |
| `slave_transaction_retries` | ❌ | Value, scope, SET, diagnostics |
| `slave_type_conversions` | ❌ | Value, scope, SET, diagnostics |
| `slow_launch_time` | ❌ | Value, scope, SET, diagnostics |
| `slow_query_log` | ❌ | Value, scope, SET, diagnostics |
| `slow_query_log_file` | ❌ | Value, scope, SET, diagnostics |
| `socket` | 🟡 | Limited fixed global read-only scalar and `SHOW VARIABLES` placeholder `/var/run/mysqld/mysqld.sock`; default/global scalar reads and `SHOW VARIABLES` rows are supported, session/local scalar reads return MySQL-style global-variable diagnostics, and assignment returns MySQL-style read-only diagnostics. No socket file is created or used |
| `sort_buffer_size` | ❌ | Value, scope, SET, diagnostics |
| `source_verify_checksum` | ❌ | Value, scope, SET, diagnostics |
| `sql_auto_is_null` | ✅ | Session/local/unscoped scalar reads and `SHOW VARIABLES` default to `0`/`OFF`, Boolean session `SET` forms are connection-local, global reads and `SHOW GLOBAL VARIABLES` remain fixed `0`/`OFF`, and supported `AUTO_INCREMENT` `IS NULL` predicates use `LAST_INSERT_ID()` when enabled. No server-global mutation, persisted state, Performance Schema variable tables, or `ISNULL()` special lookup |
| `sql_big_selects` | ✅ | Session/local/unscoped scalar reads and `SHOW VARIABLES` default to `1`/`ON`, Boolean session `SET` forms are connection-local, global reads and `SHOW GLOBAL VARIABLES` remain fixed `1`/`ON`, and descriptor-backed `SELECT` behavior is unchanged. No server-global mutation, persisted state, Performance Schema variable tables, `max_join_size`, optimizer row-estimate aborts, or changed `SELECT` diagnostics |
| `sql_buffer_result` | ✅ | Session/local/unscoped scalar reads and `SHOW VARIABLES` default to `0`/`OFF`, Boolean session `SET` forms are connection-local, global reads and `SHOW GLOBAL VARIABLES` remain fixed `0`/`OFF`, and descriptor-backed `SELECT` behavior is unchanged. No server-global mutation, persisted state, Performance Schema variable tables, physical result buffering, lock-release behavior, or optimizer effects |
| `sql_generate_invisible_primary_key` | 🟡 | Limited scalar `SELECT @@sql_generate_invisible_primary_key` with no scope, `session`, `local`, or `global`; returns MyLite's fixed disabled value `0`; limited fixed no-op `SET` forms may preserve `0`; no mutable global/session state, hidden `my_row_id` columns, generated invisible primary keys, invisible generated-column metadata, changed `CREATE TABLE` behavior, or Performance Schema variable tables |
| `sql_log_bin` | ✅ | Session/local/unscoped scalar reads and `SHOW VARIABLES` default to `1`/`ON`, Boolean session `SET` forms are connection-local, `global` scalar scope is rejected as session-only, and `SHOW GLOBAL VARIABLES` omits the session-only variable. No binary log files, GTID behavior, replication side effects, privilege checks, persisted state, or Performance Schema variable tables |
| `sql_log_off` | ✅ | Session/local/unscoped scalar reads and `SHOW VARIABLES` default to `0`/`OFF`, Boolean session `SET` forms are connection-local, global reads and `SHOW GLOBAL VARIABLES` remain fixed `0`/`OFF`, and statement execution is unchanged. No server-global mutation, persisted state, general query log files, log-row writes to `mysql.general_log`, slow query logging, privilege checks, or Performance Schema variable tables |
| `sql_mode` | 🟡 | Limited scalar `SELECT @@sql_mode` with no scope, `session`, `local`, or `global`; session/local reads return the current connection-local canonical mode string, while global reads return MySQL 8.4.9's default mode string `ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION`; limited `SET sql_mode` forms accept `DEFAULT` or string literals, with behavioral effects for `ANSI_QUOTES`, `NO_BACKSLASH_ESCAPES`, `NO_AUTO_VALUE_ON_ZERO`, `NO_ENGINE_SUBSTITUTION` on current `CREATE TABLE` / `CREATE TEMPORARY TABLE` engine validation, `REAL_AS_FLOAT`, the documented zero-temporal `STRICT_*` / `NO_ZERO_DATE` / `NO_ZERO_IN_DATE` conversion subset, limited ordinary non-strict DML adjustment, limited `UPDATE IGNORE` assignment conversion demotion, and limited `INSERT ... SELECT` selected-row adjustment; no mutable global state, broader changed statement behavior, or Performance Schema variable tables |
| `sql_notes` | ✅ | Session/local/unscoped scalar and expression reads plus `SHOW VARIABLES` default to `1`/`ON`, Boolean session `SET` forms are connection-local, global reads and `SHOW GLOBAL VARIABLES` remain fixed `1`/`ON`, and `sql_notes=0` suppresses current note-level diagnostics before they are counted or retained. Note-level diagnostics are currently produced by limited table/schema-existence DDL no-ops and admitted value-conversion notes. No mutable shared global state, startup/persisted values, Performance Schema variable tables, diagnostics stacks, or complete note-producing surface |
| `sql_quote_show_create` | ✅ | Session/local/unscoped scalar reads and `SHOW VARIABLES` default to `1`/`ON`, Boolean session `SET` forms are connection-local, global reads and `SHOW GLOBAL VARIABLES` remain fixed `1`/`ON`, and structured `SHOW CREATE DATABASE` / base-table `SHOW CREATE TABLE` output honors the session value for simple identifier quoting. No server-global mutation, persisted state, Performance Schema variable tables, or quote-control for pre-rendered SHOW CREATE variants |
| `sql_replica_skip_counter` | 🟡 | Limited read-only scalar `SELECT @@sql_replica_skip_counter` with no scope or `global`; returns MyLite's fixed replica-event-skip counter value `0`; `session` and `local` scopes are rejected as global-only; no `SET`, mutable global state, replica SQL thread state, relay log/event skipping, channels, GTID restrictions, or Performance Schema variable tables; deprecated `sql_slave_skip_counter` alias support is tracked separately |
| `sql_require_primary_key` | 🟡 | Limited scalar `SELECT @@sql_require_primary_key` with no scope, `session`, `local`, or `global`; no-scope/session/local reads return handle-local session state, global reads return the fixed disabled value `0`; limited `SET` forms support no-scope/session/local/`@@` session assignment for boolean/default values, while global assignment is limited to exact disabled no-ops; the session value enforces primary-key presence for current supported `CREATE TABLE`, temporary table, `CREATE TABLE ... LIKE`, CTAS, `CREATE INDEX`, supported single-action ALTER table changes on existing no-primary-key tables except the observed rename/key-maintenance exceptions, single-action `DROP PRIMARY KEY` / quoted-primary `DROP CONSTRAINT`, and supported multi-action ALTER final-state paths. No mutable shared global state, generated invisible primary keys, table import behavior, replication policy, privilege semantics, Performance Schema variable tables, or complete future ALTER enforcement matrix |
| `sql_safe_updates` | ✅ | Session/local/unscoped scalar and expression reads plus `SHOW VARIABLES` default to `0`/`OFF`, Boolean session `SET` forms are connection-local, global reads and `SHOW GLOBAL VARIABLES` remain fixed `0`/`OFF`, and supported single-table `UPDATE`/`DELETE` reject unsafe statements with `1175 / HY000` when the session value is enabled and no `LIMIT` or descriptor key predicate is present. No shared global mutation, persisted state, mysql client `--safe-updates` initialization, `sql_select_limit`, `max_join_size`, Performance Schema variable tables, joined DML enforcement, or exact optimizer access-path equivalence |
| `sql_select_limit` | ✅ | Session/local/unscoped scalar reads and `SHOW VARIABLES` expose handle-local state, global reads remain the fixed no-limit value `18446744073709551615`, and `SET` supports `DEFAULT`, unsigned integer literals, unary `+`/`-` integer literals, booleans, and integer user variables, with negative values clamped to `0` and warning `1292`. Top-level supported `SELECT` statements without explicit `LIMIT` are capped by the session value, including scalar, descriptor-backed, grouped, aggregate, information-schema, and compound result sets. No mutable global state, persisted variables, `SET_VAR`, safe-updates initialization, Performance Schema variable tables, or implicit caps for internal DML/DDL source selects |
| `sql_slave_skip_counter` | 🟡 | Limited read-only scalar `SELECT @@sql_slave_skip_counter` with no scope or `global`; deprecated alias for MyLite's fixed `sql_replica_skip_counter` value `0`; emits MySQL-compatible deprecation warning `1287` once per successful alias reference; `session` and `local` scopes are rejected as global-only; no `SET`, mutable global state, replica SQL thread state, relay log/event skipping, channels, GTID restrictions, or Performance Schema variable tables |
| `sql_warnings` | ✅ | Session readback for unscoped, `session`, and `local` reads; global default remains `0`; Boolean `SET SESSION`, `SET LOCAL`, `SET @@...`, `DEFAULT`, user-variable assignment, `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and expression reads are supported; diagnostics warnings are recorded for documented DML subsets; no global mutation, persisted state, privilege checks, protocol information strings, or Performance Schema variable tables |
| `ssl_ca` | ✅ | Default-state `NULL`/blank global readback/SHOW and `DEFAULT`/`NULL` global no-op SET |
| `ssl_capath` | ✅ | Default-state `NULL`/blank global readback/SHOW and `DEFAULT`/`NULL` global no-op SET |
| `ssl_cert` | ✅ | Default-state `NULL`/blank global readback/SHOW and `DEFAULT`/`NULL` global no-op SET |
| `ssl_cipher` | ✅ | Default-state `NULL`/blank global readback/SHOW and `DEFAULT`/`NULL` global no-op SET |
| `ssl_crl` | ✅ | Default-state `NULL`/blank global readback/SHOW and `DEFAULT`/`NULL` global no-op SET |
| `ssl_crlpath` | ✅ | Default-state `NULL`/blank global readback/SHOW and `DEFAULT`/`NULL` global no-op SET |
| `ssl_fips_mode` | ✅ | Fixed global read-only scalar and `SHOW VARIABLES` value `OFF`; no FIPS/TLS runtime behavior |
| `ssl_key` | ✅ | Default-state `NULL`/blank global readback/SHOW and `DEFAULT`/`NULL` global no-op SET |
| `ssl_session_cache_mode` | ✅ | Fixed global scalar `1`, `SHOW VARIABLES` `ON`, global-only diagnostics, and exact no-op global `SET`; no SSL session cache |
| `ssl_session_cache_timeout` | ✅ | Fixed global scalar and `SHOW VARIABLES` value `300`, global-only diagnostics, and exact no-op global `SET`; no SSL session cache |
| `statement_id` | ❌ | Value, scope, SET, diagnostics |
| `stored_program_cache` | ❌ | Value, scope, SET, diagnostics |
| `stored_program_definition_cache` | ❌ | Value, scope, SET, diagnostics |
| `super_read_only` | 🟡 | Limited fixed global disabled placeholder: scalar default/global reads return `0`, `SHOW VARIABLES` rows report `OFF`, session/local scalar reads and non-global `SET` forms use MySQL-shaped global-variable diagnostics, and exact no-op global `SET` forms may preserve `OFF`; no mutable global state, write blocking, privileges, startup option, or implication of `read_only` |
| `sync_binlog` | ❌ | Value, scope, SET, diagnostics |
| `sync_master_info` | ❌ | Value, scope, SET, diagnostics |
| `sync_relay_log` | ❌ | Value, scope, SET, diagnostics |
| `sync_relay_log_info` | ❌ | Value, scope, SET, diagnostics |
| `sync_source_info` | ❌ | Value, scope, SET, diagnostics |
| `syseventlog.facility` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `syseventlog.include_pid` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `syseventlog.tag` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `system_time_zone` | 🟡 | Limited fixed global-only readback value `UTC` through scalar reads and `SHOW VARIABLES`; no host-local time zone discovery, loaded named time-zone rows, or mutable state |
| `table_definition_cache` | ❌ | Value, scope, SET, diagnostics |
| `table_encryption_privilege_check` | ❌ | Value, scope, SET, diagnostics |
| `table_open_cache` | ❌ | Value, scope, SET, diagnostics |
| `table_open_cache_instances` | ❌ | Value, scope, SET, diagnostics |
| `tablespace_definition_cache` | ❌ | Value, scope, SET, diagnostics |
| `telemetry.metrics_enabled` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.metrics_reader_frequency_1` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.metrics_reader_frequency_2` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.metrics_reader_frequency_3` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_bsp_max_export_batch_size` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_bsp_max_queue_size` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_bsp_schedule_delay` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_metrics_certificates` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_metrics_cipher` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_metrics_cipher_suite` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_metrics_client_certificates` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_metrics_client_key` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_metrics_compression` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_metrics_endpoint` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_metrics_headers` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_metrics_max_tls` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_metrics_min_tls` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_metrics_protocol` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_metrics_timeout` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_traces_certificates` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_traces_cipher` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_traces_cipher_suite` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_traces_client_certificates` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_traces_client_key` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_traces_compression` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_traces_endpoint` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_traces_headers` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_traces_max_tls` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_traces_min_tls` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_traces_protocol` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_exporter_otlp_traces_timeout` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_log_level` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.otel_resource_attributes` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.query_text_enabled` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `telemetry.trace_enabled` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `temptable_max_mmap` | ❌ | Value, scope, SET, diagnostics |
| `temptable_max_ram` | ❌ | Value, scope, SET, diagnostics |
| `temptable_use_mmap` | ❌ | Value, scope, SET, diagnostics |
| `terminology_use_previous` | ❌ | Value, scope, SET, diagnostics |
| `thread_cache_size` | ❌ | Value, scope, SET, diagnostics |
| `thread_handling` | ❌ | Value, scope, SET, diagnostics |
| `thread_pool_algorithm` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `thread_pool_dedicated_listeners` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `thread_pool_high_priority_connection` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `thread_pool_longrun_trx_limit` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `thread_pool_max_active_query_threads` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `thread_pool_max_transactions_limit` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `thread_pool_max_unused_threads` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `thread_pool_prio_kickup_timer` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `thread_pool_query_threads_per_group` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `thread_pool_size` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `thread_pool_stall_limit` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `thread_pool_transaction_delay` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `thread_stack` | ❌ | Value, scope, SET, diagnostics |
| `time_zone` | 🟡 | Limited fixed global value `SYSTEM`, session-local scalar reads and `SHOW VARIABLES`, and session/local `SET time_zone` forms for `DEFAULT`, `SYSTEM`, `UTC`, and signed UTC offsets. Current date/time/timestamp materialization uses the session offset; `mysql.time_zone*` tables are metadata-only empty placeholders with no mutable global value, loaded named-zone rows, named-zone conversion beyond `UTC`, daylight-saving behavior, leap-second handling, or TIMESTAMP row storage/retrieval conversion |
| `timestamp` | 🟡 | Limited session-only `@@timestamp`, `@@SESSION.timestamp`, `SHOW VARIABLES LIKE 'timestamp'`, and `SET timestamp` / `SET SESSION timestamp` / `SET @@SESSION.timestamp` for integer, signed integer reset, `DEFAULT`, and `@@timestamp` plus integer arithmetic inside `SET timestamp`. The value controls the current statement date/time/timestamp used by `CURDATE()` / `CURRENT_DATE`, `CURTIME()` / `CURRENT_TIME`, `NOW()` / `CURRENT_TIMESTAMP`, and automatic temporal defaults, with materialization adjusted by the limited session `time_zone`; no global scope, nonzero fractional assignment values, general system-variable scalar arithmetic, TIMESTAMP row storage/retrieval conversion, persisted state, or broader temporal-variable behavior |
| `tls_certificates_enforced_validation` | ✅ | Fixed global read-only scalar `0`, `SHOW VARIABLES` `OFF`, and MySQL-style scope/SET diagnostics; no TLS certificate validation behavior |
| `tls_ciphersuites` | ✅ | Default-state `NULL`/blank global readback/SHOW and `DEFAULT`/`NULL` global no-op SET |
| `tls_version` | ✅ | Fixed global scalar and `SHOW VARIABLES` value `TLSv1.2,TLSv1.3`, global-only diagnostics, and exact no-op global `SET`; no TLS negotiation |
| `tmp_table_size` | ❌ | Value, scope, SET, diagnostics |
| `tmpdir` | ✅ | Fixed global read-only scalar and `SHOW VARIABLES` value `/tmp`; no temp-file routing, startup options, persisted state, or Performance Schema variable tables |
| `transaction_alloc_block_size` | ❌ | Value, scope, SET, diagnostics |
| `transaction_allow_batching` | ❌ | Value, scope, SET, diagnostics |
| `transaction_isolation` | 🟡 | Limited scalar reads and `SHOW VARIABLES` rows expose fixed global `REPEATABLE-READ` and connection-local session/default/local values. Direct session/local assignments update the session default; direct `SET @@transaction_isolation = ...` updates the next transaction characteristic; exact fixed-global no-op assignments may preserve the default. No mutable global default, privilege checks, persisted variables, or added MVCC/isolation behavior |
| `transaction_prealloc_size` | ❌ | Value, scope, SET, diagnostics |
| `transaction_read_only` | 🟡 | Limited scalar reads and `SHOW VARIABLES` rows expose fixed global `0`/`OFF` and connection-local session/default/local values. Direct session/local assignments update the session default; direct `SET @@transaction_read_only = ...` updates the next transaction characteristic and can make the next persistent write fail read-only; exact fixed-global no-op assignments may preserve the default. No mutable global default, privilege checks, persisted variables, or protocol read-only status flags |
| `unique_checks` | ✅ | Session/local/unscoped scalar and expression reads plus `SHOW VARIABLES` default to `1`/`ON`, Boolean session `SET` forms are connection-local, global reads and `SHOW GLOBAL VARIABLES` remain fixed `1`/`ON`, and descriptor-owned primary-key and supported unique-index duplicate checks remain enabled. No server-global mutation, persisted state, import optimizations, optimizer effects, privileges, or Performance Schema variable tables |
| `updatable_views_with_limit` | ✅ | Session/local/unscoped scalar reads and `SHOW VARIABLES` default to `YES`, Boolean session `SET` forms are connection-local and read back as `YES`/`NO`, global reads and `SHOW GLOBAL VARIABLES` remain fixed `YES`, and embedded statement execution is unchanged. No view DML side effects, server-global mutation, persisted state, check-option enforcement, privileges, or Performance Schema variable tables |
| `use_secondary_engine` | ✅ | Fixed default/session/local scalar and `SHOW VARIABLES` value `ON`, plus MySQL-style global-scope diagnostics; session placeholder `SET` readback is supported. No HeatWave or secondary-engine routing, global value, persisted state, or Performance Schema variable tables |
| `validate_password_check_user_name` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `validate_password_dictionary_file` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `validate_password_length` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `validate_password_mixed_case_count` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `validate_password_number_count` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `validate_password_policy` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `validate_password_special_char_count` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `validate_password.changed_characters_percentage` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `validate_password.check_user_name` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `validate_password.dictionary_file` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `validate_password.length` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `validate_password.mixed_case_count` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `validate_password.number_count` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `validate_password.policy` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `validate_password.special_char_count` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `version` | ✅ | MySQL-runtime-verified read-only scalar `SELECT @@version` with no scope or `global`, plus matching `SHOW VARIABLES` rows, returns the fixed MySQL 8.4.9 compatibility version string and rejects unsupported scopes with the documented MySQL-shaped diagnostics; assignment behavior, protocol handshake version reporting, and configurable server-version identity are tracked outside this variable row |
| `version_comment` | ✅ | MySQL-runtime-verified read-only scalar `SELECT @@version_comment` with no scope or `global`, plus matching `SHOW VARIABLES` rows, returns the fixed MySQL 8.4.9 community-server comment and rejects unsupported scopes with the documented MySQL-shaped diagnostics; assignment behavior, protocol metadata, and configurable build comments are tracked outside this variable row |
| `version_compile_machine` | 🟡 | Limited fixed global read-only scalar and `SHOW VARIABLES` compatibility placeholder `aarch64`; default/global scalar reads and `SHOW VARIABLES` rows are supported, session/local scalar reads return MySQL-style global-variable diagnostics, and assignment returns MySQL-style read-only diagnostics. No host build-machine introspection or startup option behavior |
| `version_compile_os` | 🟡 | Limited fixed global read-only scalar and `SHOW VARIABLES` compatibility placeholder `Linux`; default/global scalar reads and `SHOW VARIABLES` rows are supported, session/local scalar reads return MySQL-style global-variable diagnostics, and assignment returns MySQL-style read-only diagnostics. No host operating-system introspection or startup option behavior |
| `version_compile_zlib` | 🟡 | Limited fixed global read-only scalar and `SHOW VARIABLES` compatibility placeholder `1.3.2`; default/global scalar reads and `SHOW VARIABLES` rows are supported, session/local scalar reads return MySQL-style global-variable diagnostics, and assignment returns MySQL-style read-only diagnostics. No linked zlib version introspection or startup option behavior |
| `version_tokens_session` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `version_tokens_session_number` | ⚪ | Target-runtime optional absence; no `SHOW VARIABLES` rows; scalar `1193/HY000` |
| `wait_timeout` | 🟡 | Limited handle-local session scalar reads, `SHOW VARIABLES` rows, and session/local/unqualified `SET` assignment with MySQL-compatible integer range `1..31536000`, `DEFAULT = 28800`, boolean conversion, clamp warnings, and integer user-variable assignment. Global reads expose fixed `28800` and mutable global assignment is limited to exact no-op `DEFAULT`/`28800` forms; no idle timeout enforcement, protocol behavior, startup options, persisted state, privileges, or Performance Schema rows |
| `warning_count` | ✅ | Read-only session/local/unscoped scalar reads and simple expression reads over the previous diagnostics snapshot are MySQL-runtime verified; counts current warning, note, and error conditions even when `@@max_error_count` caps retained rows, honors `sql_notes=0`, and clears after nondiagnostic `SELECT`. Global scope and `SET` are rejected as in MySQL; no diagnostics stacks or broader warning-producer coverage |
| `windowing_use_high_precision` | ❌ | Value, scope, SET, diagnostics |
| `xa_detach_on_prepare` | ❌ | Value, scope, SET, diagnostics |

[Back to compatibility overview](../../COMPATIBILITY.md)
