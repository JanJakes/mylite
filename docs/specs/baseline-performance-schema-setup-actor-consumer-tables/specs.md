# Baseline Performance Schema Setup Actor/Consumer Tables

## Scope

This baseline covers the queryable metadata placeholder surface for:

- `performance_schema.setup_actors`
- `performance_schema.setup_consumers`

The target behavior is MySQL 8.4.9-compatible table shape, metadata, default
rows, and read-only diagnostics for embedded MyLite. MyLite does not implement
mutable Performance Schema instrumentation state in this slice.

## Compatibility Sources

- MySQL 8.4 Reference Manual: `setup_actors`
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-setup-actors-table.html>
- MySQL 8.4 Reference Manual: `setup_consumers`
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-setup-consumers-table.html>
- Runtime probes against MySQL 8.4.9 using
  `docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot`.

## MySQL Runtime Observations

`setup_actors` has five columns:

| Column | Type | Null | Key | Default | Charset/Collation |
| --- | --- | --- | --- | --- | --- |
| `HOST` | `char(255)` | `NO` | `PRI` | `%` | `ascii` / `ascii_general_ci` |
| `USER` | `char(32)` | `NO` | `PRI` | `%` | `utf8mb4` / `utf8mb4_bin` |
| `ROLE` | `char(32)` | `NO` | `PRI` | `%` | `utf8mb4` / `utf8mb4_bin` |
| `ENABLED` | `enum('YES','NO')` | `NO` | | `YES` | `utf8mb4_0900_ai_ci` |
| `HISTORY` | `enum('YES','NO')` | `NO` | | `YES` | `utf8mb4_0900_ai_ci` |

The table has a `HASH` primary key on `HOST`, `USER`, `ROLE`. A default MySQL
8.4.9 server returns one row: `('%', '%', '%', 'YES', 'YES')`. Metadata table
status reports `ENGINE = PERFORMANCE_SCHEMA`, `ROW_FORMAT = Fixed`,
`TABLE_ROWS = 128`, `AUTO_INCREMENT = NULL`, and collation
`utf8mb4_0900_ai_ci`.

`setup_consumers` has two columns:

| Column | Type | Null | Key | Default | Charset/Collation |
| --- | --- | --- | --- | --- | --- |
| `NAME` | `varchar(64)` | `NO` | `PRI` | `NULL` | `utf8mb4_0900_ai_ci` |
| `ENABLED` | `enum('YES','NO')` | `NO` | | `NULL` | `utf8mb4_0900_ai_ci` |

The table has a `HASH` primary key on `NAME`. A default MySQL 8.4.9 server
returns the observed consumer rows:

| NAME | ENABLED |
| --- | --- |
| `events_stages_current` | `NO` |
| `events_stages_history` | `NO` |
| `events_stages_history_long` | `NO` |
| `events_statements_cpu` | `NO` |
| `events_statements_current` | `YES` |
| `events_statements_history` | `YES` |
| `events_statements_history_long` | `NO` |
| `events_transactions_current` | `YES` |
| `events_transactions_history` | `YES` |
| `events_transactions_history_long` | `NO` |
| `events_waits_current` | `NO` |
| `events_waits_history` | `NO` |
| `events_waits_history_long` | `NO` |
| `global_instrumentation` | `YES` |
| `statements_digest` | `YES` |
| `thread_instrumentation` | `YES` |

Metadata table status reports `ENGINE = PERFORMANCE_SCHEMA`,
`ROW_FORMAT = Dynamic`, `TABLE_ROWS = 16`, `AUTO_INCREMENT = NULL`, and
collation `utf8mb4_0900_ai_ci`.

## MyLite Behavior

MyLite exposes both tables through the existing built-in system-table catalog
as descriptor-backed read-only tables:

- `SELECT`, projections, predicates, ordering, and `COUNT(*)` work through the
  common MySQL-system-table query path.
- `USE performance_schema` resolves unqualified table names for these tables.
- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.STATISTICS`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`,
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS`,
  `INFORMATION_SCHEMA.TABLES`, and `SHOW TABLE STATUS` expose MySQL-shaped
  metadata.
- DDL and DML writes against `performance_schema` continue to return
  `1044 / 42000` access-denied diagnostics.

This slice intentionally does not implement:

- writes to setup tables;
- `TRUNCATE` support for `setup_actors`;
- runtime instrumentation changes from setup table mutation;
- live foreground-thread, consumer, event, history, or digest storage;
- `setup_objects`, `setup_instruments`, or `setup_threads`.

## Storage And SQLite Integration

The implementation is a MyLite wrapper/catalog feature. It uses the existing
system-table descriptor and row-set machinery and does not require a SQLite
public extension hook, virtual table, or targeted SQLite fork patch.

## Tests

The MySQL expectation script verifies MySQL 8.4.9 columns, indexes,
constraints, table status, information-schema table metadata, and default rows.
The C runtime test verifies the same MyLite surfaces plus selected-schema
resolution and write protection.
