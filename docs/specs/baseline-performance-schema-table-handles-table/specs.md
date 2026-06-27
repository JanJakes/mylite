# Baseline Performance Schema Table Handles Table

## Scope

This baseline covers read-only metadata and empty-row behavior for
`performance_schema.table_handles`.

MyLite exposes the table through the built-in Performance Schema catalog with
the same column shape, HASH index metadata, table-status metadata, and default
empty row set observed in the target MySQL runtime. The slice does not
implement live table-lock instrumentation, open table handles, lock wait
tracking, or mutable Performance Schema instrumentation setup.

## Compatibility Sources

- MySQL 8.4 Reference Manual: Performance Schema `table_handles` table
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-table-handles-table.html>
- Runtime probes against MySQL 8.4.9 using
  `docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot`.

## MySQL Runtime Observations

`table_handles` has eight columns:

| Column | Type | Null | Key | Default |
| --- | --- | --- | --- | --- |
| `OBJECT_TYPE` | `varchar(64)` | `NO` | `MUL` | `NULL` |
| `OBJECT_SCHEMA` | `varchar(64)` | `NO` | | `NULL` |
| `OBJECT_NAME` | `varchar(64)` | `NO` | | `NULL` |
| `OBJECT_INSTANCE_BEGIN` | `bigint unsigned` | `NO` | `PRI` | `NULL` |
| `OWNER_THREAD_ID` | `bigint unsigned` | `YES` | `MUL` | `NULL` |
| `OWNER_EVENT_ID` | `bigint unsigned` | `YES` | | `NULL` |
| `INTERNAL_LOCK` | `varchar(64)` | `YES` | | `NULL` |
| `EXTERNAL_LOCK` | `varchar(64)` | `YES` | | `NULL` |

The table reports three HASH indexes through `SHOW INDEX` and
`INFORMATION_SCHEMA.STATISTICS`:

- `PRIMARY` on `OBJECT_INSTANCE_BEGIN`.
- Nonunique `OBJECT_TYPE` on `OBJECT_TYPE`, `OBJECT_SCHEMA`, `OBJECT_NAME`.
- Nonunique `OWNER_THREAD_ID` on `OWNER_THREAD_ID`, `OWNER_EVENT_ID`.

`INFORMATION_SCHEMA.TABLE_CONSTRAINTS` reports only the primary-key constraint.
`INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS` reports rows for `PRIMARY`,
`OBJECT_TYPE`, and `OWNER_THREAD_ID`.

In the default MySQL 8.4.9 runtime used by MyLite compatibility tests,
`SELECT COUNT(*) FROM performance_schema.table_handles` returns `0`.
`SHOW TABLE STATUS` reports `ENGINE = PERFORMANCE_SCHEMA`, `ROW_FORMAT =
Dynamic`, `TABLE_ROWS = 0`, `AUTO_INCREMENT = NULL`, and collation
`utf8mb4_0900_ai_ci`.

## MyLite Behavior

MyLite exposes `performance_schema.table_handles` through the shared built-in
system-table path:

- `SELECT`, projections, predicates, ordering, and `COUNT(*)` work through the
  common MySQL-system-table query path and return an empty row set.
- `USE performance_schema` resolves unqualified table references.
- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.STATISTICS`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`,
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS`,
  `INFORMATION_SCHEMA.TABLES`, and `SHOW TABLE STATUS` expose MySQL-shaped
  metadata.
- DDL, DML, and `TRUNCATE TABLE` writes against `performance_schema` continue
  to return access-denied diagnostics.

This slice intentionally does not implement lock instrumentation rows, table
handle memory addresses, owner event mapping, lock mode state, or
`setup_instruments` mutation for `wait/lock/table/sql/handler`.

## Parser, Storage, And SQLite Integration

No grammar changes are required. This is a MyLite wrapper/catalog feature that
adds a static table descriptor and routes queries to an empty row set. It does
not require SQLite public extension APIs, virtual tables, storage changes, or
targeted SQLite fork patches.

## Tests

The MySQL expectation script verifies MySQL 8.4.9 columns, indexes,
constraints, table status, information-schema table metadata, and the default
empty row set. The C runtime test verifies the same MyLite surfaces,
selected-schema resolution, empty-row predicates, and write protection.
