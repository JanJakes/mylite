# Baseline SHOW STATUS Select And Table-Lock Counter Placeholders

## Summary

MyLite exposes MySQL 8.4.9-shaped `SHOW STATUS` rows for optimizer-select and
table-lock counter families. The rows are deterministic embedded placeholders:
they preserve MySQL row names, order, scopes, and metadata shape for
application introspection, but they do not track live optimizer decisions or
table-lock waits.

This slice adds the missing `Select_range_check` placeholder and verifies the
complete `Select_%` and `Table_locks_%` row-name surfaces for default,
`SESSION`, `LOCAL`, and `GLOBAL` scopes.

## MySQL 8.4.9 Observations

Observed against the local `mysql:8.4.9` runtime:

```sql
SHOW STATUS LIKE 'Select\_%';
```

returns these row names in order:

| Variable_name |
| --- |
| `Select_full_join` |
| `Select_full_range_join` |
| `Select_range` |
| `Select_range_check` |
| `Select_scan` |

```sql
SHOW STATUS LIKE 'Table_locks\_%';
```

returns these row names in order:

| Variable_name |
| --- |
| `Table_locks_immediate` |
| `Table_locks_waited` |

The same names are present for `SHOW SESSION STATUS`, `SHOW LOCAL STATUS`, and
`SHOW GLOBAL STATUS`. Values are counters and can differ by connection and
server state, so MyLite pins only the embedded placeholder values.

## MyLite Behavior

`SHOW STATUS LIKE 'Select\_%'` returns:

| Variable_name | Value |
| --- | --- |
| `Select_full_join` | `0` |
| `Select_full_range_join` | `0` |
| `Select_range` | `0` |
| `Select_range_check` | `0` |
| `Select_scan` | `0` |

`SHOW STATUS LIKE 'Table_locks\_%'` returns:

| Variable_name | Value |
| --- | --- |
| `Table_locks_immediate` | `0` |
| `Table_locks_waited` | `0` |

The rows are also exposed in `sys.metrics` as lowercase `Global Status` metrics
with `Enabled = 'YES'`.

## Non-Goals

- No live optimizer-path accounting for `Select_%`.
- No live table-lock acquisition or wait counters.
- No Performance Schema status table emulation beyond the existing
  `sys.metrics` synthetic view.

## Verification

- `packages/libmylite/tests/mysql_baseline_show_status_expectations.sh`
  verifies MySQL 8.4.9 row names and order.
- `packages/libmylite/tests/runtime_show_status_test.c` verifies MyLite result
  rows and values.
- `packages/libmylite/tests/runtime_sys_metrics_view_test.c` verifies
  representative `sys.metrics` rows and the global metric count.
