# Baseline sys.schema_tables_with_full_table_scans Views

This slice adds MySQL-shaped metadata and deterministic empty read-only rows for
`sys.schema_tables_with_full_table_scans` and
`sys.x$schema_tables_with_full_table_scans`. MyLite does not collect
Performance Schema table I/O wait summaries by index usage, so the views expose
the MySQL 8.4.9 catalog surface while returning no full-scan rows.

## Compatibility Authority

- MySQL 8.4 Reference Manual,
  `sys.schema_tables_with_full_table_scans` and
  `sys.x$schema_tables_with_full_table_scans`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-schema-tables-with-full-table-scans.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_schema_tables_with_full_table_scans_views_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified column metadata, user-table row behavior after a full table scan, view
definition metadata, table dependency metadata, empty routine dependency
metadata, selected-schema access, and SHOW metadata.

## Supported Behavior

Supported direct reads:

```sql
SELECT * FROM sys.schema_tables_with_full_table_scans;
SELECT * FROM sys.`x$schema_tables_with_full_table_scans`;

USE sys;
SELECT COUNT(*) FROM schema_tables_with_full_table_scans;
SELECT COUNT(*) FROM `x$schema_tables_with_full_table_scans`;
```

Both views return zero rows. MySQL populates these views from
`performance_schema.table_io_waits_summary_by_index_usage` rows where
`INDEX_NAME IS NULL` and `COUNT_READ > 0`. MyLite currently has no Performance
Schema wait-summary collection and no full-table-scan counter, so an empty
placeholder is the closest deterministic embedded behavior.

## Column Metadata

Both views have four columns:

| Column | Formatted view type | x$ view type | Null | Default | Collation |
| --- | --- | --- | --- | --- | --- |
| `object_schema` | `varchar(64)` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `object_name` | `varchar(64)` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `rows_full_scanned` | `bigint unsigned` | `bigint unsigned` | `NO` | `NULL` | SQL `NULL` |
| `latency` | `varchar(11)` | `bigint unsigned` | formatted: `YES`; x$: `NO` | `NULL` | formatted: `utf8mb3_general_ci`; x$: SQL `NULL` |

`SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and
`INFORMATION_SCHEMA.COLUMNS` expose this shape. The views have no indexes or
constraints, so `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`,
`TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
`TABLE_CONSTRAINTS_EXTENSIONS` return zero rows for the view objects.

## View Metadata

`INFORMATION_SCHEMA.VIEWS` exposes built-in rows for both views with:

- `TABLE_CATALOG = 'def'`
- `TABLE_SCHEMA = 'sys'`
- `CHECK_OPTION = 'NONE'`
- `IS_UPDATABLE = 'YES'`
- `DEFINER = 'mysql.sys@localhost'`
- `SECURITY_TYPE = 'INVOKER'`
- `CHARACTER_SET_CLIENT = 'utf8mb4'`
- `COLLATION_CONNECTION = 'utf8mb4_0900_ai_ci'`

`SHOW CREATE VIEW` and `SHOW CREATE TABLE` return MySQL-shaped metadata with
`ALGORITHM=MERGE` and the `mysql.sys@localhost` definer. The formatted view
definition wraps `SUM_TIMER_WAIT` with `format_pico_time()`. The raw `x$` view
exposes `SUM_TIMER_WAIT` directly.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports one dependency from each view to
`performance_schema.table_io_waits_summary_by_index_usage`.

`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` reports zero rows.

## Unsupported Behavior

This slice intentionally does not implement:

- Performance Schema table I/O wait collection by index usage;
- live detection of full table scans;
- real row-production based on `INDEX_NAME IS NULL` and `COUNT_READ > 0`;
- privilege filtering, definer validation, or SQL SECURITY enforcement;
- physical SQLite views or persisted catalog descriptors for built-in sys
  views;
- broader sys view execution.

Writes to the views remain blocked by the existing built-in schema write guard.

## Parser And Grammar

No Lemon grammar changes are required. Existing qualified and selected-schema
table references, quoted identifiers for `x$` view names, `SHOW COLUMNS`,
`SHOW INDEX`, `SHOW CREATE VIEW`, `SHOW CREATE TABLE`, and
`INFORMATION_SCHEMA` query support are sufficient.

## Architecture

- Public API: unchanged.
- Parser/AST: unchanged.
- Runtime metadata: extends the synthetic system-table descriptor table with
  `sys.schema_tables_with_full_table_scans` and
  `sys.x$schema_tables_with_full_table_scans` view entries.
- Query execution: reuses the existing synthetic system-table SELECT planner
  and returns empty row sets for both views.
- Information schema: adds `COLUMNS`, `TABLES`, `VIEWS`, and
  `VIEW_TABLE_USAGE` rows plus empty index, constraint, and routine-dependency
  metadata through existing synthetic metadata builders.
- SHOW metadata: reuses the synthetic system-table column/index paths and the
  built-in sys view `SHOW CREATE` short-circuit.
- Storage/SQLite: unchanged. No SQLite extension API or fork hook is required.

## Performance

Reads of these views produce no rows and do not scan user data or SQLite
storage. Metadata queries use the existing static descriptor arrays.

## Tests

MySQL 8.4.9 expectation coverage:

- formatted and raw column metadata;
- user-schema row behavior after a full table scan;
- empty direct reads through MyLite's placeholder behavior;
- view and table-dependency metadata;
- empty routine-dependency, index, and constraint metadata for the view
  objects;
- qualified and selected-schema `SHOW CREATE VIEW` / `SHOW CREATE TABLE`;
- `ROW_COUNT() = -1` and zero warnings after a sys view read.

MyLite runtime coverage mirrors the supported metadata and empty row behavior.
