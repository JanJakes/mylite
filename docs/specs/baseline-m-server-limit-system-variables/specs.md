# Baseline M Server Limit System Variables

## Summary

This slice exposes MySQL 8.4.9-shaped metadata for fixed/global M-range server
limit variables that applications commonly inspect during bootstrap:

- `master_verify_checksum`
- `max_binlog_cache_size`
- `max_binlog_size`
- `max_binlog_stmt_cache_size`
- `max_connect_errors`
- `max_connections`
- `max_digest_length`
- `max_prepared_stmt_count`
- `max_relay_log_size`
- `max_write_lock_count`

The variables in this slice are represented as embedded placeholders. MyLite
returns MySQL 8.4.9 defaults, `SHOW VARIABLES` rows, global-only diagnostics,
read-only diagnostics where applicable, and exact/default fixed global no-op
assignments for mutable MySQL variables. MyLite does not enforce server
connection limits, binary log cache limits, prepared statement limits, relay log
limits, digest truncation limits, or lock scheduling limits.

This slice intentionally excludes session-scoped or behavior-changing nearby M
variables such as `max_execution_time`, `max_heap_table_size`, `max_join_size`,
`max_sort_length`, `min_examined_row_limit`, `max_delayed_threads`,
`max_insert_delayed_threads`, and `max_points_in_geometry`. Those belong in
separate handle-local session-state or execution-effect slices.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_m_server_limit_system_variables_expectations.sh`.

The manual establishes that system variables have scope, dynamic/read-only
classification, defaults, and expression/`SHOW VARIABLES` visibility. Runtime
probes establish pinned defaults, scalar display, `SHOW` display, scope
diagnostics, read-only diagnostics, and deprecation warnings.

## MySQL 8.4.9 Observations

| Variable | Scalar value | `SHOW VARIABLES` value | Scalar scope |
| --- | --- | --- | --- |
| `master_verify_checksum` | `0` | `OFF` | global |
| `max_binlog_cache_size` | `18446744073709547520` | `18446744073709547520` | global |
| `max_binlog_size` | `1073741824` | `1073741824` | global |
| `max_binlog_stmt_cache_size` | `18446744073709547520` | `18446744073709547520` | global |
| `max_connect_errors` | `100` | `100` | global |
| `max_connections` | `151` | `151` | global |
| `max_digest_length` | `1024` | `1024` | global |
| `max_prepared_stmt_count` | `16382` | `16382` | global |
| `max_relay_log_size` | `0` | `0` | global |
| `max_write_lock_count` | `18446744073709551615` | `18446744073709551615` | global |

Global-only variables also appear in `SHOW SESSION VARIABLES`. Session scalar
reads return `1238 / HY000`, `Variable '<name>' is a GLOBAL variable`.

Observed assignment behavior:

- mutable global variables accept `SET GLOBAL <name> = DEFAULT` and exact
  current-value assignments;
- non-global assignment to mutable global variables returns `1229 / HY000`;
- `max_digest_length` is read-only and rejects assignment with `1238 / HY000`;
- `master_verify_checksum` is a deprecated alias and emits warning
  `1287 / HY000`, recommending `source_verify_checksum`, on scalar reads and
  successful no-op assignments. A fresh `SHOW VARIABLES LIKE
  'master_verify_checksum'` result does not append that warning.

## MyLite Scope

MyLite supports:

- scalar reads and `SHOW VARIABLES` rows for all variables in this slice;
- MySQL-shaped global-only diagnostics;
- MySQL-shaped read-only diagnostics for `max_digest_length`;
- deprecation warning `1287` for `master_verify_checksum` scalar reads and
  no-op SET;
- exact/default fixed global no-op assignments for mutable variables.

MyLite intentionally does not support:

- mutable process-global state for this slice;
- binary log file, cache, statement-cache, or relay-log sizing behavior;
- connection admission limits, host-cache blocking, or thread scheduling;
- digest truncation behavior controlled by `max_digest_length`;
- prepared-statement count enforcement;
- write-lock scheduling or starvation behavior.

## Syntax

The existing system-variable, `SHOW VARIABLES`, and `SET` productions admit the
supported forms:

```sql
SELECT @@GLOBAL.max_connections;
SHOW VARIABLES LIKE 'max_binlog_size';
SET GLOBAL max_connect_errors = DEFAULT;
SET GLOBAL master_verify_checksum = OFF;
```

No new grammar is required.

## Diagnostics

- Unknown variables continue to use `1193 / HY000`.
- Session reads of global-only variables use `1238 / HY000`.
- Non-global `SET` for mutable global variables uses `1229 / HY000`.
- Read-only variables use MySQL-shaped `1238 / HY000` diagnostics.
- Value-changing global assignments return MyLite's deterministic unsupported
  fixed-no-op diagnostics.
- `master_verify_checksum` scalar reads and successful no-op assignments append
  warning `1287 / HY000`.

## Runtime And Storage

This slice is implemented in MyLite's system-variable registry, scalar readback,
`SHOW VARIABLES` display path, compatibility placeholder SET validation, and
diagnostic warning paths. It does not introduce public ABI, catalog rows,
SQLite SQL, SQLite fork patches, file-format state, VFS behavior, or mutable
process-global state.

## Tests

Coverage includes:

- a MySQL 8.4.9 expectation script for defaults, `SHOW` rows, global-only
  diagnostics, read-only diagnostics, no-op assignment forms, deprecation
  warnings, and upstream mutable global observations;
- runtime C tests for scalar values, `SHOW` rows, diagnostics, deprecation
  warnings, fixed no-op assignments, and unsupported state-changing
  assignments;
- full `SHOW VARIABLES` registry regression coverage.
