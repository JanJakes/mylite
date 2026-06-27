# Baseline Performance Schema Setup Logger And Meter Tables

## Scope

This baseline covers read-only metadata and default rows for
`performance_schema.setup_loggers` and `performance_schema.setup_meters`.

MyLite exposes MySQL 8.4.9-shaped descriptors for both tables and returns the
default rows observed in the target MySQL runtime. The slice does not implement
mutable Performance Schema telemetry configuration, periodic metric export, or
dynamic logging-level changes.

## Compatibility Sources

- MySQL 8.4 Reference Manual: Performance Schema `setup_meters` table
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-setup-meters-table.html>
- MySQL 8.4 Reference Manual: Performance Schema telemetry tables
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-telemetry-tables.html>
- Runtime probes against MySQL 8.4.9 using
  `docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot`.

## MySQL Runtime Observations

`setup_loggers` has three columns:

| Column | Type | Null | Key | Default |
| --- | --- | --- | --- | --- |
| `NAME` | `varchar(128)` | `NO` | | `NULL` |
| `LEVEL` | `enum('none','error','warn','info','debug')` | `NO` | | `NULL` |
| `DESCRIPTION` | `varchar(1023)` | `YES` | | `NULL` |

The table has no indexes or constraints. `SHOW TABLE STATUS` reports
`ENGINE = PERFORMANCE_SCHEMA`, `ROW_FORMAT = Dynamic`, `TABLE_ROWS = 1`,
`AUTO_INCREMENT = NULL`, and collation `utf8mb4_0900_ai_ci`.

The MySQL 8.4.9 target runtime returns one default logger row:

| NAME | LEVEL | DESCRIPTION |
| --- | --- | --- |
| `logger/error/error_log` | `info` | `MySQL error logger` |

`setup_meters` has four columns:

| Column | Type | Null | Key | Default |
| --- | --- | --- | --- | --- |
| `NAME` | `varchar(63)` | `NO` | `PRI` | `NULL` |
| `FREQUENCY` | `mediumint unsigned` | `NO` | | `NULL` |
| `ENABLED` | `enum('YES','NO')` | `NO` | | `NULL` |
| `DESCRIPTION` | `varchar(1023)` | `YES` | | `NULL` |

The table has a `HASH` primary key on `NAME`. `SHOW TABLE STATUS` reports
`ENGINE = PERFORMANCE_SCHEMA`, `ROW_FORMAT = Dynamic`, `TABLE_ROWS = 12`,
`AUTO_INCREMENT = NULL`, and collation `utf8mb4_0900_ai_ci`.

The MySQL 8.4.9 target runtime returns twelve meter rows:

| NAME | FREQUENCY | ENABLED | DESCRIPTION |
| --- | --- | --- | --- |
| `mysql.inno` | `10` | `YES` | `MySql InnoDB metrics` |
| `mysql.inno.buffer_pool` | `10` | `YES` | `MySql InnoDB buffer pool metrics` |
| `mysql.inno.data` | `10` | `YES` | `MySql InnoDB data metrics` |
| `mysql.myisam` | `10` | `YES` | `MySql MyISAM storage engine stats` |
| `mysql.perf_schema` | `10` | `YES` | `MySql performance_schema lost instruments` |
| `mysql.stats` | `10` | `YES` | `MySql core metrics` |
| `mysql.stats.com` | `10` | `YES` | `MySql command stats` |
| `mysql.stats.connection` | `10` | `YES` | `MySql connection stats` |
| `mysql.stats.handler` | `10` | `YES` | `MySql handler stats` |
| `mysql.stats.ssl` | `10` | `YES` | `MySql TLS related stats` |
| `mysql.x` | `10` | `YES` | `MySql X plugin metrics` |
| `mysql.x.stmt` | `10` | `YES` | `MySql X plugin statement statistics` |

## MyLite Behavior

MyLite exposes both tables through the built-in system-table catalog:

- `SELECT`, projections, predicates, ordering, and `COUNT(*)` work through the
  common MySQL-system-table query path.
- `USE performance_schema` resolves unqualified table references.
- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.STATISTICS`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`,
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS`,
  `INFORMATION_SCHEMA.TABLES`, and `SHOW TABLE STATUS` expose MySQL-shaped
  metadata.
- DDL and DML writes against `performance_schema` continue to return
  `1044 / 42000` access-denied diagnostics.

This slice intentionally does not implement updates to logger levels, meter
frequency, meter enabled state, telemetry export, persisted telemetry
configuration, or full `setup_metrics` row support.

## Parser, Storage, And SQLite Integration

No grammar changes are required. The implementation is a MyLite
wrapper/catalog feature that appends static descriptor rows. It does not
require SQLite public extension APIs, virtual tables, or targeted SQLite fork
patches.

## Tests

The MySQL expectation script verifies MySQL 8.4.9 columns, primary-key and
no-key metadata, constraints, table status, information-schema table metadata,
and default row sets. The C runtime test verifies the same MyLite surfaces plus
selected-schema resolution and write protection.
