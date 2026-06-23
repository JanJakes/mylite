# Baseline sys.session Views

This slice adds MySQL-shaped metadata and deterministic partial rowsets for
the `sys.session`, `sys.x$session`, and `sys.session_ssl_status` views.

MySQL defines `sys.session` and `sys.x$session` as user-session filters over
`sys.processlist` and `sys.x$processlist`. MyLite does not have background
Performance Schema threads, so every MyLite processlist snapshot is a user
session and these views reuse the processlist-backed row builder.

MySQL defines `sys.session_ssl_status` from
`performance_schema.status_by_thread`. MyLite's embedded `libmylite` runtime has
no TLS transport and no Performance Schema status-by-thread table, so it
returns one row per MyLite connection with the thread-id placeholder, empty SSL
version/cipher values, and `ssl_sessions_reused = '0'`, matching MySQL's
observable no-SSL connection shape.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.session` and `sys.x$session`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-session.html>
- MySQL 8.4 Reference Manual, `sys.session_ssl_status`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-session-ssl-status.html>
- MySQL 8.4 Reference Manual, `sys.processlist` and `sys.x$processlist`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-processlist.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_session_views_expectations.sh`.

Runtime probes verified formatted/raw session column metadata, SSL-status
column metadata, view definitions, dependency metadata, table/status metadata,
empty index/constraint metadata, selected-schema behavior, live session rows,
no-SSL SSL-status values, warnings, and `ROW_COUNT()` behavior.

## Supported Behavior

Supported reads:

```sql
SELECT * FROM sys.session;
SELECT * FROM sys.`x$session`;
SELECT * FROM sys.session_ssl_status;

USE sys;
SELECT * FROM session;
SELECT * FROM `x$session`;
SELECT * FROM session_ssl_status;
```

`sys.session` and `sys.x$session` expose the same MyLite open-connection rows
as `sys.processlist` and `sys.x$processlist`. MyLite does not create Daemon or
background processlist rows, so no additional filtering is needed.

`sys.session_ssl_status` returns one row per open MyLite connection:

- `thread_id`: the MyLite connection id as a thread-id placeholder;
- `ssl_version`: empty string;
- `ssl_cipher`: empty string;
- `ssl_sessions_reused`: `0`.

## Metadata

`sys.session` has the same 30 formatted columns as `sys.processlist`.
`sys.x$session` has the same 30 raw columns as `sys.x$processlist`.
`sys.session_ssl_status` has four columns:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `thread_id` | `bigint unsigned` | `NO` | `NULL` | SQL `NULL` |
| `ssl_version` | `varchar(1024)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `ssl_cipher` | `varchar(1024)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |
| `ssl_sessions_reused` | `varchar(1024)` | `YES` | `NULL` | `utf8mb4_0900_ai_ci` |

`SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and
`INFORMATION_SCHEMA.COLUMNS` expose these shapes. The views have no index or
constraint metadata, so `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`,
`TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
`TABLE_CONSTRAINTS_EXTENSIONS` return zero rows.

`INFORMATION_SCHEMA.VIEWS` exposes all three built-in view rows with:

- `TABLE_CATALOG = 'def'`
- `TABLE_SCHEMA = 'sys'`
- `CHECK_OPTION = 'NONE'`
- `IS_UPDATABLE = 'NO'`
- `DEFINER = 'mysql.sys@localhost'`
- `SECURITY_TYPE = 'INVOKER'`
- `CHARACTER_SET_CLIENT = 'utf8mb4'`
- `COLLATION_CONNECTION = 'utf8mb4_0900_ai_ci'`

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports:

- `sys.session -> sys.processlist`
- `sys.session -> sys.sys_config`
- `sys.x$session -> sys.x$processlist`
- `sys.session_ssl_status -> performance_schema.status_by_thread`

`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` returns no rows for these views.

`SHOW CREATE VIEW` and `SHOW CREATE TABLE` return MySQL-shaped built-in view
definitions. `sys.session` and `sys.x$session` use
`ALGORITHM=UNDEFINED`; `sys.session_ssl_status` uses `ALGORITHM=MERGE`.

## Unsupported Behavior

This slice intentionally does not implement:

- physical `performance_schema.status_by_thread` rows;
- TLS transport state, TLS version/cipher discovery, or reused-session counts;
- background-thread and daemon filtering beyond the absence of such rows in
  MyLite's embedded processlist registry;
- physical execution of sys view definitions;
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
- Runtime metadata: extends the synthetic sys core descriptors with session and
  SSL-status view entries.
- Query execution: reuses the processlist row builder for `sys.session` and
  `sys.x$session`; adds a small processlist-snapshot row builder for
  `sys.session_ssl_status`.
- Processlist registry: reused as-is.
- SHOW metadata: reuses the synthetic system-table column/index paths and the
  built-in sys view `SHOW CREATE` path.
- Storage/SQLite: unchanged.

## Performance

Row building is O(open MyLite connections). It does not scan SQLite user data or
attempt live Performance Schema instrumentation.

## Tests

MySQL 8.4.9 expectation coverage verifies:

- formatted/raw session column metadata;
- SSL-status column metadata and no-SSL values;
- information-schema table, view, dependency, and empty constraint/index rows;
- selected-schema `SHOW CREATE` behavior;
- current-session rows;
- `ROW_COUNT() = -1` and zero warnings.

MyLite runtime coverage verifies:

- current-session rows for `sys.session` and `sys.x$session`;
- no-SSL placeholder rows for `sys.session_ssl_status`;
- formatted/raw/SSL metadata differences;
- view and dependency metadata;
- `SHOW CREATE` fragments;
- selected-schema access.
