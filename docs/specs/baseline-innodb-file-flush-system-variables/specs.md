# Baseline InnoDB File and Flush System Variables

## Scope

This slice adds MySQL 8.4.9-shaped metadata placeholders for these InnoDB
file-per-table, flush, recovery, and fsync-threshold system variables:

- `innodb_file_per_table`
- `innodb_fill_factor`
- `innodb_flush_log_at_timeout`
- `innodb_flush_log_at_trx_commit`
- `innodb_flush_method`
- `innodb_flush_neighbors`
- `innodb_flush_sync`
- `innodb_flushing_avg_loops`
- `innodb_force_load_corrupted`
- `innodb_force_recovery`
- `innodb_fsync_threshold`

The goal is baseline compatibility for scalar reads, `SHOW VARIABLES` rows,
scope diagnostics, read-only diagnostics, and exact fixed-value `SET` no-ops
where MySQL permits runtime global assignment. This does not implement InnoDB
tablespace file placement, redo-log flushing, recovery modes, neighbor page
flushing, page-cleaner heuristics, or fsync scheduling.

## MySQL 8.4.9 Observations

Runtime probes against MySQL 8.4.9 showed these defaults and scopes:

| Variable | Scalar value | SHOW value | Scope | Mutation |
| --- | --- | --- | --- | --- |
| `innodb_file_per_table` | `1` | `ON` | global | dynamic global |
| `innodb_fill_factor` | `100` | `100` | global | dynamic global |
| `innodb_flush_log_at_timeout` | `1` | `1` | global | dynamic global |
| `innodb_flush_log_at_trx_commit` | `1` | `1` | global | dynamic global |
| `innodb_flush_method` | `O_DIRECT` | `O_DIRECT` | global | read-only |
| `innodb_flush_neighbors` | `0` | `0` | global | dynamic global |
| `innodb_flush_sync` | `1` | `ON` | global | dynamic global |
| `innodb_flushing_avg_loops` | `30` | `30` | global | dynamic global |
| `innodb_force_load_corrupted` | `0` | `OFF` | global | read-only |
| `innodb_force_recovery` | `0` | `0` | global | read-only |
| `innodb_fsync_threshold` | `0` | `0` | global | dynamic global |

All variables appear in default, global, and session `SHOW VARIABLES` output.
Scalar reads through `@@SESSION` or `@@LOCAL` fail with MySQL error
`1238/HY000` and text containing `Variable '<name>' is a GLOBAL variable`.

For dynamic global-only variables, unqualified, `SESSION`, and `LOCAL`
assignment fail with `1229/HY000` and text containing `GLOBAL variable and
should be set with SET GLOBAL`. `SET GLOBAL variable = DEFAULT` and assigning
the observed default value succeeds. For read-only variables, every assignment
scope fails with `1238/HY000` and text containing `read only variable`.

## MyLite Behavior

MyLite exposes fixed placeholder values matching the observed defaults through:

- `SELECT @@variable`
- `SELECT @@GLOBAL.variable`
- `SHOW VARIABLES LIKE ...`
- `SHOW GLOBAL VARIABLES LIKE ...`
- `SHOW SESSION VARIABLES LIKE ...`

`@@SESSION.variable` and `@@LOCAL.variable` return the MySQL-shaped
global-variable diagnostic for all variables in this slice.

For dynamic global-only variables, MyLite accepts `SET GLOBAL variable =
DEFAULT` and exact fixed-value `SET GLOBAL` assignments as no-ops. Non-global
assignment scopes use MySQL-shaped global-only diagnostics. MyLite does not
store mutable global state for these variables because the embedded engine does
not implement the affected InnoDB file or flush subsystems.

For read-only variables, every direct and user-variable assignment path returns
MySQL-shaped read-only diagnostics before value validation.

## Non-Goals

- File-per-table tablespace routing or `.ibd` file placement.
- Redo-log, fsync, page-flushing, page-cleaner, or neighbor-flush behavior.
- Recovery-mode startup effects, forced loading of corrupt pages, or write
  blocking caused by recovery options.
- Mutable shared global state, persisted variables, startup option handling,
  privilege checks, Performance Schema variable tables, or cross-connection
  visibility.
- Full validation or mutation semantics for alternate MySQL-accepted runtime
  values beyond exact fixed no-ops.

## SQLite Integration

No SQLite fork or new SQLite extension point is required. This is MyLite runtime
metadata and diagnostics around system-variable reads and assignment syntax.
