# Baseline SHOW STATUS Binary-Log Counters

## Summary

This slice expands the `SHOW STATUS` placeholder registry with the MySQL 8.4.9
`Binlog_%` binary-log cache counter rows:

```sql
SHOW [GLOBAL | SESSION | LOCAL] STATUS LIKE 'Binlog\_%'
```

MyLite exposes the observed row names in MySQL runtime order for default,
session, local, and global scopes. Values are deterministic embedded `0`
placeholders. This is status metadata only; it does not implement a server
binary log, statement cache accounting, or transaction cache accounting.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `SHOW STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-status.html>
- MySQL 8.4 Reference Manual, server status variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-status-variable-reference.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_show_status_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified these `Binlog_%` row names:

| Variable |
| --- |
| `Binlog_cache_disk_use` |
| `Binlog_cache_use` |
| `Binlog_stmt_cache_disk_use` |
| `Binlog_stmt_cache_use` |

The observed values are live MySQL server counters and are intentionally not
pinned as MyLite expectations.

## Supported Behavior

Each row is visible through default, `SESSION`, `LOCAL`, and `GLOBAL`
`SHOW STATUS` scopes and has a fixed MyLite value of `0`. Existing `LIKE` and
limited `WHERE` filtering semantics apply.

`sys.metrics` is derived from the global-visible status descriptor set, so
these rows also appear as lowercase `Global Status` metrics with value `0`.

## Unsupported Behavior

This slice intentionally does not add:

- server binary-log files;
- live binary-log cache counters;
- row, statement, transaction, relay-log, or replication accounting;
- `FLUSH STATUS` reset behavior;
- Performance Schema binary-log cache status rows beyond current synthetic
  descriptor-derived surfaces.

## Tests

Fast C tests assert exact `SHOW STATUS LIKE 'Binlog\_%'` rows for session and
global scopes, deterministic `0` values, and representative `sys.metrics`
readback. The MySQL expectation script verifies the row-name set and scope
visibility against MySQL 8.4.9 without depending on live mutable values.
