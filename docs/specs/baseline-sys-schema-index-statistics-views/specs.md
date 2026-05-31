# Baseline sys.schema_index_statistics Views

This slice adds MySQL-shaped metadata and deterministic read-only rows for
`sys.schema_index_statistics` and `sys.x$schema_index_statistics`. MyLite does
not collect Performance Schema wait timers, so the views expose descriptor
index inventory with zero counters and zero latency placeholders.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.schema_index_statistics` and
  `sys.x$schema_index_statistics`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-schema-index-statistics.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_schema_index_statistics_views_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified column metadata, user-table index rows, view metadata, dependency
metadata, and selected-schema access.

## Supported Behavior

Supported direct reads:

```sql
SELECT * FROM sys.schema_index_statistics;
SELECT * FROM sys.x$schema_index_statistics;

USE sys;
SELECT table_schema, table_name, index_name FROM schema_index_statistics;
SELECT table_schema, table_name, index_name FROM `x$schema_index_statistics`;
```

MyLite emits one row per known descriptor index for:

- supported mysql/sys system-table primary and secondary index descriptors;
- persistent user base-table primary, unique, nonunique, full-text, and spatial
  index descriptors.

Temporary-table indexes, unsupported Performance Schema indexes, and unsupported
system-table descriptors are omitted. Every emitted row reports zero counters
because MyLite does not currently maintain Performance Schema wait summaries.

`sys.schema_index_statistics` formats each latency column as the MySQL
zero-time string `  0 ps`. `sys.x$schema_index_statistics` exposes the same
latency fields as raw unsigned integer `0` text.

## Column Metadata

Both views have eleven columns:

| Column | Formatted view type | x$ view type | Null | Default | Collation |
| --- | --- | --- | --- | --- | --- |
| `table_schema` | `varchar(64)` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `table_name` | `varchar(64)` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `index_name` | `varchar(64)` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `rows_selected` | `bigint unsigned` | `bigint unsigned` | `NO` | `NULL` | SQL `NULL` |
| `select_latency` | `varchar(11)` | `bigint unsigned` | formatted: `YES`; x$: `NO` | `NULL` | formatted: `utf8mb3_general_ci`; x$: SQL `NULL` |
| `rows_inserted` | `bigint unsigned` | `bigint unsigned` | `NO` | `NULL` | SQL `NULL` |
| `insert_latency` | `varchar(11)` | `bigint unsigned` | formatted: `YES`; x$: `NO` | `NULL` | formatted: `utf8mb3_general_ci`; x$: SQL `NULL` |
| `rows_updated` | `bigint unsigned` | `bigint unsigned` | `NO` | `NULL` | SQL `NULL` |
| `update_latency` | `varchar(11)` | `bigint unsigned` | formatted: `YES`; x$: `NO` | `NULL` | formatted: `utf8mb3_general_ci`; x$: SQL `NULL` |
| `rows_deleted` | `bigint unsigned` | `bigint unsigned` | `NO` | `NULL` | SQL `NULL` |
| `delete_latency` | `varchar(11)` | `bigint unsigned` | formatted: `YES`; x$: `NO` | `NULL` | formatted: `utf8mb3_general_ci`; x$: SQL `NULL` |

`SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and
`INFORMATION_SCHEMA.COLUMNS` expose this shape. The views have no index or
constraint metadata, so `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`,
`TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
`TABLE_CONSTRAINTS_EXTENSIONS` return zero rows for the view objects
themselves.

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
`ALGORITHM=MERGE` and `DEFINER=\`mysql.sys\`@\`localhost\``. Qualified targets
render the view name as `` `sys`.`schema_index_statistics` `` or
`` `sys`.`x$schema_index_statistics` ``. Unqualified targets resolved after
`USE sys` render the unqualified view name.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports one dependency row from each view
to `performance_schema.table_io_waits_summary_by_index_usage`.
`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` reports zero rows.

## Unsupported Behavior

This slice intentionally does not implement:

- Performance Schema table I/O wait collection;
- real latency accumulation or statement attribution;
- dynamic `performance_schema.table_io_waits_summary_by_index_usage` rows;
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
  `sys.schema_index_statistics` and `sys.x$schema_index_statistics` view
  entries.
- Query execution: reuses the existing synthetic system-table SELECT planner
  and builds descriptor-backed index rows.
- Information schema: adds `COLUMNS`, `VIEWS`, and `VIEW_TABLE_USAGE` rows
  through the existing synthetic metadata builders.
- SHOW metadata: reuses the synthetic system-table column/index paths and the
  built-in sys view `SHOW CREATE` short-circuit.
- Storage/SQLite: unchanged. No SQLite extension API or fork hook is required.

## Performance

The views scan supported mysql/sys system-table descriptors and persistent
catalog table descriptors, loading index descriptors for user base tables. They
perform no SQLite data-table scan.

## Tests

MySQL 8.4.9 expectation coverage:

- formatted and raw column metadata;
- user-schema primary, unique, and nonunique index rows;
- view and dependency metadata;
- empty index and constraint metadata for the view objects;
- qualified and selected-schema `SHOW CREATE VIEW` / `SHOW CREATE TABLE`;
- `ROW_COUNT() = -1` and zero warnings after a synthetic view read.

MyLite runtime coverage mirrors the supported metadata, selected-schema, and
descriptor-backed row cases, including zero-counter user and system index rows.
