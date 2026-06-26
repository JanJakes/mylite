# Baseline InnoDB Storage System Variables

## Scope

This slice adds MySQL 8.4.9-shaped metadata placeholders for these InnoDB
storage, compression, file, DDL, doublewrite, and shutdown system variables:

- `innodb_checksum_algorithm`
- `innodb_cmp_per_index_enabled`
- `innodb_commit_concurrency`
- `innodb_compression_failure_threshold_pct`
- `innodb_compression_level`
- `innodb_compression_pad_pct_max`
- `innodb_concurrency_tickets`
- `innodb_data_file_path`
- `innodb_data_home_dir`
- `innodb_ddl_buffer_size`
- `innodb_ddl_threads`
- `innodb_deadlock_detect`
- `innodb_dedicated_server`
- `innodb_default_row_format`
- `innodb_directories`
- `innodb_disable_sort_file_cache`
- `innodb_doublewrite`
- `innodb_doublewrite_batch_size`
- `innodb_doublewrite_dir`
- `innodb_doublewrite_files`
- `innodb_doublewrite_pages`
- `innodb_extend_and_initialize`
- `innodb_fast_shutdown`

The goal is baseline compatibility for scalar system-variable reads,
`SHOW VARIABLES` rows, scope diagnostics, read-only diagnostics, and exact
fixed-value `SET` no-ops. This does not implement InnoDB storage files,
compression, background flushing, doublewrite behavior, row-format routing,
deadlock detection, DDL buffer sizing, or shutdown behavior.

## MySQL 8.4.9 Observations

The expectation script in this directory's matching test file verifies these
observations against a real MySQL 8.4.9 runtime:

| Variable | Scalar value | SHOW value | Scope | Mutation |
| --- | --- | --- | --- | --- |
| `innodb_checksum_algorithm` | `crc32` | `crc32` | global | dynamic global |
| `innodb_cmp_per_index_enabled` | `0` | `OFF` | global | dynamic global |
| `innodb_commit_concurrency` | `0` | `0` | global | dynamic global |
| `innodb_compression_failure_threshold_pct` | `5` | `5` | global | dynamic global |
| `innodb_compression_level` | `6` | `6` | global | dynamic global |
| `innodb_compression_pad_pct_max` | `50` | `50` | global | dynamic global |
| `innodb_concurrency_tickets` | `5000` | `5000` | global | dynamic global |
| `innodb_data_file_path` | `ibdata1:12M:autoextend` | `ibdata1:12M:autoextend` | global | read-only |
| `innodb_data_home_dir` | SQL `NULL` | empty string | global | read-only |
| `innodb_ddl_buffer_size` | `1048576` | `1048576` | global/session/local | dynamic |
| `innodb_ddl_threads` | `4` | `4` | global/session/local | dynamic |
| `innodb_deadlock_detect` | `1` | `ON` | global | dynamic global |
| `innodb_dedicated_server` | `0` | `OFF` | global | read-only |
| `innodb_default_row_format` | `dynamic` | `dynamic` | global | dynamic global |
| `innodb_directories` | SQL `NULL` | empty string | global | read-only |
| `innodb_disable_sort_file_cache` | `0` | `OFF` | global | dynamic global |
| `innodb_doublewrite` | `ON` | `ON` | global | dynamic global |
| `innodb_doublewrite_batch_size` | `0` | `0` | global | read-only |
| `innodb_doublewrite_dir` | SQL `NULL` | empty string | global | read-only |
| `innodb_doublewrite_files` | `2` | `2` | global | read-only |
| `innodb_doublewrite_pages` | `128` | `128` | global | read-only |
| `innodb_extend_and_initialize` | `1` | `ON` | global | dynamic global |
| `innodb_fast_shutdown` | `1` | `1` | global | dynamic global |

Global-only scalar reads through `@@SESSION` or `@@LOCAL` fail with MySQL error
`1238/HY000` and text containing `Variable '<name>' is a GLOBAL variable`.
Unqualified/session/local assignment to dynamic global-only variables fails with
`1229/HY000` and text containing `GLOBAL variable and should be set with SET
GLOBAL`. Read-only variables fail for all assignment scopes with `1238/HY000`
and text containing `read only variable`.

## MyLite Behavior

MyLite exposes fixed placeholder values matching the observed MySQL 8.4.9
defaults through:

- `SELECT @@variable`
- `SELECT @@GLOBAL.variable`
- `SELECT @@SESSION.variable` / `@@LOCAL.variable` when MySQL allows those
  scopes
- `SHOW VARIABLES LIKE ...`
- `SHOW GLOBAL VARIABLES LIKE ...`
- `SHOW SESSION VARIABLES LIKE ...`

For dynamic global-only variables, MyLite accepts `SET GLOBAL variable =
DEFAULT` and exact fixed-value `SET GLOBAL` assignments as no-ops. Non-global
assignment scopes use MySQL-shaped global-only diagnostics.

For `innodb_ddl_buffer_size` and `innodb_ddl_threads`, MyLite accepts
unqualified, `SESSION`, `LOCAL`, and `GLOBAL` `DEFAULT` or exact fixed-value
assignments as no-ops. The value remains fixed and handle-local mutation state
is not stored because MyLite does not implement the affected InnoDB DDL memory
planner.

For read-only variables, every direct and user-variable assignment path returns
MySQL-shaped read-only diagnostics before value validation.

SQL `NULL` scalar placeholders render as NULL result cells. The corresponding
`SHOW VARIABLES` values render as empty strings, matching MySQL.

## Non-Goals

- Creating or sizing InnoDB data files or directories.
- InnoDB compression, checksum, doublewrite, deadlock, flushing, DDL planner,
  row-format, or shutdown side effects.
- Mutable global state, persisted variables, startup option handling, privilege
  checks, Performance Schema variable tables, or cross-connection visibility.
- Full validation of alternate accepted MySQL values beyond exact fixed
  no-ops.

## SQLite Integration

No SQLite fork or new SQLite extension point is required. This is MyLite runtime
metadata and diagnostics around system-variable reads and assignment syntax.
