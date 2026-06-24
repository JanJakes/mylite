# Baseline SHOW STATUS Deprecated and Error-Log Placeholders

## Scope

This slice adds MySQL 8.4.9-shaped `SHOW STATUS` rows for deprecated-usage and
error-log counters exposed by the pinned comparison runtime:

| Variable | Default/session/LOCAL visibility | GLOBAL visibility | MyLite value |
| --- | --- | --- | --- |
| `Deprecated_use_fk_on_non_standard_key_count` | yes | yes | `0` |
| `Deprecated_use_fk_on_non_standard_key_last_timestamp` | yes | yes | `0` |
| `Deprecated_use_i_s_processlist_count` | yes | yes | `0` |
| `Deprecated_use_i_s_processlist_last_timestamp` | yes | yes | `0` |
| `Error_log_buffered_bytes` | yes | yes | `0` |
| `Error_log_buffered_events` | yes | yes | `0` |
| `Error_log_expired_events` | yes | yes | `0` |
| `Error_log_latest_write` | yes | yes | `0` |

## Sources

- MySQL 8.4 Reference Manual, `SHOW STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-status.html>
- MySQL 8.4 Reference Manual, server status variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-status-variables.html>
- MySQL 8.4.9 runtime probes in
  `packages/libmylite/tests/mysql_baseline_show_status_expectations.sh`.

## Semantics

MyLite exposes the row names and scope visibility observed from MySQL 8.4.9 for
`SHOW STATUS LIKE 'Deprecated%'` and `SHOW STATUS LIKE 'Error_log%'`.

The rows are deterministic embedded placeholders. They do not track deprecated
feature use or server error-log buffering. `sys.metrics` exposes these rows as
lowercase `Global Status` metrics because all rows are global-visible in the
target runtime.

## Unsupported Behavior

This slice does not implement:

- live counters for deprecated `INFORMATION_SCHEMA.PROCESSLIST` or
  non-standard-key foreign-key use;
- last-use timestamps for deprecated features;
- server error-log buffering, expiration, write timestamps, or log sinks.
