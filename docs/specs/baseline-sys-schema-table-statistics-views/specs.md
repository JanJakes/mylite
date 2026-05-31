# Baseline sys.schema_table_statistics Views

This slice adds MySQL-shaped metadata and deterministic read-only rows for
`sys.schema_table_statistics` and `sys.x$schema_table_statistics`. MyLite does
not collect Performance Schema table I/O wait summaries, so the views expose
descriptor-backed table inventory with zero counters, zero latency, and zero
byte placeholders.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.schema_table_statistics` and
  `sys.x$schema_table_statistics`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-schema-table-statistics.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_schema_table_statistics_views_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified column metadata, user-table rows, view definition metadata, table
dependency metadata, no routine dependency metadata, and selected-schema
access.

## Supported Behavior

Supported direct reads:

```sql
SELECT * FROM sys.schema_table_statistics;
SELECT * FROM sys.`x$schema_table_statistics`;

USE sys;
SELECT table_schema, table_name FROM schema_table_statistics;
SELECT table_schema, table_name FROM `x$schema_table_statistics`;
```

MyLite emits one row per known descriptor base table for:

- supported `mysql` system-table descriptors;
- `sys.sys_config`;
- persistent user base-table descriptors.

Temporary tables, views, unsupported Performance Schema tables, and unsupported
system-table descriptors are omitted. Every emitted row reports zero counters,
zero latency, and zero bytes because MyLite does not currently maintain
Performance Schema table wait summaries or file I/O summaries.

`sys.schema_table_statistics` formats latency columns as the MySQL zero-time
string `  0 ps` and byte columns as `   0 bytes`. `sys.x$schema_table_statistics`
exposes the same metric fields as raw numeric `0` text.

## Column Metadata

Both views have nineteen columns:

| Column | Formatted view type | x$ view type | Null | Default | Collation |
| --- | --- | --- | --- | --- | --- |
| `table_schema` | `varchar(64)` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `table_name` | `varchar(64)` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `total_latency` | `varchar(11)` | `bigint unsigned` | formatted: `YES`; x$: `NO` | `NULL` | formatted: `utf8mb3_general_ci`; x$: SQL `NULL` |
| `rows_fetched` | `bigint unsigned` | `bigint unsigned` | `NO` | `NULL` | SQL `NULL` |
| `fetch_latency` | `varchar(11)` | `bigint unsigned` | formatted: `YES`; x$: `NO` | `NULL` | formatted: `utf8mb3_general_ci`; x$: SQL `NULL` |
| `rows_inserted` | `bigint unsigned` | `bigint unsigned` | `NO` | `NULL` | SQL `NULL` |
| `insert_latency` | `varchar(11)` | `bigint unsigned` | formatted: `YES`; x$: `NO` | `NULL` | formatted: `utf8mb3_general_ci`; x$: SQL `NULL` |
| `rows_updated` | `bigint unsigned` | `bigint unsigned` | `NO` | `NULL` | SQL `NULL` |
| `update_latency` | `varchar(11)` | `bigint unsigned` | formatted: `YES`; x$: `NO` | `NULL` | formatted: `utf8mb3_general_ci`; x$: SQL `NULL` |
| `rows_deleted` | `bigint unsigned` | `bigint unsigned` | `NO` | `NULL` | SQL `NULL` |
| `delete_latency` | `varchar(11)` | `bigint unsigned` | formatted: `YES`; x$: `NO` | `NULL` | formatted: `utf8mb3_general_ci`; x$: SQL `NULL` |
| `io_read_requests` | `decimal(42,0)` | `decimal(42,0)` | `YES` | `NULL` | SQL `NULL` |
| `io_read` | `varchar(11)` | `decimal(41,0)` | `YES` | `NULL` | formatted: `utf8mb3_general_ci`; x$: SQL `NULL` |
| `io_read_latency` | `varchar(11)` | `decimal(42,0)` | `YES` | `NULL` | formatted: `utf8mb3_general_ci`; x$: SQL `NULL` |
| `io_write_requests` | `decimal(42,0)` | `decimal(42,0)` | `YES` | `NULL` | SQL `NULL` |
| `io_write` | `varchar(11)` | `decimal(41,0)` | `YES` | `NULL` | formatted: `utf8mb3_general_ci`; x$: SQL `NULL` |
| `io_write_latency` | `varchar(11)` | `decimal(42,0)` | `YES` | `NULL` | formatted: `utf8mb3_general_ci`; x$: SQL `NULL` |
| `io_misc_requests` | `decimal(42,0)` | `decimal(42,0)` | `YES` | `NULL` | SQL `NULL` |
| `io_misc_latency` | `varchar(11)` | `decimal(42,0)` | `YES` | `NULL` | formatted: `utf8mb3_general_ci`; x$: SQL `NULL` |

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
- `IS_UPDATABLE = 'NO'`
- `DEFINER = 'mysql.sys@localhost'`
- `SECURITY_TYPE = 'INVOKER'`
- `CHARACTER_SET_CLIENT = 'utf8mb4'`
- `COLLATION_CONNECTION = 'utf8mb4_0900_ai_ci'`

`SHOW CREATE VIEW` and `SHOW CREATE TABLE` return MySQL-shaped metadata with
`ALGORITHM=TEMPTABLE` and the `mysql.sys@localhost` definer. The formatted view
definition uses `format_pico_time()` and `format_bytes()` around the
Performance Schema counters; the raw `x$` view exposes those counters directly.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports dependencies from each view to
`performance_schema.table_io_waits_summary_by_table` and
`sys.x$ps_schema_table_statistics_io`.
`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` reports zero rows.

## Unsupported Behavior

This slice intentionally does not implement:

- Performance Schema table I/O wait collection;
- real row-operation, latency, byte, or file-I/O accumulation;
- dynamic `performance_schema.table_io_waits_summary_by_table` rows;
- execution of `sys.x$ps_schema_table_statistics_io`;
- temporary-table rows;
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
  `sys.schema_table_statistics` and `sys.x$schema_table_statistics` view
  entries.
- Query execution: reuses the existing synthetic system-table SELECT planner
  and builds descriptor-backed base-table rows.
- Information schema: adds `COLUMNS`, `VIEWS`, and `VIEW_TABLE_USAGE` rows
  through the existing synthetic metadata builders.
- SHOW metadata: reuses the synthetic system-table column/index paths and the
  built-in sys view `SHOW CREATE` short-circuit.
- Storage/SQLite: unchanged. No SQLite extension API or fork hook is required.

## Performance

The views scan supported mysql/sys system-table descriptors and persistent
catalog table descriptors. They perform no SQLite data-table scan and collect
no live counters.

## Tests

MySQL 8.4.9 expectation coverage:

- formatted and raw column metadata;
- user-schema table rows and selected-schema access;
- view and dependency metadata;
- empty index and constraint metadata for the view objects;
- qualified and selected-schema `SHOW CREATE VIEW` / `SHOW CREATE TABLE`;
- `ROW_COUNT() = -1` and zero warnings after a sys view read.

MyLite runtime coverage mirrors the supported metadata, selected-schema, and
descriptor-backed row cases, including zero-counter user and system table rows.
