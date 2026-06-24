# Baseline SHOW STATUS

## Summary

This phase adds a MyLite-owned `SHOW STATUS` result surface:

```sql
SHOW [GLOBAL | SESSION | LOCAL] STATUS [LIKE 'pattern' | WHERE predicate]
```

The statement exposes a limited registry of status rows with embedded MyLite
values, including the MySQL 8.4.9 `Com_%` command-counter name surface. It is
intended for clients that probe basic MySQL server state, command-counter
presence, or feature availability. It does not implement MySQL's complete
status-variable catalog, live status counters, Performance Schema status
tables, privilege checks, `mysqladmin extended-status`, `FLUSH STATUS`, or
arbitrary `WHERE` expression evaluation.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - `SHOW STATUS`: <https://dev.mysql.com/doc/refman/8.4/en/show-status.html>
  - server status variable reference:
    <https://dev.mysql.com/doc/refman/8.4/en/server-status-variable-reference.html>
- Observed MySQL 8.4.9 runtime behavior from the local `mylite-mysql-849`
  container, captured by
  `packages/libmylite/tests/mysql_baseline_show_status_expectations.sh`.

Runtime probes against MySQL 8.4.9 establish these expectations for the
admitted subset:

- result columns are `Variable_name` and `Value`;
- `SHOW STATUS`, `SHOW SESSION STATUS`, and `SHOW LOCAL STATUS` use session
  scope; `LOCAL` is a session synonym;
- `SHOW GLOBAL STATUS` uses global scope;
- session-scope status includes global-only rows such as `Connections`;
- global-scope status omits session-only rows such as `Compression`;
- `LIKE` filters match status variable names case-insensitively using `%`, `_`,
  and backslash escapes;
- `SHOW STATUS LIKE 'Threads\_%'` returns the thread rows whose names match the
  pattern;
- `SHOW STATUS LIKE 'Com\_%'` returns the 168 MySQL 8.4.9 command-counter
  names in the target runtime order for default, session, and global scopes;
- `SHOW STATUS WHERE ...` admits the same limited output-row predicate subset
  as `SHOW VARIABLES WHERE` over `Variable_name` and `Value`;
- `SHOW STATUS LIKE 'Threads%' WHERE ...`, `SHOW STATUS ORDER BY ...`,
  `SHOW STATUS LIMIT ...`, `SHOW FULL STATUS`, and non-string `LIKE` operands
  are syntax errors;
- successful supported statements leave warning count `0`, error count `0`,
  and make `ROW_COUNT()` return `-1`.

The exact live values of MySQL counters vary by server startup and connection
history. MyLite therefore verifies MySQL shape, scope, and filtering behavior
against MySQL 8.4.9, while MyLite's own C tests assert the deterministic
embedded values documented below.

## Ownership Boundaries

- Public API: no ABI or public-header change. `mylite_execute()` returns a
  normal row result through the existing result handle.
- Statement context: successful `SHOW STATUS` is a result-producing statement.
  It reports affected rows `0`, warning count `0`, no stored warnings, and
  updates the previous row count to `-1`.
- Lexer/parser/AST: add a `SHOW STATUS` statement node with an optional scope
  token and either an optional string-only `LIKE` clause or a `WHERE` predicate
  node. Reuse the existing identifier, string-literal, and predicate AST
  helpers.
- Runtime/analyzer: resolve the optional scope and iterate a static
  MyLite-owned status registry. Apply scope, `LIKE`, and the limited output-row
  predicate while appending rows.
- Catalog: not involved. Status variables are runtime/embedded metadata, not
  schema or table descriptors.
- Result builder: build rows directly through the existing result API.
- Storage/VFS/file format: no file reads beyond normal handle state, no writes,
  no catalog mutation, no SQLite schema generation change, and no `.mylite`
  preamble change.
- SQLite physical storage: no generated SQLite SQL and no SQLite fork patch.

## Syntax

The independent MyLite subset is:

