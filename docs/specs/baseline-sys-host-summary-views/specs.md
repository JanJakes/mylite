# Baseline sys.host_summary Views

This slice adds MySQL-shaped metadata and deterministic read-only empty rows for
the `sys.host_summary` and `sys.x$host_summary` views. MySQL uses these sys
views to summarize statement, file I/O, connection, user, and memory counters
grouped by Performance Schema host. MyLite does not collect Performance Schema
account, statement, file-I/O, or memory counters, so both views are
metadata-complete empty placeholders.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.host_summary` and `sys.x$host_summary`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-host-summary.html>
- MySQL 8.4 Reference Manual, Performance Schema `accounts` table:
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-accounts-table.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_host_summary_views_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified column metadata, view metadata, table dependency metadata, absence of
routine dependencies, index/constraint absence, selected-schema access,
`SHOW CREATE` rendering, `SHOW TABLE STATUS`, and read status behavior. Direct
MySQL rows are environment-dependent because they reflect live Performance
Schema account, statement, file-I/O, and memory counters.

## Supported Behavior

Supported direct reads:

```sql
SELECT * FROM sys.host_summary;
SELECT * FROM sys.`x$host_summary`;

USE sys;
SELECT * FROM host_summary;
SELECT * FROM `x$host_summary`;
```

Both views return zero rows. This differs from a live MySQL server with
Performance Schema account rows, but preserves the queryable sys view surface
without inventing host or wait counter data. MySQL can emit a division-by-zero
warning while reading `sys.x$host_summary` when live statement totals contain
zero denominators; MyLite's empty placeholder does not evaluate that expression
and therefore returns no warning.

## Column Metadata

`sys.host_summary` has twelve columns:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `host` | `varchar(255)` | `YES` | `NULL` | `ascii_general_ci` |
| `statements` | `decimal(64,0)` | `YES` | `NULL` | SQL `NULL` |
| `statement_latency` | `varchar(11)` | `YES` | `NULL` | `utf8mb3_general_ci` |
| `statement_avg_latency` | `varchar(11)` | `YES` | `NULL` | `utf8mb3_general_ci` |
| `table_scans` | `decimal(65,0)` | `YES` | `NULL` | SQL `NULL` |
| `file_ios` | `decimal(64,0)` | `YES` | `NULL` | SQL `NULL` |
| `file_io_latency` | `varchar(11)` | `YES` | `NULL` | `utf8mb3_general_ci` |
| `current_connections` | `decimal(41,0)` | `YES` | `NULL` | SQL `NULL` |
| `total_connections` | `decimal(41,0)` | `YES` | `NULL` | SQL `NULL` |
| `unique_users` | `bigint` | `NO` | `0` | SQL `NULL` |
| `current_memory` | `varchar(11)` | `YES` | `NULL` | `utf8mb3_general_ci` |
| `total_memory_allocated` | `varchar(11)` | `YES` | `NULL` | `utf8mb3_general_ci` |

`sys.x$host_summary` has the same column names with raw numeric latency and
memory metadata:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `host` | `varchar(255)` | `YES` | `NULL` | `ascii_general_ci` |
| `statements` | `decimal(64,0)` | `YES` | `NULL` | SQL `NULL` |
| `statement_latency` | `decimal(64,0)` | `YES` | `NULL` | SQL `NULL` |
| `statement_avg_latency` | `decimal(65,4)` | `YES` | `NULL` | SQL `NULL` |
| `table_scans` | `decimal(65,0)` | `YES` | `NULL` | SQL `NULL` |
| `file_ios` | `decimal(64,0)` | `YES` | `NULL` | SQL `NULL` |
| `file_io_latency` | `decimal(64,0)` | `YES` | `NULL` | SQL `NULL` |
| `current_connections` | `decimal(41,0)` | `YES` | `NULL` | SQL `NULL` |
| `total_connections` | `decimal(41,0)` | `YES` | `NULL` | SQL `NULL` |
| `unique_users` | `bigint` | `NO` | `0` | SQL `NULL` |
| `current_memory` | `decimal(63,0)` | `YES` | `NULL` | SQL `NULL` |
| `total_memory_allocated` | `decimal(64,0)` | `YES` | `NULL` | SQL `NULL` |

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
uses `format_pico_time()` and `format_bytes()` around aggregate counters; the
raw `x$` view exposes raw aggregate values and the raw average expression.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports dependencies for each view on:

- `performance_schema.accounts`
- `sys.x$host_summary_by_file_io`
- `sys.x$host_summary_by_statement_latency`
- `sys.x$memory_by_host_by_current_bytes`

`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` returns no rows for these two views.
This matches the observed MySQL 8.4.9 runtime, which does not expose
unqualified `format_pico_time()` or `format_bytes()` calls as routine
dependencies for this view pair.

## Unsupported Behavior

This slice intentionally does not implement:

- Performance Schema account, statement, file-I/O, or memory summary
  collection;
- live host rows, connection counts, unique-user counts, latency totals, memory
  totals, or live `x$host_summary` warning production;
- execution of the sys helper views or formatting functions referenced by the
  view definitions;
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
Performance Schema account or wait-summary counters.

## Tests

MySQL 8.4.9 expectation coverage:

- column metadata for formatted and raw views;
- selected-schema access and live-row presence without depending on variable
  counter values;
- view and table-dependency metadata and absence of routine dependencies;
- empty index and constraint metadata for both view objects;
- qualified and selected-schema `SHOW CREATE VIEW` / `SHOW CREATE TABLE`;
- `SHOW TABLE STATUS` metadata;
- `ROW_COUNT() = -1` after reads, plus observed live warning behavior for the
  raw view.

MyLite runtime coverage mirrors the supported empty placeholder rows,
selected-schema access, and metadata surfaces for both views.
