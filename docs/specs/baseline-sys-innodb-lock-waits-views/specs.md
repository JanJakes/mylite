# Baseline sys.innodb_lock_waits Views

This slice adds MySQL-shaped metadata and deterministic empty read-only rows for
`sys.innodb_lock_waits` and `sys.x$innodb_lock_waits`. MySQL exposes InnoDB row
lock waits by joining Performance Schema lock tables with
`INFORMATION_SCHEMA.INNODB_TRX`. MyLite does not maintain InnoDB lock-wait
instrumentation, so these sys views are exposed as empty embedded placeholders
with the MySQL 8.4.9 catalog surface.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.innodb_lock_waits` and
  `sys.x$innodb_lock_waits`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-innodb-lock-waits.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_innodb_lock_waits_views_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified column metadata, empty baseline rows, selected-schema access, view
definition metadata, table and routine dependency metadata, SHOW metadata, and
read status behavior.

## Supported Behavior

Supported direct reads:

```sql
SELECT * FROM sys.innodb_lock_waits;
SELECT * FROM sys.`x$innodb_lock_waits`;

USE sys;
SELECT COUNT(*) FROM innodb_lock_waits;
SELECT COUNT(*) FROM `x$innodb_lock_waits`;
```

Both views return zero rows. This matches the local target runtime when no
InnoDB transaction is waiting on a row lock. MyLite does not synthesize lock
wait rows and does not expose dynamic `performance_schema.data_lock_waits` or
`performance_schema.data_locks` data.

## Column Metadata

Both views have the same thirty columns. The formatted view exposes
`waiting_query` and `blocking_query` as `longtext` with
`utf8mb4_0900_ai_ci`; the raw `x$` view exposes those two columns as
`varchar(1024)` with `utf8mb3_general_ci`.

| Column | Formatted type | Raw type | Null | Default | Collation |
| --- | --- | --- | --- | --- | --- |
| `wait_started` | `datetime` | `datetime` | `YES` | `NULL` | SQL `NULL` |
| `wait_age` | `time` | `time` | `YES` | `NULL` | SQL `NULL` |
| `wait_age_secs` | `bigint` | `bigint` | `YES` | `NULL` | SQL `NULL` |
| `locked_table` | `mediumtext` | `mediumtext` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `locked_table_schema` | `varchar(64)` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `locked_table_name` | `varchar(64)` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `locked_table_partition` | `varchar(64)` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `locked_table_subpartition` | `varchar(64)` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `locked_index` | `varchar(64)` | `varchar(64)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `locked_type` | `varchar(32)` | `varchar(32)` | `NO` | `NULL` | `utf8mb4_0900_ai_ci` |
| `waiting_trx_id` | `bigint unsigned` | `bigint unsigned` | `NO` | `0` | SQL `NULL` |
| `waiting_trx_started` | `datetime` | `datetime` | `NO` | `0000-00-00 00:00:00` | SQL `NULL` |
| `waiting_trx_age` | `time` | `time` | `YES` | `NULL` | SQL `NULL` |
| `waiting_trx_rows_locked` | `bigint unsigned` | `bigint unsigned` | `NO` | `0` | SQL `NULL` |
| `waiting_trx_rows_modified` | `bigint unsigned` | `bigint unsigned` | `NO` | `0` | SQL `NULL` |
| `waiting_pid` | `bigint unsigned` | `bigint unsigned` | `NO` | `0` | SQL `NULL` |
| `waiting_query` | `longtext` | `varchar(1024)` | `YES` | `NULL` | formatted `utf8mb4_0900_ai_ci`; raw `utf8mb3_general_ci` |
| `waiting_lock_id` | `varchar(128)` | `varchar(128)` | `NO` | `NULL` | `utf8mb4_0900_ai_ci` |
| `waiting_lock_mode` | `varchar(32)` | `varchar(32)` | `NO` | `NULL` | `utf8mb4_0900_ai_ci` |
| `blocking_trx_id` | `bigint unsigned` | `bigint unsigned` | `NO` | `0` | SQL `NULL` |
| `blocking_pid` | `bigint unsigned` | `bigint unsigned` | `NO` | `0` | SQL `NULL` |
| `blocking_query` | `longtext` | `varchar(1024)` | `YES` | `NULL` | formatted `utf8mb4_0900_ai_ci`; raw `utf8mb3_general_ci` |
| `blocking_lock_id` | `varchar(128)` | `varchar(128)` | `NO` | `NULL` | `utf8mb4_0900_ai_ci` |
| `blocking_lock_mode` | `varchar(32)` | `varchar(32)` | `NO` | `NULL` | `utf8mb4_0900_ai_ci` |
| `blocking_trx_started` | `datetime` | `datetime` | `NO` | `0000-00-00 00:00:00` | SQL `NULL` |
| `blocking_trx_age` | `time` | `time` | `YES` | `NULL` | SQL `NULL` |
| `blocking_trx_rows_locked` | `bigint unsigned` | `bigint unsigned` | `NO` | `0` | SQL `NULL` |
| `blocking_trx_rows_modified` | `bigint unsigned` | `bigint unsigned` | `NO` | `0` | SQL `NULL` |
| `sql_kill_blocking_query` | `varchar(33)` | `varchar(33)` | `NO` | empty string | `utf8mb4_0900_ai_ci` |
| `sql_kill_blocking_connection` | `varchar(27)` | `varchar(27)` | `NO` | empty string | `utf8mb4_0900_ai_ci` |

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
definition applies `sys.format_statement()` to the waiting and blocking query
text; the raw `x$` view exposes those query expressions directly.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports dependencies from both views to:

- `information_schema.INNODB_TRX`
- `performance_schema.data_lock_waits`
- `performance_schema.data_locks`

The formatted view also reports a dependency on `sys.sys_config`.

`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` reports dependencies from the formatted
view to `sys.format_statement` and `sys.quote_identifier`, and from the raw
view to `sys.quote_identifier`.

## Unsupported Behavior

This slice intentionally does not implement:

- live InnoDB row-lock wait collection;
- row production from `performance_schema.data_lock_waits` or
  `performance_schema.data_locks`;
- blocking session discovery or `KILL` command behavior derived from these
  views;
- physical MySQL sys view execution;
- stored-function execution for `sys.format_statement()` or
  `sys.quote_identifier()`;
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
  `sys.innodb_lock_waits` and `sys.x$innodb_lock_waits` view entries.
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