```ebnf
show_status_statement:
    SHOW show_status_scope_opt STATUS show_status_filter_opt

show_status_scope_opt:
    empty
  | GLOBAL
  | SESSION
  | LOCAL

show_status_filter_opt:
    empty
  | LIKE string_literal
  | WHERE show_status_predicate
```

`LIKE` and `WHERE` are mutually exclusive. The grammar intentionally excludes
`FULL`, `ORDER BY`, `LIMIT`, schema qualifiers, parameters, non-string `LIKE`
operands, and combined `LIKE ... WHERE` filters. `LOCAL` is admitted as a
session-scope synonym because MySQL 8.4.9 accepts it for this statement.

### MyLite Lemon-Syntax Snippet

```lemon
statement(A) ::= show_status_statement(B). {
    A = B;
}

show_status_statement(A) ::=
    SHOW(S) show_status_scope_opt(O) STATUS(T) show_status_filter_opt(F). {
    A = mylite_sql_parser_make_show_status_statement(state, S, O, T, F);
}

show_status_scope_opt(A) ::= . {
    A = NULL;
}
show_status_scope_opt(A) ::= GLOBAL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
show_status_scope_opt(A) ::= SESSION(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
show_status_scope_opt(A) ::= LOCAL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

show_status_filter_opt(A) ::= . {
    A = NULL;
}
show_status_filter_opt(A) ::= LIKE STRING(P). {
    A = mylite_sql_parser_make_literal(state, P, MYLITE_SQL_AST_LITERAL_STRING);
}
show_status_filter_opt(A) ::= WHERE(W) predicate(P). {
    A = mylite_sql_parser_make_where_clause(state, W, P);
}
```

These snippets are independently authored for MyLite's admitted subset and are
not MySQL's full grammar.

## Status Registry

This phase exposes a limited common registry. Values are deterministic embedded
placeholders unless listed otherwise. All numeric values are returned as
decimal text. The core lifecycle rows (`Bytes_%`, `Connections`, `Compression`,
`Prepared_stmt_count`, `Queries`, `Questions`, `Threads_%`, and `Uptime%`) and
the `Binlog_%`, `Com_%`, `Handler_%`, `Key_%`, `Last_query_%`, `Max_%`,
`Select_%`, `Slow_%`, `Sort_%`, `Ssl_%`, `Table_locks_%`,
`Table_open_cache_%`, `Tc_log_%`, and `Telemetry_%` rows mirror the MySQL 8.4.9
row names and scope visibility for the documented placeholder subset. The
`Created_%`, `Delayed_%`, `Open%`, and other documented server/legacy scalar
rows also mirror the MySQL 8.4.9 names and order. These values are fixed
placeholders rather than live counters.

