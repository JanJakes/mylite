# Baseline Performance Schema Setup Metrics Table

## Scope

This baseline covers read-only metadata and MySQL 8.4.9 default rows for
`performance_schema.setup_metrics`.

MyLite exposes the table through the built-in Performance Schema catalog with
the same column shape, HASH primary-key metadata, table-status metadata, and
default telemetry metric rows observed in the target MySQL runtime. The slice
does not implement OpenTelemetry export, metric collection, mutable telemetry
configuration, or live counter values.

## Compatibility Sources

- MySQL 8.4 Reference Manual: Performance Schema `setup_metrics` table
  <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-setup-metrics-table.html>
- Runtime probes against MySQL 8.4.9 using
  `docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot`.

## MySQL Runtime Observations

`setup_metrics` has six columns:

| Column | Type | Null | Key | Default |
| --- | --- | --- | --- | --- |
| `NAME` | `varchar(63)` | `NO` | `PRI` | `NULL` |
| `METER` | `varchar(63)` | `NO` | | `NULL` |
| `METRIC_TYPE` | `enum('ASYNC COUNTER','ASYNC UPDOWN COUNTER','ASYNC GAUGE COUNTER')` | `NO` | | `NULL` |
| `NUM_TYPE` | `enum('INTEGER','DOUBLE')` | `NO` | | `NULL` |
| `UNIT` | `varchar(63)` | `YES` | | `NULL` |
| `DESCRIPTION` | `varchar(1023)` | `YES` | | `NULL` |

The table reports a `HASH` primary key on `NAME` through `SHOW INDEX` and the
Information Schema constraint tables. The observed default row set contains 422
rows, even though ten metric names appear under more than one meter. MyLite
mirrors the metadata and rows as an introspection surface; it does not enforce
the Performance Schema pseudo-key.

`SHOW TABLE STATUS` reports `ENGINE = PERFORMANCE_SCHEMA`, `ROW_FORMAT =
Dynamic`, `TABLE_ROWS = 422`, `AUTO_INCREMENT = NULL`, and collation
`utf8mb4_0900_ai_ci`.

Observed row counts by meter:

| Meter | Rows |
| --- | ---: |
| `mysql.inno` | 34 |
| `mysql.inno.buffer_pool` | 15 |
| `mysql.inno.data` | 8 |
| `mysql.myisam` | 7 |
| `mysql.perf_schema` | 34 |
| `mysql.stats` | 57 |
| `mysql.stats.com` | 167 |
| `mysql.stats.connection` | 7 |
| `mysql.stats.handler` | 18 |
| `mysql.stats.ssl` | 13 |
| `mysql.x` | 46 |
| `mysql.x.stmt` | 16 |

The ordered row payload
`NAME, METER, METRIC_TYPE, NUM_TYPE, UNIT, DESCRIPTION` sorted by
`METER, NAME, DESCRIPTION` has MySQL SHA-256 digest
`973c99e28c4949fb057d37e42a439ba0e5aae8f134ff96005e39f4cc4b22ce50`.

## MyLite Behavior

MyLite exposes `performance_schema.setup_metrics` through the shared built-in
system-table path:

- `SELECT`, projections, predicates, ordering, and `COUNT(*)` work
  through the common MySQL-system-table query path.
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

This slice intentionally does not implement metric collection, current metric
values, OpenTelemetry export, runtime edits to setup rows, plugin-dependent row
mutation, or `performance_schema.setup_instruments`.

## Parser, Storage, And SQLite Integration

No grammar changes are required. This is a MyLite wrapper/catalog feature that
appends static descriptor rows generated from MySQL 8.4.9 runtime observations.
It does not require SQLite public extension APIs, virtual tables, or targeted
SQLite fork patches.

## Tests

The MySQL expectation script verifies MySQL 8.4.9 columns, primary-key metadata,
constraints, table status, information-schema table metadata, representative
rows, per-meter counts, and the full ordered row-set digest. The C runtime test
verifies the same MyLite surfaces, a full row-set FNV-1a digest, selected-schema
resolution, and write protection.
