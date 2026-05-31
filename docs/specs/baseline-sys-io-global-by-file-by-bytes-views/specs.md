# Baseline sys.io_global_by_file_by_bytes Views

This slice adds MySQL-shaped metadata and deterministic read-only empty rows for
the `sys.io_global_by_file_by_bytes` and
`sys.x$io_global_by_file_by_bytes` views. MySQL uses these sys views to summarize
Performance Schema file-summary counters by file. MyLite does not collect
Performance Schema file I/O summaries, so both views are metadata-complete empty
placeholders.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.io_global_by_file_by_bytes` and
  `sys.x$io_global_by_file_by_bytes`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-io-global-by-file-by-bytes.html>
- MySQL 8.4 Reference Manual, Performance Schema file I/O summary tables:
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-file-summary-tables.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_io_global_by_file_by_bytes_views_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified column metadata, view metadata, table and routine dependency metadata,
index/constraint absence, selected-schema access, `SHOW CREATE` rendering,
`SHOW TABLE STATUS`, and read status behavior. Direct MySQL row values are
environment-dependent because they reflect live file-summary counters.

## Supported Behavior

Supported direct reads:

```sql
SELECT * FROM sys.io_global_by_file_by_bytes;
SELECT * FROM sys.`x$io_global_by_file_by_bytes`;

USE sys;
SELECT * FROM io_global_by_file_by_bytes;
SELECT * FROM `x$io_global_by_file_by_bytes`;
```

Both views return zero rows. This differs from a live MySQL server that has
Performance Schema file-summary counters, but preserves the queryable sys view
surface without inventing file paths or I/O counters.

## Column Metadata

`sys.io_global_by_file_by_bytes` has nine columns:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `file` | `varchar(512)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `count_read` | `bigint unsigned` | `NO` | `NULL` | SQL `NULL` |
| `total_read` | `varchar(11)` | `YES` | `NULL` | `utf8mb3_general_ci` |
| `avg_read` | `varchar(11)` | `YES` | `NULL` | `utf8mb3_general_ci` |
| `count_write` | `bigint unsigned` | `NO` | `NULL` | SQL `NULL` |
| `total_written` | `varchar(11)` | `YES` | `NULL` | `utf8mb3_general_ci` |
| `avg_write` | `varchar(11)` | `YES` | `NULL` | `utf8mb3_general_ci` |
| `total` | `varchar(11)` | `YES` | `NULL` | `utf8mb3_general_ci` |
| `write_pct` | `decimal(26,2)` | `NO` | `0.00` | SQL `NULL` |

`sys.x$io_global_by_file_by_bytes` has the same column names with raw numeric
metadata:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `file` | `varchar(512)` | `NO` | `NULL` | `utf8mb4_0900_ai_ci` |
| `count_read` | `bigint unsigned` | `NO` | `NULL` | SQL `NULL` |
| `total_read` | `bigint` | `NO` | `NULL` | SQL `NULL` |
| `avg_read` | `decimal(23,4)` | `NO` | `0.0000` | SQL `NULL` |
| `count_write` | `bigint unsigned` | `NO` | `NULL` | SQL `NULL` |
| `total_written` | `bigint` | `NO` | `NULL` | SQL `NULL` |
| `avg_write` | `decimal(23,4)` | `NO` | `0.0000` | SQL `NULL` |
| `total` | `bigint` | `NO` | `0` | SQL `NULL` |
| `write_pct` | `decimal(26,2)` | `NO` | `0.00` | SQL `NULL` |

`SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and
`INFORMATION_SCHEMA.COLUMNS` expose this shape. The views have no index or
constraint metadata, so `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`,
`TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
`TABLE_CONSTRAINTS_EXTENSIONS` return zero rows.

## View Metadata

`INFORMATION_SCHEMA.VIEWS` exposes both built-in rows with:

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
uses `sys.format_path()` and `format_bytes()` over
`performance_schema.file_summary_by_instance`; the raw `x$` view exposes raw
Performance Schema byte counters and averages.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports:

- `sys.io_global_by_file_by_bytes` depends on
  `performance_schema.file_summary_by_instance` and
  `performance_schema.global_variables`;
- `sys.x$io_global_by_file_by_bytes` depends on
  `performance_schema.file_summary_by_instance`.

`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` reports a dependency from
`sys.io_global_by_file_by_bytes` to `sys.format_path`.

## Unsupported Behavior

This slice intentionally does not implement:

- Performance Schema file summary collection;
- live file path, byte, average, or write percentage rows;
- execution of `sys.format_path()` or `format_bytes()`;
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
  two `sys` view entries.
- Query execution: reuses the existing synthetic system-table SELECT planner
  and returns no rows after validating the expected column count.
- Information schema: adds `COLUMNS`, `TABLES`, `VIEWS`,
  `VIEW_TABLE_USAGE`, and `VIEW_ROUTINE_USAGE` rows plus empty index and
  constraint metadata through existing synthetic metadata builders.
- SHOW metadata: reuses the synthetic system-table column/index paths and the
  built-in sys view `SHOW CREATE` short-circuit.
- Storage/SQLite: unchanged. No SQLite extension API or fork hook is required.

## Performance

Both views return empty rows without scanning SQLite data tables or collecting
file-I/O counters.

## Tests

MySQL 8.4.9 expectation coverage:

- column metadata for formatted and raw views;
- selected-schema access and live-row presence without depending on variable
  counter values;
- view, table-dependency, and routine-dependency metadata;
- empty index and constraint metadata for both view objects;
- qualified and selected-schema `SHOW CREATE VIEW` / `SHOW CREATE TABLE`;
- `SHOW TABLE STATUS` metadata;
- `ROW_COUNT() = -1` and zero warnings after reads.

MyLite runtime coverage mirrors the supported empty placeholder rows,
selected-schema access, and metadata surfaces for both views.
