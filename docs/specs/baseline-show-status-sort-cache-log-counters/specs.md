# Baseline SHOW STATUS Sort, Cache, Log, And Slow Counter Placeholders

## Summary

MyLite exposes small MySQL 8.4.9 status-counter families used for query sort
diagnostics, table-open cache diagnostics, transaction-coordinator log
diagnostics, and slow-thread counters. The rows are deterministic embedded
placeholders with MySQL row names, order, and scope visibility, but no live
counter lifecycle.

## MySQL 8.4.9 Observations

Observed against the local `mysql:8.4.9` runtime:

```sql
SHOW STATUS LIKE 'Sort\_%';
```

returns `Sort_merge_passes`, `Sort_range`, `Sort_rows`, and `Sort_scan`.

```sql
SHOW STATUS LIKE 'Table_open_cache\_%';
```

returns `Table_open_cache_hits`, `Table_open_cache_misses`, and
`Table_open_cache_overflows`.

```sql
SHOW STATUS LIKE 'Tc_log\_%';
```

returns `Tc_log_max_pages_used`, `Tc_log_page_size`, and `Tc_log_page_waits`.

```sql
SHOW STATUS LIKE 'Slow\_%';
```

returns `Slow_launch_threads` and `Slow_queries`.

The same row names are visible in session and global scopes. Counter values can
change with server activity, so MyLite pins deterministic placeholder values.

## MyLite Behavior

All rows in this slice return `0` for default, `SESSION`, `LOCAL`, and `GLOBAL`
`SHOW STATUS` scopes. The rows are also exposed through `sys.metrics` as
lowercase `Global Status` metrics with `Enabled = 'YES'`.

## Non-Goals

- No live sort, slow-query, table-cache, or transaction-coordinator log
  accounting.
- No `FLUSH STATUS` counter reset lifecycle.
- No Performance Schema status table emulation beyond the existing
  `sys.metrics` synthetic view.

## Verification

- `packages/libmylite/tests/mysql_baseline_show_status_expectations.sh`
  verifies MySQL 8.4.9 row names and order.
- `packages/libmylite/tests/runtime_show_status_test.c` verifies MyLite result
  rows and values.
- `packages/libmylite/tests/runtime_sys_metrics_view_test.c` verifies
  representative `sys.metrics` rows and the global metric count.
