# Baseline sys.schema_table_lock_waits Views

This slice adds MySQL-shaped metadata and deterministic empty read-only rows for
`sys.schema_table_lock_waits` and `sys.x$schema_table_lock_waits`. MyLite does
not collect Performance Schema metadata-lock waits, so the views are exposed as
empty embedded placeholders with the MySQL 8.4.9 catalog surface.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.schema_table_lock_waits` and
  `sys.x$schema_table_lock_waits`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-schema-table-lock-waits.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_schema_table_lock_waits_views_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified column metadata, empty baseline rows, selected-schema access, view
definition metadata, table and routine dependency metadata, and SHOW metadata.

## Supported Behavior

Supported direct reads:

```sql
SELECT * FROM sys.schema_table_lock_waits;
SELECT * FROM sys.`x$schema_table_lock_waits`;

USE sys;
SELECT COUNT(*) FROM schema_table_lock_waits;
SELECT COUNT(*) FROM `x$schema_table_lock_waits`;
```

Both views return zero rows. This is a deliberate embedded placeholder: MyLite
does not maintain Performance Schema `metadata_locks`, `threads`, or current
statement rows, and it has no cross-session blocking lock inventory to report.

## Column Metadata

Both views have the same eighteen columns:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `object_schema` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `object_name` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `waiting_thread_id` | `bigint unsigned` | `NO` | `NULL` | SQL `NULL` |
| `waiting_pid` | `bigint unsigned` | `YES` | `NULL` | SQL `NULL` |
| `waiting_account` | `text` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `waiting_lock_type` | `varchar(32)` | `NO` | `NULL` | `utf8mb4_0900_ai_ci` |
| `waiting_lock_duration` | `varchar(32)` | `NO` | `NULL` | `utf8mb4_0900_ai_ci` |
| `waiting_query` | `longtext` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `waiting_query_secs` | `bigint` | `YES` | `NULL` | SQL `NULL` |
| `waiting_query_rows_affected` | `bigint unsigned` | `YES` | `NULL` | SQL `NULL` |
| `waiting_query_rows_examined` | `bigint unsigned` | `YES` | `NULL` | SQL `NULL` |
| `blocking_thread_id` | `bigint unsigned` | `NO` | `NULL` | SQL `NULL` |
| `blocking_pid` | `bigint unsigned` | `YES` | `NULL` | SQL `NULL` |
| `blocking_account` | `text` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `blocking_lock_type` | `varchar(32)` | `NO` | `NULL` | `utf8mb4_0900_ai_ci` |
| `blocking_lock_duration` | `varchar(32)` | `NO` | `NULL` | `utf8mb4_0900_ai_ci` |
| `sql_kill_blocking_query` | `varchar(31)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `sql_kill_blocking_connection` | `varchar(25)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |

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
`ALGORITHM=TEMPTABLE` and the `mysql.sys@localhost` definer. The formatted view
definition uses `sys.format_statement()` for `waiting_query`; the raw `x$` view
exposes the current statement text directly.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports dependencies from both views to
`performance_schema.events_statements_current`, `performance_schema.metadata_locks`,
and `performance_schema.threads`; `schema_table_lock_waits` also reports a
dependency on `sys.sys_config`.

`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` reports dependencies from
`schema_table_lock_waits` to `sys.format_statement` and
`sys.ps_thread_account`, and from `x$schema_table_lock_waits` to
`sys.ps_thread_account`.

## Unsupported Behavior

This slice intentionally does not implement:

- live Performance Schema metadata-lock wait collection;
- blocking session discovery or `KILL` command behavior derived from these
  views;
- row production for real metadata locks;
- physical MySQL sys view execution;
- stored-function execution for `sys.format_statement()` or
  `sys.ps_thread_account()`;
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
  `sys.schema_table_lock_waits` and `sys.x$schema_table_lock_waits` view
  entries.
- Query execution: reuses the existing synthetic system-table SELECT planner
  and returns empty row sets for both views.
- Information schema: adds `COLUMNS`, `VIEWS`, `VIEW_TABLE_USAGE`, and
  `VIEW_ROUTINE_USAGE` rows through existing synthetic metadata builders.
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
- `ROW_COUNT() = -1` and zero warnings after a synthetic view read.

MyLite runtime coverage mirrors the supported metadata and empty row behavior.
