# Baseline Server Logging System Variables

## Summary

This slice exposes a MySQL 8.4.9-shaped baseline for server logging system
variables that applications commonly inspect:

- `general_log`
- `general_log_file`
- `log_error`
- `log_error_services`
- `log_error_suppression_list`
- `log_error_verbosity`
- `log_output`
- `log_queries_not_using_indexes`
- `log_raw`
- `log_replica_updates`
- `log_slave_updates`
- `log_slow_admin_statements`
- `log_slow_extra`
- `log_slow_replica_statements`
- `log_slow_slave_statements`
- `log_statements_unsafe_for_binlog`
- `log_throttle_queries_not_using_indexes`
- `log_timestamps`
- `long_query_time`
- `slow_query_log`
- `slow_query_log_file`

MyLite does not implement server log files, error-log sinks, log tables, slow
query collection, binary logging side effects, startup options, persisted
variables, or privileged process-global logging mutation. The supported
baseline is the SQL-visible default-state shape: scalar reads, `SHOW
VARIABLES`, scope diagnostics, read-only diagnostics, fixed global no-op
assignments where they are safe, deprecation warnings, and session-local
`long_query_time` assignment semantics.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_server_logging_system_variables_expectations.sh`.

The official manual defines the variable names, scopes, dynamic/read-only
properties, defaults, valid ranges, and deprecation status. Runtime probes
define the exact pinned-container defaults, `SHOW` display values, diagnostics,
warning codes, and numeric formatting.

## MySQL 8.4.9 Observations

With the comparison server reset to defaults, MySQL reports these scalar and
`SHOW VARIABLES` values:

| Variable | Scalar value | `SHOW VARIABLES` value |
| --- | --- | --- |
| `general_log` | `0` | `OFF` |
| `general_log_file` | `@@datadir` + `@@hostname` + `.log` | same path |
| `log_error` | `stderr` | `stderr` |
| `log_error_services` | `log_filter_internal; log_sink_internal` | same text |
| `log_error_suppression_list` | empty string | empty string |
| `log_error_verbosity` | `2` | `2` |
| `log_output` | `FILE` | `FILE` |
| `log_queries_not_using_indexes` | `0` | `OFF` |
| `log_raw` | `0` | `OFF` |
| `log_replica_updates` | `1` | `ON` |
| `log_slave_updates` | `1` | `ON` |
| `log_slow_admin_statements` | `0` | `OFF` |
| `log_slow_extra` | `0` | `OFF` |
| `log_slow_replica_statements` | `0` | `OFF` |
| `log_slow_slave_statements` | `0` | `OFF` |
| `log_statements_unsafe_for_binlog` | `1` | `ON` |
| `log_throttle_queries_not_using_indexes` | `0` | `0` |
| `log_timestamps` | `UTC` | `UTC` |
| `long_query_time` | `10.000000` | `10.000000` |
| `slow_query_log` | `0` | `OFF` |
| `slow_query_log_file` | `@@datadir` + `@@hostname` + `-slow.log` | same path |

All listed variables appear in `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and
`SHOW SESSION VARIABLES`. All except `long_query_time` are global-only for
scalar reads; `@@SESSION.name` and `@@LOCAL.name` fail with `1238 / HY000` and
a message that the variable is global. Non-global `SET name = DEFAULT` fails
with `1229 / HY000` for dynamic global-only variables. `log_error`,
`log_replica_updates`, and `log_slave_updates` are read-only and fail with
`1238 / HY000` for both unscoped and global assignments.

`log_slave_updates`, `log_slow_slave_statements`, and
`log_statements_unsafe_for_binlog` emit warning `1287` on scalar reads. The
first two warnings point to the newer replica-named variables; the unsafe
binlog variable warns that the variable is deprecated. Setting the deprecated
dynamic aliases to their default values succeeds and emits warning `1287`.

