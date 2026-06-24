# Baseline sys user summary views

## Scope

This slice adds MySQL 8.4.9-shaped metadata placeholders for these sys schema
views:

- `sys.user_summary`
- `sys.x$user_summary`
- `sys.user_summary_by_file_io`
- `sys.x$user_summary_by_file_io`
- `sys.user_summary_by_file_io_type`
- `sys.x$user_summary_by_file_io_type`
- `sys.user_summary_by_stages`
- `sys.x$user_summary_by_stages`
- `sys.user_summary_by_statement_latency`
- `sys.x$user_summary_by_statement_latency`
- `sys.user_summary_by_statement_type`
- `sys.x$user_summary_by_statement_type`

The views are documented by the MySQL 8.4 Reference Manual sys schema view
pages for user activity, file I/O, stages, and statement summaries. MyLite uses
the manual as feature-surface context and MySQL 8.4.9 runtime probes as the
source of truth for column descriptors, view metadata, dependency metadata, and
`SHOW CREATE VIEW` text.

## MyLite behavior

MyLite exposes these views as read-only built-in sys views with no rows. This is
intentional for the baseline because MyLite does not maintain Performance Schema
per-user instrumentation tables. Empty result sets match the embedded
placeholder strategy already used for related sys summary views and avoid
inventing server activity that does not exist.

The following metadata surfaces must work for every view in scope:

- `SELECT COUNT(*) FROM sys.<view>` returns `0`.
- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and `DESC` expose
  MySQL 8.4.9-shaped columns.
- `INFORMATION_SCHEMA.COLUMNS` exposes matching data type, nullability, default,
  character set, collation, numeric precision, scale, privileges, and ordinal
  metadata.
- `INFORMATION_SCHEMA.TABLES` and `SHOW TABLE STATUS` expose view rows with
  MySQL-shaped status fields.
- `INFORMATION_SCHEMA.VIEWS` exposes `CHECK_OPTION`, `IS_UPDATABLE`, `DEFINER`,
  `SECURITY_TYPE`, client character set, and connection collation metadata.
- `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` exposes the observed underlying
  `performance_schema` and `sys` dependencies.
- `INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` returns no rows for this family.
- `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`,
  `KEY_COLUMN_USAGE`, and `TABLE_CONSTRAINTS_EXTENSIONS` return no rows.
- `SHOW CREATE VIEW` and `SHOW CREATE TABLE` expose MySQL-shaped view
  definitions, including selected-schema and qualified-name forms.

## Runtime-verified metadata

MySQL 8.4.9 reports these column counts:

| View family | Formatted view columns | Raw `x$` view columns |
| --- | ---: | ---: |
| `user_summary` | 12 | 12 |
| `user_summary_by_file_io` | 3 | 3 |
| `user_summary_by_file_io_type` | 5 | 5 |
| `user_summary_by_stages` | 5 | 5 |
| `user_summary_by_statement_latency` | 10 | 10 |
| `user_summary_by_statement_type` | 11 | 11 |

Important user-specific differences from the already-supported host summary
family:

- The grouping column is `user varchar(32)` with `utf8mb4_bin` collation.
- `user_summary` reports `unique_hosts`, not `unique_users`.
- `x$user_summary.statement_avg_latency` is `decimal(65,4) NOT NULL DEFAULT
  0.0000`.
- `user_summary_by_file_io_type` uses a `latency` column, not
  `total_latency`.
- The statement-latency raw view uses `decimal(42,0)` for `max_latency`.

Observed `INFORMATION_SCHEMA.VIEWS.IS_UPDATABLE` is `YES` only for:

- `user_summary_by_file_io_type`
- `x$user_summary_by_file_io_type`
- `user_summary_by_stages`
- `x$user_summary_by_stages`
- `user_summary_by_statement_type`
- `x$user_summary_by_statement_type`

All other views in this slice report `NO`.

Observed table dependencies:

- `user_summary` and `x$user_summary`: `performance_schema.accounts`,
  `sys.x$memory_by_user_by_current_bytes`, `sys.x$user_summary_by_file_io`,
  and `sys.x$user_summary_by_statement_latency`.
- `user_summary_by_file_io` and `x$user_summary_by_file_io`:
  `performance_schema.events_waits_summary_by_user_by_event_name`.
- `user_summary_by_file_io_type` and `x$user_summary_by_file_io_type`:
  `performance_schema.events_waits_summary_by_user_by_event_name`.
- `user_summary_by_stages` and `x$user_summary_by_stages`:
  `performance_schema.events_stages_summary_by_user_by_event_name`.
- `user_summary_by_statement_latency` and
  `x$user_summary_by_statement_latency`:
  `performance_schema.events_statements_summary_by_user_by_event_name`.
- `user_summary_by_statement_type` and `x$user_summary_by_statement_type`:
  `performance_schema.events_statements_summary_by_user_by_event_name`.

## Storage and execution

This slice has no `.mylite` file format impact and requires no SQLite fork
hook. It is implemented through MyLite-owned catalog descriptors, static
metadata row synthesis, and empty sys-view row dispatch. No SQLite virtual table
or public extension API is needed because the baseline returns empty result
sets.

## Known incompatibilities

The following remain explicitly unsupported:

- Live Performance Schema aggregation by user.
- Populated rows for foreground, background, or historical users.
- Privilege filtering of sys schema metadata.
- Executable view definitions over real `performance_schema` tables.
- Server instrumentation toggles, timing fidelity, and sys helper routine
  side effects beyond metadata dependency reporting.

## Test plan

`packages/libmylite/tests/mysql_baseline_sys_user_summary_views_expectations.sh`
records MySQL 8.4.9 expectations for the metadata surfaces above.

`packages/libmylite/tests/runtime_sys_user_summary_views_test.c` verifies the
same supported MyLite surfaces, including row emptiness, descriptor counts,
updatability, dependency rows, selected-schema access, and representative
`SHOW CREATE` text.
