# Baseline InnoDB IO and Log System Variables

## Scope

This slice adds MySQL 8.4.9-shaped metadata placeholders for these InnoDB IO,
lock-wait, redo-log, and LRU system variables:

- `innodb_idle_flush_pct`
- `innodb_io_capacity`
- `innodb_io_capacity_max`
- `innodb_lock_wait_timeout`
- `innodb_log_buffer_size`
- `innodb_log_checksums`
- `innodb_log_compressed_pages`
- `innodb_log_file_size`
- `innodb_log_files_in_group`
- `innodb_log_group_home_dir`
- `innodb_log_spin_cpu_abs_lwm`
- `innodb_log_spin_cpu_pct_hwm`
- `innodb_log_wait_for_flush_spin_hwm`
- `innodb_log_write_ahead_size`
- `innodb_log_writer_threads`
- `innodb_lru_scan_depth`

The goal is baseline compatibility for scalar reads, `SHOW VARIABLES` rows,
scope diagnostics, read-only diagnostics, fixed/default global no-op
assignments, and handle-local `innodb_lock_wait_timeout` session assignment.
This does not implement InnoDB flushing, physical redo-log sizing or layout,
redo-log checksum/write behavior, log writer scheduling, lock wait enforcement,
or LRU scanning behavior.

## MySQL 8.4.9 Observations

Runtime probes against MySQL 8.4.9 showed these defaults and scopes:

| Variable | Scalar value | SHOW value | Scope | Mutation |
| --- | --- | --- | --- | --- |
| `innodb_idle_flush_pct` | `100` | `100` | global | dynamic global |
| `innodb_io_capacity` | `10000` | `10000` | global | dynamic global |
| `innodb_io_capacity_max` | `20000` | `20000` | global | dynamic global |
| `innodb_lock_wait_timeout` | `50` | `50` | global/session | dynamic session and global |
| `innodb_log_buffer_size` | `67108864` | `67108864` | global | dynamic global |
| `innodb_log_checksums` | `1` | `ON` | global | dynamic global |
| `innodb_log_compressed_pages` | `1` | `ON` | global | dynamic global |
| `innodb_log_file_size` | `50331648` | `50331648` | global | read-only |
| `innodb_log_files_in_group` | `2` | `2` | global | read-only |
| `innodb_log_group_home_dir` | `./` | `./` | global | read-only |
| `innodb_log_spin_cpu_abs_lwm` | `80` | `80` | global | dynamic global |
| `innodb_log_spin_cpu_pct_hwm` | `50` | `50` | global | dynamic global |
| `innodb_log_wait_for_flush_spin_hwm` | `400` | `400` | global | dynamic global |
| `innodb_log_write_ahead_size` | `8192` | `8192` | global | dynamic global |
| `innodb_log_writer_threads` | `1` | `ON` | global | dynamic global |
| `innodb_lru_scan_depth` | `1024` | `1024` | global | dynamic global |

All variables appear in default, global, and session `SHOW VARIABLES` output.
For global-only variables, scalar `@@SESSION` and `@@LOCAL` reads fail with
`1238/HY000` and text containing `Variable '<name>' is a GLOBAL variable`.

For dynamic global-only variables, unqualified, `SESSION`, and `LOCAL`
assignment fail with `1229/HY000` and text containing `GLOBAL variable and
should be set with SET GLOBAL`. `SET GLOBAL ... = DEFAULT` succeeds. For
read-only variables, every assignment scope fails with `1238/HY000` and text
containing `read only variable`.

`innodb_lock_wait_timeout` is distinct from `lock_wait_timeout`. MySQL accepts
unqualified, `SESSION`, and `LOCAL` assignments as session changes; `GLOBAL`
assignments change server-global state.

## MyLite Behavior

MyLite exposes fixed placeholder values matching the observed defaults through:

- `SELECT @@variable`
- `SELECT @@GLOBAL.variable`
- `SHOW VARIABLES LIKE ...`
- `SHOW GLOBAL VARIABLES LIKE ...`
- `SHOW SESSION VARIABLES LIKE ...`

`@@SESSION.variable` and `@@LOCAL.variable` return the MySQL-shaped
global-variable diagnostic for every global-only variable in this slice.

For dynamic global-only variables, MyLite accepts `SET GLOBAL variable =
DEFAULT` and exact fixed-value `SET GLOBAL` assignments as no-ops. Alternate
global values are rejected with a MyLite unsupported fixed-no-op diagnostic
instead of mutating shared server-global state.

For `innodb_lock_wait_timeout`, MyLite supports handle-local unqualified,
`SESSION`, and `LOCAL` integer assignments with scalar and `SHOW` readback.
`SET GLOBAL innodb_lock_wait_timeout` accepts only `DEFAULT` and the fixed
default value as no-ops.

For read-only variables, every direct and user-variable assignment path returns
MySQL-shaped read-only diagnostics before value validation.

## Non-Goals

- InnoDB physical IO scheduling, flushing, or LRU scanning behavior.
- InnoDB row-lock wait timeout enforcement.
- Redo-log buffer allocation, checksumming, compression-page logging,
  write-ahead sizing, writer thread scheduling, or redo-log file layout.
- Mutable shared global state, persisted variables, startup option handling,
  privilege checks, Performance Schema variable tables, or cross-connection
  visibility.
- Full validation or mutation semantics for alternate MySQL-accepted global
  runtime values beyond exact/default no-ops.

## SQLite Integration

No SQLite fork or new SQLite extension point is required. This is MyLite runtime
metadata and diagnostics around system-variable reads and assignment syntax.
