# Baseline SHOW STATUS InnoDB Placeholders

## Scope

This slice adds MySQL 8.4.9-shaped `SHOW STATUS LIKE 'Innodb%'` rows exposed
by the pinned comparison runtime. MyLite's storage engine is SQLite with
MyLite-owned MySQL compatibility metadata, so these rows are deterministic
embedded placeholders rather than live InnoDB instrumentation.

All 72 rows are visible in default, `SESSION`, `LOCAL`, and `GLOBAL` scopes.
Most counter and size rows return `0`. Fixed state placeholders are:

| Variable | MyLite value |
| --- | --- |
| `Innodb_buffer_pool_dump_status` | `Dumping of buffer pool not started` |
| `Innodb_buffer_pool_load_status` | empty string |
| `Innodb_buffer_pool_resize_status` | empty string |
| `Innodb_redo_log_read_only` | `OFF` |
| `Innodb_redo_log_resize_status` | `OK` |
| `Innodb_page_size` | `16384` |
| `Innodb_redo_log_enabled` | `ON` |

## Sources

- MySQL 8.4 Reference Manual, `SHOW STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-status.html>
- MySQL 8.4 Reference Manual, server status variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-status-variables.html>
- MySQL 8.4.9 runtime probes in
  `packages/libmylite/tests/mysql_baseline_show_status_expectations.sh`.

## Semantics

`SHOW STATUS LIKE 'Innodb%'` exposes the row names and scope visibility observed
from MySQL 8.4.9. `sys.metrics` exposes the same rows as lowercase
`Global Status` metrics because they are global-visible.

The values are placeholders. They do not imply that MyLite uses InnoDB pages,
redo logs, undo tablespaces, buffer-pool statistics, doublewrite buffers, row
locks, or InnoDB sampling internals.

## Unsupported Behavior

This slice does not implement live InnoDB status accounting, `FLUSH STATUS`
counter reset behavior, physical InnoDB page or redo-log state, or MySQL
storage-engine performance instrumentation.
