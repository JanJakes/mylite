# Baseline Binary Log System Variables

## Summary

This slice exposes MySQL 8.4.9-shaped SQL-visible metadata for these binary-log
system variables:

- `binlog_cache_size`
- `binlog_checksum`
- `binlog_direct_non_transactional_updates`
- `binlog_encryption`
- `binlog_error_action`
- `binlog_expire_logs_auto_purge`
- `binlog_expire_logs_seconds`
- `binlog_format`
- `binlog_group_commit_sync_delay`
- `binlog_group_commit_sync_no_delay_count`
- `binlog_gtid_simple_recovery`
- `binlog_max_flush_queue_time`
- `binlog_order_commits`
- `binlog_rotate_encryption_master_key_at_startup`
- `binlog_row_event_max_size`
- `binlog_row_image`
- `binlog_row_metadata`
- `binlog_row_value_options`
- `binlog_rows_query_log_events`
- `binlog_stmt_cache_size`
- `binlog_transaction_compression`
- `binlog_transaction_compression_level_zstd`
- `binlog_transaction_dependency_history_size`

MyLite does not implement physical binary logs, log rotation, event grouping,
GTID recovery, binary-log encryption, row-event images, statement safety
tracking, or replication side effects. The supported baseline is the
default-state SQL metadata surface: scalar reads, `SHOW VARIABLES` rows,
scope diagnostics, read-only diagnostics, exact/default no-op `SET` forms, and
MySQL deprecation warnings for the two deprecated variables in this group.

## Compatibility Authority

- MySQL 8.4 Reference Manual, binary logging options and variables:
  <https://dev.mysql.com/doc/refman/8.4/en/replication-options-binary-log.html>
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_binary_log_system_variables_expectations.sh`.

The official manual defines the names, scopes, dynamic/read-only properties,
types, defaults, and deprecation status. Runtime probes define the exact
8.4.9 defaults, `SHOW` display values, diagnostics, warnings, and session
scope behavior in the pinned comparison container.

## MySQL 8.4.9 Observations

With the comparison server reset to defaults, MySQL reports:

| Variable | Scalar value | `SHOW VARIABLES` value | Scalar scope |
| --- | --- | --- | --- |
| `binlog_cache_size` | `32768` | `32768` | global |
| `binlog_checksum` | `CRC32` | `CRC32` | global |
| `binlog_direct_non_transactional_updates` | `0` | `OFF` | global/session |
| `binlog_encryption` | `0` | `OFF` | global |
| `binlog_error_action` | `ABORT_SERVER` | `ABORT_SERVER` | global |
| `binlog_expire_logs_auto_purge` | `1` | `ON` | global |
| `binlog_expire_logs_seconds` | `2592000` | `2592000` | global |
| `binlog_format` | `ROW` | `ROW` | global/session |
| `binlog_group_commit_sync_delay` | `0` | `0` | global |
| `binlog_group_commit_sync_no_delay_count` | `0` | `0` | global |
| `binlog_gtid_simple_recovery` | `1` | `ON` | global |
| `binlog_max_flush_queue_time` | `0` | `0` | global |
| `binlog_order_commits` | `1` | `ON` | global |
| `binlog_rotate_encryption_master_key_at_startup` | `0` | `OFF` | global |
| `binlog_row_event_max_size` | `8192` | `8192` | global |
| `binlog_row_image` | `FULL` | `FULL` | global/session |
| `binlog_row_metadata` | `MINIMAL` | `MINIMAL` | global |
| `binlog_row_value_options` | empty string | empty string | global/session |
| `binlog_rows_query_log_events` | `0` | `OFF` | global/session |
| `binlog_stmt_cache_size` | `32768` | `32768` | global |
| `binlog_transaction_compression` | `0` | `OFF` | global/session |
| `binlog_transaction_compression_level_zstd` | `3` | `3` | global/session |
| `binlog_transaction_dependency_history_size` | `25000` | `25000` | global |

All listed variables appear in `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and
`SHOW SESSION VARIABLES`. Global-only variables reject explicit `@@SESSION`
reads with `1238 / HY000` and the global-variable diagnostic. Session-capable
variables accept explicit session reads.