| Variable | Default/session/LOCAL visibility | GLOBAL visibility | MyLite value |
| --- | --- | --- | --- |
| `Aborted_clients` | yes | yes | `0` |
| `Aborted_connects` | yes | yes | `0` |
| `Acl_cache_items_count` | yes | yes | `0` |
| `Binlog_cache_disk_use` | yes | yes | `0` |
| `Binlog_cache_use` | yes | yes | `0` |
| `Binlog_stmt_cache_disk_use` | yes | yes | `0` |
| `Binlog_stmt_cache_use` | yes | yes | `0` |
| `Bytes_received` | yes | yes | `0` |
| `Bytes_sent` | yes | yes | `0` |
| `Com_admin_commands` | yes | yes | `0` |
| `Com_assign_to_keycache` | yes | yes | `0` |
| `Com_alter_db` | yes | yes | `0` |
| `Com_alter_event` | yes | yes | `0` |
| `Com_alter_function` | yes | yes | `0` |
| `Com_alter_instance` | yes | yes | `0` |
| `Com_alter_procedure` | yes | yes | `0` |
| `Com_alter_resource_group` | yes | yes | `0` |
| `Com_alter_server` | yes | yes | `0` |
| `Com_alter_table` | yes | yes | `0` |
| `Com_alter_tablespace` | yes | yes | `0` |
| `Com_alter_user` | yes | yes | `0` |
| `Com_alter_user_default_role` | yes | yes | `0` |
| `Com_analyze` | yes | yes | `0` |
| `Com_begin` | yes | yes | `0` |
| `Com_binlog` | yes | yes | `0` |
| `Com_call_procedure` | yes | yes | `0` |
| `Com_change_db` | yes | yes | `0` |
| `Com_change_repl_filter` | yes | yes | `0` |
| `Com_change_replication_source` | yes | yes | `0` |
| `Com_check` | yes | yes | `0` |
| `Com_checksum` | yes | yes | `0` |
| `Com_clone` | yes | yes | `0` |
| `Com_commit` | yes | yes | `0` |
| `Com_create_db` | yes | yes | `0` |
| `Com_create_event` | yes | yes | `0` |
| `Com_create_function` | yes | yes | `0` |
| `Com_create_index` | yes | yes | `0` |
| `Com_create_procedure` | yes | yes | `0` |
| `Com_create_role` | yes | yes | `0` |
| `Com_create_server` | yes | yes | `0` |
| `Com_create_table` | yes | yes | `0` |
| `Com_create_resource_group` | yes | yes | `0` |
| `Com_create_trigger` | yes | yes | `0` |
| `Com_create_udf` | yes | yes | `0` |
| `Com_create_user` | yes | yes | `0` |
| `Com_create_view` | yes | yes | `0` |
| `Com_create_spatial_reference_system` | yes | yes | `0` |
| `Com_dealloc_sql` | yes | yes | `0` |
| `Com_delete` | yes | yes | `0` |
| `Com_delete_multi` | yes | yes | `0` |
| `Com_do` | yes | yes | `0` |
| `Com_drop_db` | yes | yes | `0` |
| `Com_drop_event` | yes | yes | `0` |
| `Com_drop_function` | yes | yes | `0` |
| `Com_drop_index` | yes | yes | `0` |
| `Com_drop_procedure` | yes | yes | `0` |
| `Com_drop_resource_group` | yes | yes | `0` |
| `Com_drop_role` | yes | yes | `0` |
| `Com_drop_server` | yes | yes | `0` |
| `Com_drop_spatial_reference_system` | yes | yes | `0` |
| `Com_drop_table` | yes | yes | `0` |
| `Com_drop_trigger` | yes | yes | `0` |
| `Com_drop_user` | yes | yes | `0` |
| `Com_drop_view` | yes | yes | `0` |
| `Com_empty_query` | yes | yes | `0` |
| `Com_execute_sql` | yes | yes | `0` |
| `Com_explain_other` | yes | yes | `0` |
| `Com_flush` | yes | yes | `0` |
| `Com_get_diagnostics` | yes | yes | `0` |
| `Com_grant` | yes | yes | `0` |
| `Com_grant_roles` | yes | yes | `0` |
| `Com_ha_close` | yes | yes | `0` |
| `Com_ha_open` | yes | yes | `0` |
| `Com_ha_read` | yes | yes | `0` |
| `Com_help` | yes | yes | `0` |
| `Com_import` | yes | yes | `0` |
| `Com_insert` | yes | yes | `0` |
| `Com_insert_select` | yes | yes | `0` |
| `Com_install_component` | yes | yes | `0` |
| `Com_install_plugin` | yes | yes | `0` |
| `Com_kill` | yes | yes | `0` |
| `Com_load` | yes | yes | `0` |
| `Com_lock_instance` | yes | yes | `0` |
| `Com_lock_tables` | yes | yes | `0` |
| `Com_optimize` | yes | yes | `0` |
| `Com_preload_keys` | yes | yes | `0` |
| `Com_prepare_sql` | yes | yes | `0` |
| `Com_purge` | yes | yes | `0` |
| `Com_purge_before_date` | yes | yes | `0` |
| `Com_release_savepoint` | yes | yes | `0` |
| `Com_rename_table` | yes | yes | `0` |
| `Com_rename_user` | yes | yes | `0` |
| `Com_repair` | yes | yes | `0` |
| `Com_replace` | yes | yes | `0` |
| `Com_replace_select` | yes | yes | `0` |
| `Com_reset` | yes | yes | `0` |
| `Com_resignal` | yes | yes | `0` |
| `Com_restart` | yes | yes | `0` |
| `Com_revoke` | yes | yes | `0` |
| `Com_revoke_all` | yes | yes | `0` |
| `Com_revoke_roles` | yes | yes | `0` |
| `Com_rollback` | yes | yes | `0` |
| `Com_rollback_to_savepoint` | yes | yes | `0` |
| `Com_savepoint` | yes | yes | `0` |
| `Com_select` | yes | yes | `0` |
| `Com_set_option` | yes | yes | `0` |
| `Com_set_password` | yes | yes | `0` |
| `Com_set_resource_group` | yes | yes | `0` |
| `Com_set_role` | yes | yes | `0` |
| `Com_signal` | yes | yes | `0` |
| `Com_show_binlog_events` | yes | yes | `0` |
| `Com_show_binlogs` | yes | yes | `0` |
| `Com_show_charsets` | yes | yes | `0` |
| `Com_show_collations` | yes | yes | `0` |
| `Com_show_create_db` | yes | yes | `0` |
| `Com_show_create_event` | yes | yes | `0` |
| `Com_show_create_func` | yes | yes | `0` |
| `Com_show_create_proc` | yes | yes | `0` |
| `Com_show_create_table` | yes | yes | `0` |
| `Com_show_create_trigger` | yes | yes | `0` |
| `Com_show_databases` | yes | yes | `0` |
| `Com_show_engine_logs` | yes | yes | `0` |
| `Com_show_engine_mutex` | yes | yes | `0` |
| `Com_show_engine_status` | yes | yes | `0` |
| `Com_show_events` | yes | yes | `0` |
| `Com_show_errors` | yes | yes | `0` |
| `Com_show_fields` | yes | yes | `0` |
| `Com_show_function_code` | yes | yes | `0` |
| `Com_show_function_status` | yes | yes | `0` |
| `Com_show_grants` | yes | yes | `0` |
| `Com_show_keys` | yes | yes | `0` |
| `Com_show_binary_log_status` | yes | yes | `0` |
| `Com_show_open_tables` | yes | yes | `0` |
| `Com_show_parse_tree` | yes | yes | `0` |
| `Com_show_plugins` | yes | yes | `0` |
| `Com_show_privileges` | yes | yes | `0` |
| `Com_show_procedure_code` | yes | yes | `0` |
| `Com_show_procedure_status` | yes | yes | `0` |
| `Com_show_processlist` | yes | yes | `0` |
| `Com_show_profile` | yes | yes | `0` |
| `Com_show_profiles` | yes | yes | `0` |
| `Com_show_relaylog_events` | yes | yes | `0` |
| `Com_show_replicas` | yes | yes | `0` |
| `Com_show_replica_status` | yes | yes | `0` |
| `Com_show_status` | yes | yes | `0` |
| `Com_show_storage_engines` | yes | yes | `0` |
| `Com_show_table_status` | yes | yes | `0` |
| `Com_show_tables` | yes | yes | `0` |
| `Com_show_triggers` | yes | yes | `0` |
| `Com_show_variables` | yes | yes | `0` |
| `Com_show_warnings` | yes | yes | `0` |
| `Com_show_create_user` | yes | yes | `0` |
| `Com_shutdown` | yes | yes | `0` |
| `Com_replica_start` | yes | yes | `0` |
| `Com_replica_stop` | yes | yes | `0` |
| `Com_group_replication_start` | yes | yes | `0` |
| `Com_group_replication_stop` | yes | yes | `0` |
| `Com_stmt_execute` | yes | yes | `0` |
| `Com_stmt_close` | yes | yes | `0` |
| `Com_stmt_fetch` | yes | yes | `0` |
| `Com_stmt_prepare` | yes | yes | `0` |
| `Com_stmt_reset` | yes | yes | `0` |
| `Com_stmt_send_long_data` | yes | yes | `0` |
| `Com_truncate` | yes | yes | `0` |
| `Com_uninstall_component` | yes | yes | `0` |
| `Com_uninstall_plugin` | yes | yes | `0` |
| `Com_unlock_instance` | yes | yes | `0` |
| `Com_unlock_tables` | yes | yes | `0` |
| `Com_update` | yes | yes | `0` |
| `Com_update_multi` | yes | yes | `0` |
| `Com_xa_commit` | yes | yes | `0` |
| `Com_xa_end` | yes | yes | `0` |
| `Com_xa_prepare` | yes | yes | `0` |
| `Com_xa_recover` | yes | yes | `0` |
| `Com_xa_rollback` | yes | yes | `0` |
| `Com_xa_start` | yes | yes | `0` |
| `Com_stmt_reprepare` | yes | yes | `0` |
| `Compression` | yes | no | `OFF` |
| `Connection_control_delay_generated` | yes | yes | `0` |
| `Connection_control_exempted_unknown_users` | yes | yes | `0` |
| `Connection_errors_accept` | yes | yes | `0` |
| `Connection_errors_internal` | yes | yes | `0` |
| `Connection_errors_max_connections` | yes | yes | `0` |
| `Connection_errors_peer_address` | yes | yes | `0` |
| `Connection_errors_select` | yes | yes | `0` |
| `Connection_errors_tcpwrap` | yes | yes | `0` |
| `Connections` | yes | yes | `1` |
| `Created_tmp_disk_tables` | yes | yes | `0` |
| `Created_tmp_files` | yes | yes | `0` |
| `Created_tmp_tables` | yes | yes | `0` |
| `Delayed_errors` | yes | yes | `0` |
| `Delayed_insert_threads` | yes | yes | `0` |
| `Delayed_writes` | yes | yes | `0` |
| `Flush_commands` | yes | yes | `0` |
| `Global_connection_memory` | yes | yes | `0` |
| `Handler_commit` | yes | yes | `0` |
| `Handler_delete` | yes | yes | `0` |
| `Handler_discover` | yes | yes | `0` |
| `Handler_external_lock` | yes | yes | `0` |
| `Handler_mrr_init` | yes | yes | `0` |
| `Handler_prepare` | yes | yes | `0` |
| `Handler_read_first` | yes | yes | `0` |
| `Handler_read_key` | yes | yes | `0` |
| `Handler_read_last` | yes | yes | `0` |
| `Handler_read_next` | yes | yes | `0` |
| `Handler_read_prev` | yes | yes | `0` |
| `Handler_read_rnd` | yes | yes | `0` |
| `Handler_read_rnd_next` | yes | yes | `0` |
| `Handler_rollback` | yes | yes | `0` |
| `Handler_savepoint` | yes | yes | `0` |
| `Handler_savepoint_rollback` | yes | yes | `0` |
| `Handler_update` | yes | yes | `0` |
| `Handler_write` | yes | yes | `0` |
| `Key_blocks_not_flushed` | yes | yes | `0` |
| `Key_blocks_unused` | yes | yes | `0` |
| `Key_blocks_used` | yes | yes | `0` |
| `Key_read_requests` | yes | yes | `0` |
| `Key_reads` | yes | yes | `0` |
| `Key_write_requests` | yes | yes | `0` |
| `Key_writes` | yes | yes | `0` |
| `Last_query_cost` | yes | yes | `0.000000` |
| `Last_query_partial_plans` | yes | yes | `0` |
| `Locked_connects` | yes | yes | `0` |
| `Max_execution_time_exceeded` | yes | yes | `0` |
| `Max_execution_time_set` | yes | yes | `0` |
| `Max_execution_time_set_failed` | yes | yes | `0` |
| `Max_used_connections` | yes | yes | `1` |
| `Max_used_connections_time` | yes | yes | `1970-01-01 00:00:00` |
| `Not_flushed_delayed_rows` | yes | yes | `0` |
| `Ongoing_anonymous_transaction_count` | yes | yes | `0` |
| `Open_files` | yes | yes | `0` |
| `Open_streams` | yes | yes | `0` |
| `Open_table_definitions` | yes | yes | `0` |
| `Open_tables` | yes | yes | `0` |
| `Opened_files` | yes | yes | `0` |
| `Opened_table_definitions` | yes | yes | `0` |
| `Opened_tables` | yes | yes | `0` |
| `Prepared_stmt_count` | yes | yes | `0` |
| `Queries` | yes | yes | `0` |
| `Questions` | yes | yes | `0` |
| `Replica_open_temp_tables` | yes | yes | `0` |
| `Resource_group_supported` | yes | yes | `OFF` |
| `Secondary_engine_execution_count` | yes | yes | `0` |
| `Select_full_join` | yes | yes | `0` |
| `Select_full_range_join` | yes | yes | `0` |
| `Select_range` | yes | yes | `0` |
| `Select_range_check` | yes | yes | `0` |
| `Select_scan` | yes | yes | `0` |
| `Slave_open_temp_tables` | yes | yes | `0` |
| `Slow_launch_threads` | yes | yes | `0` |
| `Slow_queries` | yes | yes | `0` |
| `Sort_merge_passes` | yes | yes | `0` |
| `Sort_range` | yes | yes | `0` |
| `Sort_rows` | yes | yes | `0` |
| `Sort_scan` | yes | yes | `0` |
| `Ssl_accept_renegotiates` | yes | yes | `0` |
| `Ssl_accepts` | yes | yes | `0` |
| `Ssl_callback_cache_hits` | yes | yes | `0` |
| `Ssl_cipher` | yes | yes | empty string |
| `Ssl_cipher_list` | yes | yes | empty string |
| `Ssl_client_connects` | yes | yes | `0` |
| `Ssl_connect_renegotiates` | yes | yes | `0` |
| `Ssl_ctx_verify_depth` | yes | yes | `0` |
| `Ssl_ctx_verify_mode` | yes | yes | `0` |
| `Ssl_default_timeout` | yes | yes | `0` |
| `Ssl_finished_accepts` | yes | yes | `0` |
| `Ssl_finished_connects` | yes | yes | `0` |
| `Ssl_server_not_after` | yes | yes | empty string |
| `Ssl_server_not_before` | yes | yes | empty string |
| `Ssl_session_cache_hits` | yes | yes | `0` |
| `Ssl_session_cache_misses` | yes | yes | `0` |
| `Ssl_session_cache_mode` | yes | yes | empty string |
| `Ssl_session_cache_overflows` | yes | yes | `0` |
| `Ssl_session_cache_size` | yes | yes | `0` |
| `Ssl_session_cache_timeout` | yes | yes | `0` |
| `Ssl_session_cache_timeouts` | yes | yes | `0` |
| `Ssl_sessions_reused` | yes | yes | `0` |
| `Ssl_used_session_cache_entries` | yes | yes | `0` |
| `Ssl_verify_depth` | yes | yes | `0` |
| `Ssl_verify_mode` | yes | yes | `0` |
| `Ssl_version` | yes | yes | empty string |
| `Table_locks_immediate` | yes | yes | `0` |
| `Table_locks_waited` | yes | yes | `0` |
| `Table_open_cache_hits` | yes | yes | `0` |
| `Table_open_cache_misses` | yes | yes | `0` |
| `Table_open_cache_overflows` | yes | yes | `0` |
| `Tc_log_max_pages_used` | yes | yes | `0` |
| `Tc_log_page_size` | yes | yes | `0` |
| `Tc_log_page_waits` | yes | yes | `0` |
| `Telemetry_logs_supported` | yes | yes | `OFF` |
| `Telemetry_metrics_supported` | yes | yes | `OFF` |
| `Telemetry_traces_supported` | yes | yes | `OFF` |
| `Threads_cached` | yes | yes | `0` |
| `Threads_connected` | yes | yes | `1` |
| `Threads_created` | yes | yes | `1` |
| `Threads_running` | yes | yes | `1` |
| `Uptime` | yes | yes | `0` |
| `Uptime_since_flush_status` | yes | yes | `0` |

