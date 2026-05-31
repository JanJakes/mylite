# Baseline sys.latest_file_io Views

This slice adds MySQL-shaped metadata and deterministic empty read-only rows for
`sys.latest_file_io` and `sys.x$latest_file_io`. MySQL exposes the latest file
I/O events by joining Performance Schema wait history with thread and process
metadata. MyLite does not collect Performance Schema file-I/O wait history, so
these sys views are exposed as empty embedded placeholders with the MySQL 8.4.9
catalog surface.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.latest_file_io` and
  `sys.x$latest_file_io`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-latest-file-io.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_latest_file_io_views_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified column metadata, empty baseline rows, view definition metadata, table
and routine dependency metadata, SHOW metadata, selected-schema behavior, and
read status behavior.

## Supported Behavior

Supported direct reads:

```sql
SELECT * FROM sys.latest_file_io;
SELECT * FROM sys.`x$latest_file_io`;

USE sys;
SELECT COUNT(*) FROM latest_file_io;
SELECT COUNT(*) FROM `x$latest_file_io`;
```

Both views return zero rows. This matches the local target runtime when no
file-I/O events are present in `performance_schema.events_waits_history_long`.
MyLite does not synthesize file-I/O rows and does not expose dynamic
Performance Schema wait-history data.

## Column Metadata

Both views have five columns. The formatted view exposes formatted latency and
requested-byte strings. The raw `x$` view exposes numeric latency and requested
byte counts.

| Column | Formatted type | Raw type | Null | Default | Collation |
| --- | --- | --- | --- | --- | --- |
| `thread` | `varchar(317)` | `varchar(317)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `file` | `varchar(512)` | `varchar(512)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `latency` | `varchar(11)` | `bigint unsigned` | `YES` | `NULL` | formatted `utf8mb3_general_ci`; raw SQL `NULL` |
| `operation` | `varchar(32)` | `varchar(32)` | `NO` | `NULL` | `utf8mb4_0900_ai_ci` |
| `requested` | `varchar(11)` | `bigint` | `YES` | `NULL` | formatted `utf8mb3_general_ci`; raw SQL `NULL` |

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
- `IS_UPDATABLE = 'NO'`
- `DEFINER = 'mysql.sys@localhost'`
- `SECURITY_TYPE = 'INVOKER'`
- `CHARACTER_SET_CLIENT = 'utf8mb4'`
- `COLLATION_CONNECTION = 'utf8mb4_0900_ai_ci'`

`SHOW CREATE VIEW` and `SHOW CREATE TABLE` return MySQL-shaped metadata with
`ALGORITHM=MERGE` and the `mysql.sys@localhost` definer. The formatted view
definition applies `sys.format_path()` to the file path and uses MySQL sys
formatting functions for latency and byte counts; the raw `x$` view exposes
the underlying path, wait time, and byte count expressions directly.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports dependencies from both views to:

- `information_schema.PROCESSLIST`
- `performance_schema.events_waits_history_long`
- `performance_schema.threads`

The formatted view also reports a dependency on
`performance_schema.global_variables`.

`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` reports a dependency from the formatted
view to `sys.format_path`. The raw view reports no routine dependency rows.

## Unsupported Behavior

This slice intentionally does not implement:

- Performance Schema file-I/O wait collection;
- row production from `performance_schema.events_waits_history_long`;
- thread/process account formatting from live Performance Schema rows;
- stored-function execution for `sys.format_path()`, `format_pico_time()`, or
  `format_bytes()`;
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
  `sys.latest_file_io` and `sys.x$latest_file_io` view entries.
- Query execution: reuses the existing synthetic system-table SELECT planner
  and returns empty row sets for both views.
- Information schema: adds `COLUMNS`, `TABLES`, `VIEWS`,
  `VIEW_TABLE_USAGE`, and `VIEW_ROUTINE_USAGE` rows plus empty index and
  constraint metadata through existing synthetic metadata builders.
- SHOW metadata: reuses the synthetic system-table column/index paths and the
  built-in sys view `SHOW CREATE` short-circuit.
- Storage/SQLite: unchanged. No SQLite extension API or fork hook is required.

## Performance

Reads of these views produce no rows and do not scan user data or SQLite
storage. Metadata queries use the existing static descriptor arrays.

## Tests

MySQL 8.4.9 expectation coverage:

- column metadata for both views;
- empty direct reads through qualified and selected-schema names;
- view, table-dependency, and routine-dependency metadata;
- empty index and constraint metadata for the view objects;
- qualified and selected-schema `SHOW CREATE VIEW` / `SHOW CREATE TABLE`;
- `SHOW TABLE STATUS` metadata;
- `ROW_COUNT() = -1` and zero warnings after a synthetic view read.

MyLite runtime coverage mirrors the supported metadata and empty row behavior.
