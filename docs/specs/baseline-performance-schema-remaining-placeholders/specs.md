# Baseline Performance Schema Remaining Metadata Placeholders

## Scope

This slice covers the remaining red Performance Schema baseline tables:

- `performance_schema.error_log`
- `performance_schema.log_status`
- `performance_schema.setup_instruments`

MyLite exposes MySQL 8.4.9-shaped metadata for these tables and implements
read-only placeholder rows where that is useful for embedded applications. The
slice does not implement live server error-log buffering, binary-log backup
state, replication-log state, storage-engine log inventory, or a full mutable
Performance Schema instrumentation registry.

## Compatibility Sources

Official MySQL 8.4 reference pages used for the feature surface:

- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-error-log-table.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-log-status-table.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-setup-instruments-table.html>

Expected metadata was verified against a local MySQL 8.4.9 runtime container:

```sh
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names
```

The verification script for this slice is:

```sh
packages/libmylite/tests/mysql_baseline_performance_schema_remaining_placeholders_expectations.sh
```

## Semantics

`error_log` is exposed as an empty read-only table with MySQL-shaped columns,
primary key metadata, secondary index metadata, information-schema metadata,
`SHOW COLUMNS`, `DESCRIBE`, `SHOW INDEX`, and `SHOW TABLE STATUS`. MyLite does
not keep a Performance Schema error-log ring buffer in this slice, so row reads
return zero rows and table-status metadata reports `TABLE_ROWS = 0`. The MySQL
target runtime's `error_log.TABLE_ROWS` value is intentionally not locked into
tests because it changes with transient server startup and diagnostic events.

`log_status` is exposed as a read-only one-row placeholder. The row uses
MyLite's deterministic server UUID and valid empty JSON objects for `LOCAL`,
`REPLICATION`, and `STORAGE_ENGINES`. This gives backup/status probes a stable
object-shaped result without pretending to expose binary-log, relay-log, or
storage-engine checkpoint state.

`setup_instruments` is exposed with MySQL 8.4.9-shaped columns and a compact
deterministic default-instrument inventory. The inventory covers representative
statement, memory, stage, socket, idle, Performance Schema mutex, binary-log
mutex, and TC-log mutex classes that align with MyLite's existing
`sys.ps_is_instrument_default_enabled()` and
`sys.ps_is_instrument_default_timed()` helpers. It intentionally does not embed
the full MySQL target runtime's thousand-plus instrument rows.

All three tables are read-only in MyLite. DDL, truncation, renames, and
single-table DML writes targeting `performance_schema` keep returning MyLite's
existing `1044 / 42000` access-denied diagnostic.

## Metadata

`error_log` columns:

- `LOGGED timestamp(6) NOT NULL`, primary key.
- `THREAD_ID bigint unsigned NULL`, secondary HASH index.
- `PRIO enum('System','Error','Warning','Note') NOT NULL`, secondary HASH index.
- `ERROR_CODE varchar(10) NULL`, secondary HASH index.
- `SUBSYSTEM varchar(7) NULL`, secondary HASH index.
- `DATA text NOT NULL`.

`log_status` columns:

- `SERVER_UUID char(36) NOT NULL` with `utf8mb4_bin` collation.
- `LOCAL json NOT NULL`.
- `REPLICATION json NOT NULL`.
- `STORAGE_ENGINES json NOT NULL`.

`log_status` has no indexes or constraints.

`setup_instruments` columns:

- `NAME varchar(128) NOT NULL`, primary key.
- `ENABLED enum('YES','NO') NOT NULL`.
- `TIMED enum('YES','NO') NULL`.
- `PROPERTIES set('singleton','progress','user','global_statistics','mutable','controlled_by_default') NOT NULL`.
- `FLAGS set('controlled') NULL`.
- `VOLATILITY int NOT NULL`.
- `DOCUMENTATION longtext NULL`.

`setup_instruments` has a HASH primary key on `NAME`.

## Parser, Storage, And SQLite Integration

No grammar changes are required. The implementation is a MyLite
wrapper/catalog feature using MyLite-owned built-in table descriptors and row
appenders. It does not require SQLite public extension APIs, virtual tables, or
targeted SQLite fork patches.

## Tests

- Verify the expectation script syntax with `sh -n`.
- Run the expectation script against MySQL 8.4.9 to lock observed metadata.
- Build and run
  `mylite_runtime_performance_schema_remaining_placeholders_test`.
- Run the focused CTest filter
  `^libmylite\.runtime\.performance_schema_remaining_placeholders$`.
- Run the broader Performance Schema runtime CTest filter.
- Run `git diff --check`, `git diff --cached --check`, formatting checks, and
  the full `cmake --workflow --preset check` before committing.