The registry is intentionally not complete. Missing rows are omitted rather
than synthesized on demand. This mirrors MySQL's result behavior for unknown or
unavailable status names under `LIKE`, while documenting MyLite's partial
catalog.

## Scope Semantics

No explicit scope, `SESSION`, and `LOCAL` use session-visible status rows.
`GLOBAL` uses global-visible rows. The current registry marks all rows as
session-visible because MySQL exposes global-only rows through session-scope
status; `Compression` is the only session-only row in this slice and is omitted
from `SHOW GLOBAL STATUS`.

The `LIKE` filter is applied after scope filtering.

## LIKE Semantics

The optional `LIKE` operand must be an ordinary SQL string literal. Matching is
against `Variable_name` only. Matching is ASCII case-insensitive; `%` matches
any byte sequence; `_` matches one byte; a backslash escapes the next pattern
byte. This reuses the existing MyLite SHOW-pattern matcher.

## WHERE Predicate Semantics

The optional `WHERE` predicate is evaluated after scope filtering and against
the visible output row. The admitted output columns are:

- `Variable_name`
- `Value`

The predicate subset matches the baseline `SHOW VARIABLES WHERE` evaluator:
string and `NULL` literals, comparisons, null-safe equality, `LIKE`/`NOT LIKE`,
`IN`/`NOT IN`, `IS NULL`/`IS NOT NULL`, parenthesized predicates, `NOT`, `AND`,
and `OR`. Output-column names are resolved ASCII case-insensitively.