`binlog_gtid_simple_recovery`,
`binlog_rotate_encryption_master_key_at_startup`, and
`binlog_row_event_max_size` are read-only at runtime and reject `SET GLOBAL`
with `1238 / HY000`. Unscoped `SET` for global-only dynamic variables rejects
with `1229 / HY000` and the MySQL `SET GLOBAL` required diagnostic.

`binlog_format` and `binlog_max_flush_queue_time` emit warning `1287` on
scalar reads and on supported `SET ... = DEFAULT` forms. MySQL supports
session mutation for session-capable variables such as `binlog_format`,
`binlog_row_image`, `binlog_rows_query_log_events`, and
`binlog_transaction_compression_level_zstd`.

## MyLite Scope

MyLite supports:

- unscoped and `GLOBAL` scalar reads for all listed variables;
- `SESSION` / `LOCAL` scalar reads for the MySQL session-capable variables;
- MySQL-style `SESSION` / `LOCAL` scalar diagnostics for global-only variables;
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES` rows
  with MySQL-shaped display values;
- read-only diagnostics for the three MySQL runtime read-only variables;
- exact/default no-op assignments for supported non-read-only variables, using
  MySQL-style boolean, text, and integer literal forms;
- MySQL-style deprecation warnings for `binlog_format` and
  `binlog_max_flush_queue_time` scalar reads and exact/default `SET` forms;
- deterministic unsupported diagnostics for state-changing assignments and
  user-variable-backed binlog assignments.

MyLite intentionally does not support:

- binary log file creation, event buffering, checksums, encryption, rotation,
  expiry, or group commit behavior;
- GTID recovery, replication topology, source/replica side effects, or unsafe
  statement tracking;
- mutable global binary-log configuration, startup options, persisted
  variables, `SET_VAR` hints, or privilege checks;
- handle-local mutation of session-capable binlog variables. This is a known
  future compatibility area. MyLite currently accepts only exact/default no-op
  assignments so application initialization can succeed without pretending a
  physical binary log exists.

## Syntax

No new grammar is required. Existing scalar system-variable, `SHOW VARIABLES`,
and `SET` productions admit the supported forms:

```sql
SELECT @@binlog_format, @@GLOBAL.binlog_cache_size;
SHOW VARIABLES LIKE 'binlog_%';
SET GLOBAL binlog_cache_size = DEFAULT;
SET SESSION binlog_row_image = FULL;
```

## Diagnostics

- Unknown variables continue to use `1193 / HY000`.
- `@@SESSION` / `@@LOCAL` reads of global-only variables use `1238 / HY000`
  with `Variable '<name>' is a GLOBAL variable`.
- Unscoped assignment to global-only dynamic variables uses `1229 / HY000`
  with the MySQL global-variable message.
- Runtime read-only variables use `1238 / HY000` with
  `Variable '<name>' is a read only variable`.
- State-changing values and user-variable-backed assignments use MyLite's
  deterministic fixed-no-op unsupported diagnostics.
- Deprecated binary-log variable reads and supported no-op assignments use
  warning `1287`.

## Runtime And Storage

This slice is implemented in MyLite's system-variable registry, scalar readback,
`SHOW VARIABLES` display path, `SET` validation, and diagnostic handling. It
does not add public ABI, catalog rows, SQLite SQL, SQLite extension API use, a
SQLite fork hook, file-format state, VFS behavior, persistent state, or mutable
process-global state.

## Tests

Coverage includes:

- a MySQL 8.4.9 expectation script for defaults, `SHOW` rows, scope
  diagnostics, read-only diagnostics, deprecation warnings, and upstream
  mutable session behavior;
- a runtime C test for MyLite scalar values, `SHOW` rows, diagnostics, no-op
  fixed assignments, unsupported state-changing assignments, and deprecation
  warnings;
- full `SHOW VARIABLES` registry regression coverage.

Focused verification:

```sh
sh -n packages/libmylite/tests/mysql_baseline_binary_log_system_variables_expectations.sh
sh packages/libmylite/tests/mysql_baseline_binary_log_system_variables_expectations.sh
cmake --build --preset dev --target mylite_runtime_binary_log_system_variables_test
ctest --preset dev -R '^libmylite\.runtime\.binary_log_system_variables$' --output-on-failure
```
