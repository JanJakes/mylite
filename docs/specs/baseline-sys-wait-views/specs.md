# Baseline sys wait views

## Scope

This slice adds MySQL 8.4.9-shaped metadata placeholders for these sys schema
views:

- `sys.wait_classes_global_by_avg_latency`
- `sys.x$wait_classes_global_by_avg_latency`
- `sys.wait_classes_global_by_latency`
- `sys.x$wait_classes_global_by_latency`
- `sys.waits_by_host_by_latency`
- `sys.x$waits_by_host_by_latency`
- `sys.waits_by_user_by_latency`
- `sys.x$waits_by_user_by_latency`
- `sys.waits_global_by_latency`
- `sys.x$waits_global_by_latency`

The MySQL 8.4 Reference Manual describes these views as summaries over
Performance Schema wait instrumentation. MyLite uses those pages as feature
surface context and MySQL 8.4.9 runtime probes as the source of truth for
column descriptors, view metadata, dependencies, and `SHOW CREATE VIEW` text.

## MyLite behavior

MyLite exposes the views as read-only built-in sys views with no rows. This
matches the existing MyLite sys placeholder strategy: the metadata surface is
useful to applications, but MyLite does not maintain the Performance Schema
wait-summary tables needed to synthesize live wait statistics.

The following metadata surfaces must work for every view in scope:

- `SELECT COUNT(*) FROM sys.<view>` returns `0`.
- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and `DESC` expose
  MySQL 8.4.9-shaped columns.
- `INFORMATION_SCHEMA.COLUMNS` exposes matching type, nullability, default,
  character set, collation, precision, scale, ordinal, and privilege metadata.
- `INFORMATION_SCHEMA.TABLES` and `SHOW TABLE STATUS` expose view rows with
  MySQL-shaped status fields.
- `INFORMATION_SCHEMA.VIEWS` exposes `CHECK_OPTION`, `IS_UPDATABLE`, `DEFINER`,
  `SECURITY_TYPE`, client character set, and connection collation metadata.
- `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` exposes the observed underlying
  `performance_schema` dependencies.
- `INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` returns no rows for this family.
- `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`,
  `KEY_COLUMN_USAGE`, and `TABLE_CONSTRAINTS_EXTENSIONS` return no rows.
- `SHOW CREATE VIEW` and `SHOW CREATE TABLE` expose MySQL-shaped view
  definitions, including selected-schema and qualified-name forms.

## Runtime-verified metadata

MySQL 8.4.9 reports six columns for every view except
`waits_global_by_latency` and `x$waits_global_by_latency`, which report five:

| View family | Formatted columns | Raw `x$` columns |
| --- | ---: | ---: |
| `wait_classes_global_by_avg_latency` | 6 | 6 |
| `wait_classes_global_by_latency` | 6 | 6 |
| `waits_by_host_by_latency` | 6 | 6 |
| `waits_by_user_by_latency` | 6 | 6 |
| `waits_global_by_latency` | 5 | 5 |

Important column details:

- Wait-class views group by `event_class varchar(128)` and expose formatted
  latency text in the non-`x$` forms.
- Raw wait-class views use `decimal(42,0)` for `total` and `total_latency`,
  `bigint unsigned` for `min_latency` and `max_latency`, and
  `decimal(46,4) NOT NULL DEFAULT 0.0000` for `avg_latency`.
- Host wait views use `host varchar(255)` with the `ascii` character set and
  `ascii_general_ci` collation.
- User wait views use `user varchar(32)` with `utf8mb4_bin` collation.
- Global wait views name the event column `events`, while the underlying view
  definition aliases the selected expression as `event`.

Observed `INFORMATION_SCHEMA.VIEWS.IS_UPDATABLE` is `YES` for:

- `waits_by_host_by_latency`
- `x$waits_by_host_by_latency`
- `waits_by_user_by_latency`
- `x$waits_by_user_by_latency`
- `waits_global_by_latency`
- `x$waits_global_by_latency`

The two wait-class families report `NO`.

Observed table dependencies:

- Wait-class and global-wait views depend on
  `performance_schema.events_waits_summary_global_by_event_name`.
- Host wait views depend on
  `performance_schema.events_waits_summary_by_host_by_event_name`.
- User wait views depend on
  `performance_schema.events_waits_summary_by_user_by_event_name`.

## Storage and execution

This slice has no `.mylite` file format impact and requires no SQLite fork
hook. It is implemented through MyLite-owned catalog descriptors, static
metadata row synthesis, and empty sys-view row dispatch. No SQLite virtual table
or public extension API is needed because the baseline returns empty result
sets.

## Known incompatibilities

The following remain explicitly unsupported:

- Live Performance Schema wait collection or aggregation.
- Populated rows for host, user, global, or class wait statistics.
- Privilege filtering of sys schema metadata.
- Executable view definitions over real `performance_schema` wait tables.
- Server instrumentation toggles, timing fidelity, and sys helper routine
  behavior beyond metadata dependency reporting.

## Test plan

`packages/libmylite/tests/mysql_baseline_sys_wait_views_expectations.sh`
records MySQL 8.4.9 expectations for the metadata surfaces above.

`packages/libmylite/tests/runtime_sys_wait_views_test.c` verifies the same
supported MyLite surfaces, including row emptiness, descriptor counts,
updatability, dependency rows, selected-schema access, and representative
`SHOW CREATE` text.