Numeric, decimal, float, hex, bit, boolean, national-string, introducer, or
parameter literals; column-to-column comparisons; functions; `BETWEEN`;
`REGEXP`/`RLIKE`; subqueries; CTEs; and arbitrary expression predicates remain
outside this slice.

## Result Semantics

Successful statements return a row result with columns:

```text
Variable_name
Value
```

Rows contain text values only and are emitted in the registry order above. The
order is MyLite-defined for the partial registry and stable for tests.

Successful statements report:

- affected rows `0`;
- warning count `0`;
- no stored warnings;
- `ROW_COUNT()` as `-1` for the following statement.

The statement does not change status values yet. In particular, it does not
increment `Com_show_status`, `Created_tmp_tables`, `Queries`, or `Questions`;
that broader status-counter lifecycle is deferred.

## Diagnostics

- Syntax errors for unsupported grammar use the existing parser diagnostic
  policy, currently `1064` / `42000`.
- Non-string `LIKE` operands remain parse-time syntax errors because the
  admitted grammar accepts only string literals.
- Unsupported `WHERE` predicate forms fail through the shared
  `SHOW VARIABLES` output-row predicate diagnostics instead of being silently
  approximated.
- Allocation failures use the existing `MYLITE_NOMEM` / out-of-memory
  diagnostic policy.
- Physical SQLite failures are not expected because this statement generates no
  SQLite SQL. Any unexpected SQLite involvement is a bug.

## Performance

The statement is O(number of exposed status rows) over a small static array. It
does not query SQLite, materialize a temporary table, scan catalog descriptors,
or allocate an intermediate row set. Pattern filtering happens before each row
is appended.

## Tests

Fast C tests cover:

- parser acceptance for default, `GLOBAL`, `SESSION`, `LOCAL`, and `LIKE`
  forms, plus the admitted `WHERE` predicate forms;
- parser rejection for `LIKE ... WHERE`, `ORDER BY`, `LIMIT`, `FULL`, and
  non-string `LIKE` operands;
- result columns, fixed values, row ordering, scope filtering, and `LIKE`
  filtering;
- exact MySQL 8.4.9 `Com_%` command-counter row names in MySQL expectation
  tests and deterministic zero placeholder values in MyLite tests;
- warning count, affected rows, absence of stored warnings, and `ROW_COUNT()`;
- no storage mutation for file-backed handles;
- independent handles.

The MySQL 8.4.9 expectation script verifies the MySQL reference behavior for
shape, scope, filters, and unsupported clauses. It does not require exact
counter values because those depend on the server process and prior probes.
