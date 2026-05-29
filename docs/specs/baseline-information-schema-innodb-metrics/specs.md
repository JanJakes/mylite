# Baseline INFORMATION_SCHEMA INNODB_METRICS

## Status

This phase adds `INFORMATION_SCHEMA.INNODB_METRICS` as a MySQL-shaped
synthetic information-schema system view. The view is queryable, exposes MySQL
8.4.9 table and column metadata, and returns no rows in MyLite because MyLite
does not have InnoDB monitor counters to expose.

The slice is metadata-only. It does not add InnoDB monitor state, counter
enable/disable/reset variables, background instrumentation, Performance Schema
integration, or physical storage-engine statistics.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing empty InnoDB system-view specs and tests, especially
  `docs/specs/baseline-information-schema-innodb-trx/specs.md`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.INNODB_METRICS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-metrics-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_METRICS` exists as an `information_schema`
  `SYSTEM VIEW`.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL` for the view.
- `INNODB_METRICS` has 17 columns in order:
  `NAME`, `SUBSYSTEM`, `COUNT`, `MAX_COUNT`, `MIN_COUNT`, `AVG_COUNT`,
  `COUNT_RESET`, `MAX_COUNT_RESET`, `MIN_COUNT_RESET`, `AVG_COUNT_RESET`,
  `TIME_ENABLED`, `TIME_DISABLED`, `TIME_ELAPSED`, `TIME_RESET`, `STATUS`,
  `TYPE`, and `COMMENT`.
- Character columns use `varchar(193)` metadata with `utf8mb3` /
  `utf8mb3_general_ci`. Counter columns use `bigint` metadata. Average
  columns use `float(12,0)`. Time columns use `datetime`.
- The observed runtime returned 314 monitor rows, with dynamic counter values,
  timestamps, elapsed time, and enabled/disabled state. Those rows depend on
  InnoDB monitor configuration and runtime activity.
- Successful reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports
  `-1`.

Representative probe:

```sh
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names <<'SQL'
SELECT VERSION();
SHOW FULL TABLES FROM INFORMATION_SCHEMA
 WHERE Tables_in_INFORMATION_SCHEMA = 'INNODB_METRICS';
SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,
       DATA_LENGTH,AUTO_INCREMENT
  FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA='information_schema'
   AND TABLE_NAME='INNODB_METRICS';
SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,
       DATA_TYPE,CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,
       NUMERIC_PRECISION,NUMERIC_SCALE,DATETIME_PRECISION,
       CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES
  FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA='information_schema'
   AND TABLE_NAME='INNODB_METRICS'
 ORDER BY ORDINAL_POSITION;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_METRICS;
SELECT NAME,SUBSYSTEM,STATUS,TYPE
  FROM INFORMATION_SCHEMA.INNODB_METRICS
 ORDER BY NAME
 LIMIT 10;
SELECT @@warning_count, ROW_COUNT();
SQL
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_METRICS` using the existing
  information-schema query subset;
- wildcard projection with the MySQL-observed column order;
- case-insensitive information-schema table name lookup;
- table aliases, predicates, ordering, and `COUNT(*)` through the existing
  metadata query path;
- unqualified reads while `information_schema` is the selected schema;
- stable empty-row behavior, including after MyLite tables and indexes are
  created;
- system metadata through `INFORMATION_SCHEMA.TABLES`,
  `INFORMATION_SCHEMA.COLUMNS`, `SHOW TABLES`, `SHOW FULL TABLES`, and
  `SHOW TABLE STATUS` via the existing built-in table directory.

Out of scope:

- dynamic InnoDB monitor rows;
- counter collection, min/max/average tracking, timestamps, elapsed time,
  enable/disable state, reset behavior, and monitor comments;
- `innodb_monitor_enable`, `innodb_monitor_disable`, `innodb_monitor_reset`,
  or `innodb_monitor_reset_all` side effects;
- `PROCESS` privilege checks or account-specific visibility;
- Performance Schema transaction or storage-engine instrumentation;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, identifiers, predicates, and ordering.
- Analyzer/runtime: recognizes the view as a supported information-schema
  system view and returns an empty row set.
- Catalog metadata: unchanged. Existing table, index, and storage descriptors
  are not used to calculate monitor rows in this slice.
- Storage/SQLite: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT * FROM INFORMATION_SCHEMA.INNODB_METRICS;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_METRICS;
SELECT m.NAME, m.STATUS
  FROM INFORMATION_SCHEMA.INNODB_METRICS AS m
 WHERE m.SUBSYSTEM = 'buffer'
 ORDER BY m.NAME;
SELECT COUNT(*) FROM INNODB_METRICS;
```

## Runtime Semantics

`INNODB_METRICS` is registered in the static information-schema table registry.
Row production is intentionally empty:

- system rows for `TABLES` and `COLUMNS` are generated from static
  descriptors;
- no rows are generated from MyLite table, index, transaction, or SQLite
  activity because MyLite does not expose InnoDB monitor counters;
- successful reads introduce no warnings;
- `ROW_COUNT()` after a successful `SELECT` remains the existing query value
  `-1`.

## Diagnostics

The feature relies on existing information-schema diagnostics:

- unknown selected columns fail with the current unknown-column diagnostic;
- unsupported expressions, joins, grouping, predicates, and limits retain the
  current information-schema query subset behavior;
- allocation failures use existing MyLite runtime diagnostics.

Successful reads introduce no warnings.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- wildcard column labels and empty row set;
- row counts and representative predicates over text, numeric, and datetime
  columns;
- case-insensitive table-name lookup;
- alias projection through the existing information-schema query path;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view row;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all 17 columns;
- `USE information_schema` unqualified-table reads;
- stable empty-row behavior after creating a MyLite table and secondary index;
- MySQL 8.4.9 runtime observation of dynamic monitor rows, stored in the
  expectation script as explicit out-of-scope behavior.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_metrics_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_metrics|information_schema_innodb_buffer_pool_stats|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_metrics_expectations.sh
git diff --check
cmake --workflow --preset check
```
