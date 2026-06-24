# Baseline SHOW STATUS Handler Counters

## Summary

This slice expands the `SHOW STATUS` placeholder registry with the MySQL 8.4.9
`Handler_%` storage-engine handler counter rows:

```sql
SHOW [GLOBAL | SESSION | LOCAL] STATUS LIKE 'Handler\_%'
```

MyLite exposes the observed row names in MySQL runtime order for default,
session, local, and global scopes. Values are deterministic embedded `0`
placeholders. This is status metadata only; it does not implement live
storage-engine handler operation accounting.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `SHOW STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-status.html>
- MySQL 8.4 Reference Manual, server status variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-status-variable-reference.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_show_status_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified these `Handler_%` row names:

| Variable |
| --- |
| `Handler_commit` |
| `Handler_delete` |
| `Handler_discover` |
| `Handler_external_lock` |
| `Handler_mrr_init` |
| `Handler_prepare` |
| `Handler_read_first` |
| `Handler_read_key` |
| `Handler_read_last` |
| `Handler_read_next` |
| `Handler_read_prev` |
| `Handler_read_rnd` |
| `Handler_read_rnd_next` |
| `Handler_rollback` |
| `Handler_savepoint` |
| `Handler_savepoint_rollback` |
| `Handler_update` |
| `Handler_write` |

Session-scope values were all `0` in the clean probe. Global-scope values are
live MySQL counters and therefore intentionally not pinned.

## Supported Behavior

Each row is visible through default, `SESSION`, `LOCAL`, and `GLOBAL`
`SHOW STATUS` scopes and has a fixed MyLite value of `0`. Existing `LIKE` and
limited `WHERE` filtering semantics apply.

`sys.metrics` is derived from the global-visible status descriptor set, so
these rows also appear as lowercase `Global Status` metrics with value `0`.

## Unsupported Behavior

This slice intentionally does not add:

- live handler operation counters;
- increments for table scans, index reads, writes, transactions, or savepoints;
- `FLUSH STATUS` reset behavior;
- storage-engine-specific Performance Schema accounting;
- optimizer or execution-plan feedback from these counters.

## Tests

Fast C tests assert exact `SHOW STATUS LIKE 'Handler\_%'` rows for session and
global scopes, deterministic `0` values, and representative `sys.metrics`
readback. The MySQL expectation script verifies the row-name set and scope
visibility against MySQL 8.4.9 without depending on live mutable global values.
