# Baseline SHOW STATUS Temporary and Open Resource Counters

## Summary

This slice verifies and documents MyLite's existing `SHOW STATUS` placeholders
for the MySQL 8.4.9 temporary-table and open-resource counter rows:

```sql
SHOW [GLOBAL | SESSION | LOCAL] STATUS LIKE 'Created\_%';
SHOW [GLOBAL | SESSION | LOCAL] STATUS LIKE 'Open%';
```

MyLite exposes the observed row names in MySQL runtime order for default,
session, local, and global scopes. Values are deterministic embedded `0`
placeholders. This is status metadata only; it does not implement temporary
table, open file, table-definition cache, or table-cache accounting.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `SHOW STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-status.html>
- MySQL 8.4 Reference Manual, server status variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-status-variable-reference.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_show_status_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified these row names:

| Variable |
| --- |
| `Created_tmp_disk_tables` |
| `Created_tmp_files` |
| `Created_tmp_tables` |
| `Open_files` |
| `Open_streams` |
| `Open_table_definitions` |
| `Open_tables` |
| `Opened_files` |
| `Opened_table_definitions` |
| `Opened_tables` |

Observed MySQL values are live server counters and are intentionally not pinned
as MyLite expectations.

## Supported Behavior

Each row is visible through default, `SESSION`, `LOCAL`, and `GLOBAL`
`SHOW STATUS` scopes and has a fixed MyLite value of `0`. Existing `LIKE` and
limited `WHERE` filtering semantics apply.

`sys.metrics` is derived from the global-visible status descriptor set, so
these rows also appear as lowercase `Global Status` metrics with value `0`.

## Unsupported Behavior

This slice intentionally does not add:

- live temporary-table counters;
- open file or stream counters;
- table-definition cache or table-cache counters;
- `FLUSH STATUS` reset behavior;
- Performance Schema status-variable rows beyond current synthetic
  descriptor-derived surfaces.

## Tests

Fast C tests assert exact `SHOW STATUS LIKE 'Created\_%'` and
`SHOW STATUS LIKE 'Open%'` rows for session and global scopes, deterministic
`0` values, and representative `sys.metrics` readback. The MySQL expectation
script verifies the row-name set and scope visibility against MySQL 8.4.9
without depending on live mutable values.