`long_query_time` is session-capable and dynamic. MySQL formats reads with six
fractional digits, rounds fractional seconds to microseconds, clamps values
below `0` and above `31536000` with warning `1292`, accepts integer and decimal
user-variable assignments, and rejects string, `NULL`, and boolean assignments
with `1232 / 42000`. MySQL also supports mutable global assignment for
`long_query_time`; MyLite intentionally does not mutate process-global logging
state and only accepts default-compatible global no-ops.

## MyLite Scope

MyLite supports:

- unscoped and `GLOBAL` scalar reads for all listed variables;
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES` rows
  with MySQL-shaped display values;
- global-only `SESSION` / `LOCAL` scalar diagnostics for global-only logging
  variables;
- read-only diagnostics for `log_error`, `log_replica_updates`, and
  `log_slave_updates`;
- fixed default-compatible `SET GLOBAL` no-ops for mutable global-only logging
  variables;
- MySQL-style deprecation warnings for `log_slave_updates`,
  `log_slow_slave_statements`, and `log_statements_unsafe_for_binlog`;
- session/local/default assignment, rounding, clamping warnings, user-variable
  reads, and formatting for `long_query_time`;
- deterministic unsupported diagnostics for state-changing global
  `long_query_time` and user-variable-backed fixed logging assignments.

MyLite intentionally does not support:

- writing general, slow, or error log files;
- routing log output to `FILE`, `TABLE`, JSON sinks, or other error-log
  services;
- populating `mysql.general_log` or `mysql.slow_log`;
- slow-query timing, query classification, or throttling side effects;
- binary logging, replication log update behavior, or unsafe-statement
  reporting side effects;
- process-global mutable logging configuration, option-file startup state,
  persisted variables, or privilege checks.

## Syntax

No new grammar is required. Existing scalar system-variable, `SHOW VARIABLES`,
and `SET` productions admit the supported forms:

```sql
SELECT @@general_log, @@GLOBAL.slow_query_log_file;
SHOW VARIABLES LIKE 'log_%';
SET GLOBAL general_log = DEFAULT;
SET long_query_time = 1.25;
SET LOCAL long_query_time = 0;
```

## Diagnostics

- Unknown variables continue to use `1193 / HY000`.
- `@@SESSION` / `@@LOCAL` reads of global-only variables use `1238 / HY000`
  with `Variable '<name>' is a GLOBAL variable`.
- Non-global assignment to dynamic global-only variables uses `1229 / HY000`
  with the MySQL global-variable message.
- Read-only variables use `1238 / HY000` with
  `Variable '<name>' is a read only variable`.
- State-changing global assignments that MyLite cannot honor use MyLite's
  deterministic fixed-no-op unsupported diagnostic.
- Invalid `long_query_time` argument types use `1232 / 42000`.
- `long_query_time` clamps use warning `1292`.
- Deprecated logging variable reads and supported no-op assignments use warning
  `1287`.

## Runtime And Storage

This slice is implemented in MyLite's system-variable registry, scalar readback,
`SHOW VARIABLES` display path, session state, `SET` validation, and diagnostic
handling. It does not add public ABI, SQLite SQL, SQLite extension API use, a
SQLite fork hook, catalog rows, file-format state, VFS behavior, persistent
state, or mutable process-global state.

`general_log_file` and `slow_query_log_file` use deterministic MyLite paths
based on the existing fixed `datadir` and `hostname` placeholders:
`/var/lib/mysql/mylite.log` and `/var/lib/mysql/mylite-slow.log`.

## Tests

Coverage includes:

- a MySQL 8.4.9 expectation script for defaults, `SHOW` rows, scope
  diagnostics, read-only diagnostics, deprecation warnings, and
  `long_query_time` numeric behavior;
- a runtime C test for scalar values, `SHOW` rows, diagnostics, no-op fixed
  global assignments, deprecated warnings, `long_query_time` session state,
  rollback, and user-variable assignment paths;
- full `SHOW VARIABLES` registry regression coverage.
