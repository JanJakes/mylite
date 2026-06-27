# Baseline Performance Schema Setup Threads Table

## Scope

This baseline covers the queryable metadata placeholder surface for
`performance_schema.setup_threads`.

MyLite exposes MySQL 8.4.9-shaped read-only setup-thread class rows. It does
not implement mutable thread instrumentation configuration or live thread
creation state in this slice.

## Compatibility Sources

- MySQL 8.4 Reference Manual: `setup_threads`
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-setup-threads-table.html>
- Runtime probes against MySQL 8.4.9 using
  `docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot`.

## MySQL Runtime Observations

`setup_threads` has six columns:

| Column | Type | Null | Key | Default |
| --- | --- | --- | --- | --- |
| `NAME` | `varchar(128)` | `NO` | `PRI` | `NULL` |
| `ENABLED` | `enum('YES','NO')` | `NO` | | `NULL` |
| `HISTORY` | `enum('YES','NO')` | `NO` | | `NULL` |
| `PROPERTIES` | `set('singleton','user')` | `NO` | | `NULL` |
| `VOLATILITY` | `int` | `NO` | | `NULL` |
| `DOCUMENTATION` | `longtext` | `YES` | | `NULL` |

Character columns use `utf8mb4_0900_ai_ci`. The table has a `HASH` primary key
on `NAME`. `SHOW TABLE STATUS` reports `ENGINE = PERFORMANCE_SCHEMA`,
`ROW_FORMAT = Dynamic`, `TABLE_ROWS = 100`, `AUTO_INCREMENT = NULL`, and
collation `utf8mb4_0900_ai_ci`.

A default MySQL 8.4.9 server returns 56 rows. Every row has `ENABLED = YES`,
`HISTORY = YES`, `VOLATILITY = 0`, and `DOCUMENTATION = NULL`. `PROPERTIES`
contains either `singleton`, `user`, or the empty set depending on thread class.

## MyLite Behavior

MyLite exposes `performance_schema.setup_threads` through the built-in
system-table catalog:

- `SELECT`, projections, predicates, ordering, and `COUNT(*)` work through the
  common MySQL-system-table query path.
- `USE performance_schema` resolves unqualified `setup_threads` references.
- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.STATISTICS`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`,
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS`,
  `INFORMATION_SCHEMA.TABLES`, and `SHOW TABLE STATUS` expose MySQL-shaped
  metadata.
- DDL and DML writes against `performance_schema` continue to return
  `1044 / 42000` access-denied diagnostics.

This slice intentionally does not implement writes to `setup_threads`,
`TRUNCATE`, mutable instrumentation filters, live server thread discovery, or
`performance_schema.threads` rows.

## Storage And SQLite Integration

The implementation is a MyLite wrapper/catalog feature. It uses descriptor
metadata and static row appenders. It does not require SQLite public extension
APIs, virtual tables, or targeted SQLite fork patches.

## Tests

The MySQL expectation script verifies MySQL 8.4.9 columns, default rows,
primary-key metadata, constraints, table status, and information-schema table
metadata. The C runtime test verifies the same MyLite surfaces plus
selected-schema resolution and write protection.
