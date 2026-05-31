# Baseline sys.io_by_thread_by_latency Views

This slice adds MySQL-shaped metadata and deterministic read-only empty rows for
the `sys.io_by_thread_by_latency` and `sys.x$io_by_thread_by_latency` views.
MySQL uses these sys views to summarize file I/O wait latency grouped by
thread. MyLite does not collect Performance Schema wait-summary counters or
thread inventory, so both views are metadata-complete empty placeholders.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.io_by_thread_by_latency` and
  `sys.x$io_by_thread_by_latency`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-io-by-thread-by-latency.html>
- MySQL 8.4 Reference Manual, Performance Schema wait event summary tables:
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-wait-summary-tables.html>
- MySQL 8.4 Reference Manual, Performance Schema `threads` table:
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-threads-table.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_io_by_thread_by_latency_views_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified column metadata, view metadata, table dependency metadata, absence of
routine dependencies, index/constraint absence, selected-schema access,
`SHOW CREATE` rendering, `SHOW TABLE STATUS`, and read status behavior. Direct
MySQL row values are environment-dependent because they reflect live
Performance Schema wait-summary counters.

## Supported Behavior

Supported direct reads:

```sql
SELECT * FROM sys.io_by_thread_by_latency;
SELECT * FROM sys.`x$io_by_thread_by_latency`;

USE sys;
SELECT * FROM io_by_thread_by_latency;
SELECT * FROM `x$io_by_thread_by_latency`;
```

Both views return zero rows. This differs from a live MySQL server with file
I/O wait-summary rows, but preserves the queryable sys view surface without
inventing thread or wait-event counters.

## Column Metadata

`sys.io_by_thread_by_latency` has eight columns:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `user` | `varchar(288)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `total` | `decimal(42,0)` | `YES` | `NULL` | SQL `NULL` |
| `total_latency` | `varchar(11)` | `YES` | `NULL` | `utf8mb3_general_ci` |
| `min_latency` | `varchar(11)` | `YES` | `NULL` | `utf8mb3_general_ci` |
| `avg_latency` | `varchar(11)` | `YES` | `NULL` | `utf8mb3_general_ci` |
| `max_latency` | `varchar(11)` | `YES` | `NULL` | `utf8mb3_general_ci` |
| `thread_id` | `bigint unsigned` | `NO` | `NULL` | SQL `NULL` |
| `processlist_id` | `bigint unsigned` | `YES` | `NULL` | SQL `NULL` |

`sys.x$io_by_thread_by_latency` has the same column names with raw numeric
latency metadata:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `user` | `varchar(288)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `total` | `decimal(42,0)` | `YES` | `NULL` | SQL `NULL` |
| `total_latency` | `decimal(42,0)` | `YES` | `NULL` | SQL `NULL` |
| `min_latency` | `bigint unsigned` | `YES` | `NULL` | SQL `NULL` |
| `avg_latency` | `decimal(24,4)` | `YES` | `NULL` | SQL `NULL` |
| `max_latency` | `bigint unsigned` | `YES` | `NULL` | SQL `NULL` |
| `thread_id` | `bigint unsigned` | `NO` | `NULL` | SQL `NULL` |
| `processlist_id` | `bigint unsigned` | `YES` | `NULL` | SQL `NULL` |

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
- `IS_UPDATABLE = 'NO'`
- `DEFINER = 'mysql.sys@localhost'`
- `SECURITY_TYPE = 'INVOKER'`
- `CHARACTER_SET_CLIENT = 'utf8mb4'`
- `COLLATION_CONNECTION = 'utf8mb4_0900_ai_ci'`

`SHOW CREATE VIEW` and `SHOW CREATE TABLE` return MySQL-shaped metadata with
`ALGORITHM=TEMPTABLE` and the `mysql.sys@localhost` definer. The formatted view
uses `substring_index()`, `concat()`, `convert(... using utf8mb4)`, and
`format_pico_time()` over `performance_schema.events_waits_summary_by_thread_by_event_name`
left joined to `performance_schema.threads`; the raw `x$` view exposes raw
Performance Schema timer and count aggregates.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports dependencies for each view on:

- `performance_schema.events_waits_summary_by_thread_by_event_name`
- `performance_schema.threads`

`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` returns no rows for these two views.
This matches the observed MySQL 8.4.9 runtime, which does not expose
unqualified `format_pico_time()` as a routine dependency for this view.

## Unsupported Behavior

This slice intentionally does not implement:

- Performance Schema wait-summary collection;
- live thread, file-I/O wait, latency, or aggregate rows;
- execution of `substring_index()`, `concat()`, `convert(... using utf8mb4)`,
  or `format_pico_time()` for this sys view;
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
- Information schema: adds `COLUMNS`, `TABLES`, `VIEWS`, and
  `VIEW_TABLE_USAGE` rows plus empty index and constraint metadata through
  existing synthetic metadata builders.
- SHOW metadata: reuses the synthetic system-table column/index paths and the
  built-in sys view `SHOW CREATE` short-circuit.
- Storage/SQLite: unchanged. No SQLite extension API or fork hook is required.

## Performance

Both views return empty rows without scanning SQLite data tables or collecting
Performance Schema wait-summary counters.

## Tests

MySQL 8.4.9 expectation coverage:

- column metadata for formatted and raw views;
- selected-schema access and live-row presence without depending on variable
  counter values;
- view and table-dependency metadata and absence of routine dependencies;
- empty index and constraint metadata for both view objects;
- qualified and selected-schema `SHOW CREATE VIEW` / `SHOW CREATE TABLE`;
- `SHOW TABLE STATUS` metadata;
- `ROW_COUNT() = -1` and zero warnings after reads.

MyLite runtime coverage mirrors the supported empty placeholder rows,
selected-schema access, and metadata surfaces for both views.
