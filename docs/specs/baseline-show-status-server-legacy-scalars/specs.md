# Baseline SHOW STATUS Server and Legacy Scalar Placeholders

## Summary

This slice adds deterministic `SHOW STATUS` placeholders for MySQL 8.4.9 status
variables that are commonly present in the default server surface but do not
map to live state in MyLite's embedded runtime:

- cache, memory, and lock scalars: `Acl_cache_items_count`,
  `Global_connection_memory`, `Locked_connects`;
- legacy delayed-insert scalars: `Delayed_errors`,
  `Delayed_insert_threads`, `Delayed_writes`, `Not_flushed_delayed_rows`;
- flush and key-cache counters: `Flush_commands`, `Key_%`;
- optimizer/execution scalars: `Last_query_cost`,
  `Last_query_partial_plans`, `Max_execution_time_%`;
- connection-high-water rows: `Max_used_connections`,
  `Max_used_connections_time`;
- replication/secondary-engine scalars: `Ongoing_anonymous_transaction_count`,
  `Replica_open_temp_tables`, `Slave_open_temp_tables`,
  `Secondary_engine_execution_count`;
- static feature flags: `Resource_group_supported`,
  `Telemetry_logs_supported`, `Telemetry_metrics_supported`, and
  `Telemetry_traces_supported`.

MyLite exposes the observed row names in MySQL runtime order for default,
session, local, and global scopes. Numeric counters are fixed embedded
placeholders. Capability flags are stable MyLite-owned values and report `OFF`
when the corresponding subsystem is not implemented by MyLite.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `SHOW STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-status.html>
- MySQL 8.4 Reference Manual, server status variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-status-variable-reference.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_show_status_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified the row names and scope visibility for:

```sql
SHOW [GLOBAL | SESSION | LOCAL] STATUS LIKE 'Key\_%';
SHOW [GLOBAL | SESSION | LOCAL] STATUS LIKE 'Max\_%';
SHOW [GLOBAL | SESSION | LOCAL] STATUS LIKE 'Telemetry\_%';
SHOW STATUS WHERE Variable_name IN (...);
```

## Supported Behavior

Each row is visible through default, `SESSION`, `LOCAL`, and `GLOBAL`
`SHOW STATUS` scopes. Existing `LIKE` and limited `WHERE` filtering semantics
apply.

MyLite values are:

| Variable group | MyLite value |
| --- | --- |
| Counter and memory scalars | `0` |
| `Last_query_cost` | `0.000000` |
| `Max_used_connections` | `1` |
| `Max_used_connections_time` | `1970-01-01 00:00:00` |
| `Resource_group_supported` | `OFF` |
| `Telemetry_*_supported` | `OFF` |

`sys.metrics` is derived from the global-visible status descriptor set, so
these rows also appear as lowercase `Global Status` metrics.

## Unsupported Behavior

This slice intentionally does not add:

- live ACL cache, connection memory, delayed insert, key cache, or lock
  accounting;
- live optimizer cost tracking;
- statement execution-time enforcement counters;
- real maximum concurrent connection history;
- replication temp table, anonymous transaction, or secondary-engine state;
- resource group or telemetry subsystems;
- `FLUSH STATUS` reset behavior;
- Performance Schema status-variable table rows beyond MyLite's current
  synthetic descriptor-derived surfaces.

## Tests

Fast C tests assert exact row names and deterministic values for representative
families and `sys.metrics` readback. The MySQL expectation script verifies row
names and scope visibility against MySQL 8.4.9 without depending on live
mutable counter values.
