# Baseline Performance Schema Setup Objects Table

## Scope

This baseline covers the queryable metadata placeholder surface for
`performance_schema.setup_objects`.

MyLite exposes MySQL 8.4.9-shaped read-only setup-object filters. It does not
implement mutable Performance Schema object instrumentation state in this
slice.

## Compatibility Sources

- MySQL 8.4 Reference Manual: `setup_objects`
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-setup-objects-table.html>
- Runtime probes against MySQL 8.4.9 using
  `docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot`.

## MySQL Runtime Observations

`setup_objects` has five columns:

| Column | Type | Null | Key | Default |
| --- | --- | --- | --- | --- |
| `OBJECT_TYPE` | `enum('EVENT','FUNCTION','PROCEDURE','TABLE','TRIGGER')` | `NO` | `MUL` | `TABLE` |
| `OBJECT_SCHEMA` | `varchar(64)` | `YES` | | `%` |
| `OBJECT_NAME` | `varchar(64)` | `NO` | | `%` |
| `ENABLED` | `enum('YES','NO')` | `NO` | | `YES` |
| `TIMED` | `enum('YES','NO')` | `NO` | | `YES` |

All columns use `utf8mb4_0900_ai_ci`. The table has a unique `HASH` index named
`OBJECT` over `OBJECT_TYPE`, `OBJECT_SCHEMA`, and `OBJECT_NAME`. MySQL exposes
that index as a `UNIQUE` table constraint with three key-usage rows.

A default MySQL 8.4.9 server returns 20 rows: for each object type (`EVENT`,
`FUNCTION`, `PROCEDURE`, `TABLE`, `TRIGGER`), a wildcard enabled/timed row and
disabled rows for `information_schema`, `mysql`, and `performance_schema`.
Table status reports `ENGINE = PERFORMANCE_SCHEMA`, `ROW_FORMAT = Dynamic`,
`TABLE_ROWS = 128`, `AUTO_INCREMENT = NULL`, and collation
`utf8mb4_0900_ai_ci`.

## MyLite Behavior

MyLite exposes `performance_schema.setup_objects` through the built-in
system-table catalog:

- `SELECT`, projections, predicates, ordering, and `COUNT(*)` work through the
  common MySQL-system-table query path.
- `USE performance_schema` resolves unqualified `setup_objects` references.
- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.STATISTICS`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`,
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS`,
  `INFORMATION_SCHEMA.TABLES`, and `SHOW TABLE STATUS` expose MySQL-shaped
  metadata.
- Repeated secondary-index descriptors with the same name are emitted as
  multi-part index metadata for MySQL system tables.
- DDL and DML writes against `performance_schema` continue to return
  `1044 / 42000` access-denied diagnostics.

This slice intentionally does not implement writes to `setup_objects`,
`TRUNCATE`, mutable instrumentation filters, or live event collection.

## Storage And SQLite Integration

The implementation is a MyLite wrapper/catalog feature. It uses descriptor
metadata and static row appenders. It does not require SQLite public extension
APIs, virtual tables, or targeted SQLite fork patches.

## Tests

The MySQL expectation script verifies MySQL 8.4.9 columns, rows, unique
multi-column index metadata, constraints, table status, and information-schema
table metadata. The C runtime test verifies the same MyLite surfaces plus
selected-schema resolution and write protection.
