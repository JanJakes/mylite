# Baseline InnoDB Dirty-Page and Purge System Variables

## Scope

This slice adds MySQL 8.4.9-shaped metadata placeholders for these InnoDB
dirty-page, purge-lag, and undo tablespace threshold system variables:

- `innodb_max_dirty_pages_pct`
- `innodb_max_dirty_pages_pct_lwm`
- `innodb_max_purge_lag`
- `innodb_max_purge_lag_delay`
- `innodb_max_undo_log_size`

The goal is baseline compatibility for scalar reads, `SHOW VARIABLES` rows,
scope diagnostics, and fixed/default global no-op assignments. This does not
implement buffer-pool flushing, purge throttling, undo tablespace truncation,
mutable shared global state, or runtime effects on DML scheduling.

## MySQL 8.4.9 Observations

The MySQL 8.4 Reference Manual documents these variables as global dynamic
InnoDB system variables. Runtime probes against MySQL 8.4.9 showed these
defaults and display forms:

| Variable | Scalar value | SHOW value | Scope | Mutation |
| --- | --- | --- | --- | --- |
| `innodb_max_dirty_pages_pct` | `90.000000` | `90.000000` | global | dynamic global |
| `innodb_max_dirty_pages_pct_lwm` | `10.000000` | `10.000000` | global | dynamic global |
| `innodb_max_purge_lag` | `0` | `0` | global | dynamic global |
| `innodb_max_purge_lag_delay` | `0` | `0` | global | dynamic global |
| `innodb_max_undo_log_size` | `1073741824` | `1073741824` | global | dynamic global |

All variables appear in default, global, and session `SHOW VARIABLES` output.
For scalar reads, `@@variable` and `@@GLOBAL.variable` return the global value.
`@@SESSION.variable` and `@@LOCAL.variable` fail with `1238/HY000` and text
containing `Variable '<name>' is a GLOBAL variable`.

Unqualified, `SESSION`, and `LOCAL` assignment fail with `1229/HY000` and text
containing `GLOBAL variable and should be set with SET GLOBAL`. `SET GLOBAL
... = DEFAULT` succeeds. MySQL also accepts direct global assignments to
alternate in-range values, but those mutate server-global state.

## MyLite Behavior

MyLite exposes fixed placeholder values matching the observed defaults through:

- `SELECT @@variable`
- `SELECT @@GLOBAL.variable`
- `SHOW VARIABLES LIKE ...`
- `SHOW GLOBAL VARIABLES LIKE ...`
- `SHOW SESSION VARIABLES LIKE ...`

`@@SESSION.variable` and `@@LOCAL.variable` return MySQL-shaped global-variable
diagnostics for every variable in this slice.

MyLite accepts `SET GLOBAL variable = DEFAULT` and exact fixed-value `SET
GLOBAL` assignments as no-ops. For `innodb_max_dirty_pages_pct` and
`innodb_max_dirty_pages_pct_lwm`, the exact no-op assignment accepts the
integer spelling (`90`, `10`) and decimal spellings that represent the same
fixed value with only zero fractional digits. Alternate values are rejected
with MyLite's fixed no-op unsupported diagnostic instead of mutating shared
server-global state.

## Non-Goals

- Buffer-pool dirty-page tracking or flushing behavior.
- Purge lag measurement, DML throttling, or delay enforcement.
- Undo tablespace size tracking, truncation, or file management.
- Mutable shared global state, persisted variables, startup option handling,
  privilege checks, Performance Schema variable tables, or cross-connection
  visibility.
- Full validation or mutation semantics for alternate MySQL-accepted global
  runtime values beyond exact/default no-ops.

## SQLite Integration

No SQLite fork or new SQLite extension point is required. This is MyLite
runtime metadata and diagnostics around system-variable reads and assignment
syntax.

## References

- MySQL 8.4 Reference Manual, InnoDB Startup Options and System Variables:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-parameters.html
