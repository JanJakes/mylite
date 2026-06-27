# Baseline Performance Schema User Variables By Thread Table

## Scope

This baseline covers the queryable metadata and current-session row surface for
`performance_schema.user_variables_by_thread`.

MyLite exposes MySQL 8.4.9-shaped read-only metadata for the table and returns
one row for each user-defined variable currently stored in the MyLite session.
The row surface covers text, integer, decimal, and `NULL` values supported by
MyLite user-variable assignment. It does not implement cross-connection user
variable visibility or full binary `longblob` value fidelity in this slice.

## Compatibility Sources

- MySQL 8.4 Reference Manual: Performance Schema user-defined variable tables
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-user-variable-tables.html>
- MySQL 8.4 Reference Manual: User-defined variables
  <https://dev.mysql.com/doc/refman/8.4/en/user-variables.html>
- Runtime probes against MySQL 8.4.9 using
  `docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot`.

## MySQL Runtime Observations

`user_variables_by_thread` has three columns:

| Column | Type | Null | Key | Default |
| --- | --- | --- | --- | --- |
| `THREAD_ID` | `bigint unsigned` | `NO` | `PRI` | `NULL` |
| `VARIABLE_NAME` | `varchar(64)` | `NO` | `PRI` | `NULL` |
| `VARIABLE_VALUE` | `longblob` | `YES` | | `NULL` |

`VARIABLE_NAME` uses `utf8mb4_0900_ai_ci`. The table has a `HASH` primary key
on `(THREAD_ID, VARIABLE_NAME)`. `SHOW TABLE STATUS` reports
`ENGINE = PERFORMANCE_SCHEMA`, `ROW_FORMAT = Dynamic`, `TABLE_ROWS = 2560`,
`AUTO_INCREMENT = NULL`, and collation `utf8mb4_0900_ai_ci`.

The table starts with no rows for a session with no user variables. After
assigning user variables, MySQL returns the current thread id, the variable
name without the leading `@`, and the variable value. User-variable lookup is
case-insensitive; the Performance Schema row preserves the display spelling
from the first assignment. A later assignment with a different spelling updates
the value but not that display spelling. A variable assigned `NULL` has
`VARIABLE_VALUE = NULL`.

## MyLite Behavior

MyLite exposes `performance_schema.user_variables_by_thread` through the
built-in system-table catalog:

- `SELECT`, projections, predicates, ordering, and `COUNT(*)` work through the
  common MySQL-system-table query path.
- `USE performance_schema` resolves unqualified `user_variables_by_thread`
  references.
- `THREAD_ID` is MyLite's handle-local connection id, matching
  `CONNECTION_ID()` and `PS_CURRENT_THREAD_ID()` in MyLite's embedded runtime.
- `VARIABLE_NAME` uses the first assigned display spelling while lookups remain
  case-insensitive through MyLite's canonical lookup key.
- `VARIABLE_VALUE` exposes MyLite's stored textual value bytes for the supported
  user-variable assignment subset and `NULL` for null variables.
- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.STATISTICS`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`,
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS`,
  `INFORMATION_SCHEMA.TABLES`, and `SHOW TABLE STATUS` expose MySQL-shaped
  metadata.
- DDL and DML writes against `performance_schema` continue to return
  `1044 / 42000` access-denied diagnostics.

This slice intentionally does not implement rows for other open MyLite
connections, binary `longblob` values containing embedded NUL bytes,
Performance Schema memory sizing, `performance_schema.threads` integration, or
mutable Performance Schema instrumentation state.

## Storage And SQLite Integration

The implementation is a MyLite wrapper/catalog feature. It uses descriptor
metadata and appends rows from handle-owned session state. It does not require
SQLite public extension APIs, virtual tables, or targeted SQLite fork patches.

## Tests

The MySQL expectation script verifies MySQL 8.4.9 columns, primary-key
metadata, constraints, table status, information-schema table metadata, empty
session behavior, row values after `SET`, mixed-case display-name preservation,
and `NULL` variables. The C runtime test verifies the same MyLite surfaces plus
selected-schema resolution and write protection.
