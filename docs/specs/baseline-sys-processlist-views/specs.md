# Baseline sys.processlist Views

This slice adds MySQL-shaped metadata and a deterministic partial rowset for
the `sys.processlist` and `sys.x$processlist` views.

MySQL builds these sys views from Performance Schema thread, statement, wait,
stage, transaction, connection-attribute, and memory-summary tables. MyLite
does not collect that live Performance Schema instrumentation yet. MyLite does
already maintain an embedded processlist registry for `SHOW PROCESSLIST` and
`INFORMATION_SCHEMA.PROCESSLIST`, so these sys views expose current MyLite
sessions with stable connection, user, schema, command, current statement, and
transaction/autocommit placeholders while documenting the remaining
instrumentation gaps.

## Compatibility Authority

- MySQL 8.4 Reference Manual, sys schema process list views:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-processlist.html>
- MySQL 8.4 Reference Manual, Performance Schema threads table:
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-threads-table.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_processlist_views_expectations.sh`.

Runtime probes verified column metadata, formatted/raw metadata differences,
view dependencies, routine dependency metadata, selected-schema behavior,
`SHOW CREATE`, `SHOW TABLE STATUS`, current-session rows, warnings, and
`ROW_COUNT()` behavior.

## Supported Behavior

Supported reads:

```sql
SELECT * FROM sys.processlist;
SELECT * FROM sys.`x$processlist`;

USE sys;
SELECT * FROM processlist;
SELECT * FROM `x$processlist`;
```

MyLite returns one row per open MyLite connection registered in the embedded
processlist registry. The current connection exposes:

- `thd_id`: the MyLite connection id as a thread-id placeholder;
- `conn_id`: the MyLite connection id;
- `user`: the session client identity, currently `root@%` by default;
- `db`: the selected schema, or `NULL`;
- `command`: `Query` for the current statement, `Sleep` for idle sessions;
- `state`: `executing` for the current statement, otherwise `NULL`;
- `time`: `0`;
- `current_statement`: the current SQL text without a trailing terminator;
- `execution_engine`: `PRIMARY` for the current statement;
- statement row counters: `0`;
- `full_scan`: `NO`;
- `trx_state`: `ACTIVE` for the current statement or an explicitly active user
  transaction, otherwise `NULL`;
- `trx_autocommit`: `YES` or `NO` from the session autocommit state.

Rows intentionally do not fabricate timing, wait, memory, transaction latency,
connection attribute, or last-statement values.

## Metadata

Both views expose MySQL 8.4.9-shaped 30-column metadata. `sys.processlist` uses
formatted display metadata for statement, lock, CPU, wait, transaction latency,
and current memory fields. `sys.x$processlist` uses raw numeric metadata for
those fields where MySQL exposes raw values.

`SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and
`INFORMATION_SCHEMA.COLUMNS` expose the MySQL-observed column shape. The views
have no index or constraint metadata, so `SHOW INDEX`,
`INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
`TABLE_CONSTRAINTS_EXTENSIONS` return zero rows.

`INFORMATION_SCHEMA.VIEWS` exposes both built-in view rows with:

- `TABLE_CATALOG = 'def'`
- `TABLE_SCHEMA = 'sys'`
- `CHECK_OPTION = 'NONE'`
- `IS_UPDATABLE = 'NO'`
- `DEFINER = 'mysql.sys@localhost'`
- `SECURITY_TYPE = 'INVOKER'`
- `CHARACTER_SET_CLIENT = 'utf8mb4'`
- `COLLATION_CONNECTION = 'utf8mb4_0900_ai_ci'`

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports MySQL-shaped dependencies for
both views on:

- `performance_schema.events_stages_current`
- `performance_schema.events_statements_current`
- `performance_schema.events_transactions_current`
- `performance_schema.events_waits_current`
- `performance_schema.session_connect_attrs`
- `performance_schema.threads`
- `sys.x$memory_by_thread_by_current_bytes`

`sys.processlist` also reports the MySQL dependency on `sys.sys_config`.

`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` reports `sys.processlist` dependency on
`sys.format_statement`. `sys.x$processlist` has no routine dependency row.

`SHOW CREATE VIEW` and `SHOW CREATE TABLE` return MySQL-shaped
`ALGORITHM=TEMPTABLE` definitions with the `mysql.sys@localhost` definer.

## Unsupported Behavior

This slice intentionally does not implement:

- physical Performance Schema source tables for processlist execution;
- live statement/wait/stage/transaction timers;
- rows examined/sent/affected or temporary-table counters beyond deterministic
  zero placeholders;
- memory summary values;
- current or previous wait details;
- last-statement text and latency;
- process id or program-name connection attributes;
- sys helper-function execution;
- privilege filtering, definer validation, SQL SECURITY enforcement, or true
  updatable-view writes;
- broader sys view execution.

Writes remain blocked by the built-in schema write guard.

## Parser And Grammar

No Lemon grammar changes are required. Existing qualified and selected-schema
table references, `SHOW COLUMNS`, `SHOW INDEX`, `SHOW CREATE VIEW`,
`SHOW CREATE TABLE`, and `INFORMATION_SCHEMA` query support are sufficient.

## Architecture

- Public API: unchanged.
- Parser/AST: unchanged.
- Runtime metadata: extends the synthetic sys core descriptors with formatted
  and raw processlist view entries.
- Query execution: reuses the existing synthetic system-table SELECT planner
  and builds rows from `mylite_connection_collect_processlist_sessions()`.
- Processlist registry: snapshots now include autocommit and user-transaction
  flags so sys rows can be built without reading other connections after the
  registry mutex is released.
- SHOW metadata: reuses the built-in sys view `SHOW CREATE` path and synthetic
  column/index paths.
- Storage/SQLite: unchanged.

## Performance

Row building is O(open MyLite connections). It does not scan SQLite user data or
attempt live Performance Schema instrumentation.

## Tests

MySQL 8.4.9 expectation coverage verifies:

- formatted and raw column metadata;
- information-schema table, view, dependency, and empty constraint/index rows;
- selected-schema `SHOW CREATE` behavior;
- current-session row shape;
- `ROW_COUNT() = -1` and zero warnings.

MyLite runtime coverage verifies:

- current-session rows for both views;
- formatted/raw metadata differences;
- view and routine dependency metadata;
- `SHOW CREATE` fragments;
- selected-schema access.
