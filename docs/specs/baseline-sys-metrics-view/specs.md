# Baseline sys.metrics View

This slice adds MySQL-shaped metadata and a deterministic partial rowset for
the `sys.metrics` view. MySQL uses this sys view to union global status rows,
selected InnoDB metrics, Performance Schema memory totals, and current system
time values. MyLite does not yet collect live InnoDB or Performance Schema
instrumentation, but it already has a supported embedded global status
descriptor set. `sys.metrics` therefore exposes those supported status
descriptors as `Global Status` rows and documents the remaining live
instrumentation rows as gaps.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.metrics`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-metrics.html>
- MySQL 8.4 Reference Manual, Performance Schema status variable tables:
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-status-variable-tables.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_metrics_view_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified column metadata, live row classes, view definition metadata, table
dependency metadata, routine dependency absence, index/constraint absence,
selected-schema behavior, `SHOW CREATE` rendering, `SHOW TABLE STATUS`, and
read status behavior. Direct MySQL row values are environment-dependent because
the view reflects live server status, InnoDB metrics, memory instrumentation,
and wall-clock time.

## Supported Behavior

Supported direct reads:

```sql
SELECT * FROM sys.metrics;

USE sys;
SELECT * FROM metrics;
```

MyLite returns one row per supported global status descriptor. The current
descriptor set produces 297 rows: all global-visible status placeholders from
the `SHOW STATUS` registry, excluding session-only `Compression`. Each row uses:

| Column | Value |
| --- | --- |
| `Variable_name` | Lowercase status-variable name |
| `Variable_value` | MyLite's descriptor value |
| `Type` | `Global Status` |
| `Enabled` | `YES` |

Session-only status descriptors are excluded. For example, `Compression` is
available through session `SHOW STATUS`, but is not exposed by
`SHOW GLOBAL STATUS` and is not returned by `sys.metrics`.

## Column Metadata

`sys.metrics` has four columns:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `Variable_name` | `varchar(193)` | `NO` | empty | `utf8mb4_0900_ai_ci` |
| `Variable_value` | `text` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `Type` | `varchar(210)` | `NO` | empty | `utf8mb3_general_ci` |
| `Enabled` | `varchar(7)` | `NO` | empty | `utf8mb4_0900_ai_ci` |

`SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and
`INFORMATION_SCHEMA.COLUMNS` expose this shape. The view has no index or
constraint metadata, so `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`,
`TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
`TABLE_CONSTRAINTS_EXTENSIONS` return zero rows.

## View Metadata

`INFORMATION_SCHEMA.VIEWS` exposes the built-in row with:

- `TABLE_CATALOG = 'def'`
- `TABLE_SCHEMA = 'sys'`
- `CHECK_OPTION = 'NONE'`
- `IS_UPDATABLE = 'NO'`
- `DEFINER = 'mysql.sys@localhost'`
- `SECURITY_TYPE = 'INVOKER'`
- `CHARACTER_SET_CLIENT = 'utf8mb4'`
- `COLLATION_CONNECTION = 'utf8mb4_0900_ai_ci'`

`SHOW CREATE VIEW` and `SHOW CREATE TABLE` return MySQL-shaped metadata with
`ALGORITHM=TEMPTABLE` and the `mysql.sys@localhost` definer.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports the MySQL 8.4.9 dependency
surface:

- `information_schema.INNODB_METRICS`
- `performance_schema.global_status`
- `performance_schema.memory_summary_global_by_event_name`
- `performance_schema.setup_instruments`

`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` returns no rows for `sys.metrics`.

## Unsupported Behavior

This slice intentionally does not implement:

- full `performance_schema.global_status` coverage beyond supported MyLite
  descriptors;
- live `INFORMATION_SCHEMA.INNODB_METRICS` counts or status filtering;
- live Performance Schema memory-summary rows;
- setup-instrument enabled-state aggregation;
- `NOW(3)` / `UNIX_TIMESTAMP()` system-time metric rows;
- execution of the stored MySQL view definition through physical views;
- privilege filtering, definer validation, SQL SECURITY enforcement, or true
  updatable-view writes;
- broader sys view execution.

Writes to the view remain blocked by the existing built-in schema write guard.

## Parser And Grammar

No Lemon grammar changes are required. Existing qualified and selected-schema
table references, `SHOW COLUMNS`, `SHOW INDEX`, `SHOW CREATE VIEW`,
`SHOW CREATE TABLE`, and `INFORMATION_SCHEMA` query support are sufficient.

## Architecture

- Public API: unchanged.
- Parser/AST: unchanged.
- Runtime metadata: extends the synthetic sys core descriptor table with one
  view entry.
- Query execution: reuses the existing synthetic system-table SELECT planner
  and builds rows from MyLite's global status descriptor registry.
- Information schema: adds `COLUMNS`, `TABLES`, `VIEWS`, and
  `VIEW_TABLE_USAGE` rows plus empty routine, index, and constraint metadata
  through existing synthetic metadata builders.
- SHOW metadata: reuses the synthetic system-table column/index paths and the
  built-in sys view `SHOW CREATE` short-circuit.
- Storage/SQLite: unchanged. No SQLite extension API or fork hook is required.

## Performance

The rowset is built from a small static descriptor array and does not scan
SQLite user data or collect live instrumentation.

## Tests

MySQL 8.4.9 expectation coverage:

- column metadata;
- live row-class presence without depending on environment-specific values;
- view and table-dependency metadata and absence of routine dependencies;
- empty index and constraint metadata;
- qualified and selected-schema `SHOW CREATE VIEW` / `SHOW CREATE TABLE`;
- `SHOW TABLE STATUS` metadata;
- `ROW_COUNT() = -1` and zero warnings after reads.

MyLite runtime coverage mirrors the supported partial rowset, selected-schema
access, and metadata surfaces for the view.
