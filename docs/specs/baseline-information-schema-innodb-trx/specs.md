# Baseline INFORMATION_SCHEMA INNODB_TRX

## Status

This phase adds `INFORMATION_SCHEMA.INNODB_TRX` as a MySQL-shaped synthetic
information-schema system view. The view is queryable, exposes MySQL 8.4.9
table and column metadata, and returns no rows in MyLite because MyLite does
not yet expose InnoDB transaction, lock, or statement instrumentation through
the information schema.

The slice is metadata-only. It does not add transaction monitoring rows, lock
wait reporting, transaction snapshots, `PROCESS` privilege filtering,
Performance Schema lock tables, or mutable InnoDB instrumentation state.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing transaction lifecycle specification:
  `docs/specs/baseline-transaction-lifecycle/specs.md`
- Existing information-schema implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- Existing empty InnoDB system-view specs and tests, especially
  `docs/specs/baseline-information-schema-innodb-temp-table-info/specs.md`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.INNODB_TRX`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-trx-table.html
- MySQL 8.4 Reference Manual, InnoDB transaction and locking information:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-information-schema-transactions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_TRX` exists as an `information_schema`
  `SYSTEM VIEW`.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` reports 25 lower-case columns in order:
  `trx_id`, `trx_state`, `trx_started`, `trx_requested_lock_id`,
  `trx_wait_started`, `trx_weight`, `trx_mysql_thread_id`, `trx_query`,
  `trx_operation_state`, `trx_tables_in_use`, `trx_tables_locked`,
  `trx_lock_structs`, `trx_lock_memory_bytes`, `trx_rows_locked`,
  `trx_rows_modified`, `trx_concurrency_tickets`, `trx_isolation_level`,
  `trx_unique_checks`, `trx_foreign_key_checks`,
  `trx_last_foreign_key_error`, `trx_adaptive_hash_latched`,
  `trx_adaptive_hash_timeout`, `trx_is_read_only`,
  `trx_autocommit_non_locking`, and `trx_schedule_weight`.
- Numeric columns report empty-string defaults and SQL `NULL` character,
  numeric precision, numeric scale, and datetime precision metadata.
- `trx_started` and `trx_wait_started` are `datetime` columns with SQL `NULL`
  character, numeric, and datetime precision metadata.
- String columns use `utf8mb3` and `utf8mb3_general_ci`; their observed
  `CHARACTER_MAXIMUM_LENGTH` / `CHARACTER_OCTET_LENGTH` pairs are `4` / `13`
  for `trx_state`, `42` / `126` for `trx_requested_lock_id`, `341` / `1024`
  for `trx_query`, `21` / `64` for `trx_operation_state`, `5` / `16` for
  `trx_isolation_level`, and `85` / `256` for
  `trx_last_foreign_key_error`.
- A default connection with no active write transaction returns zero rows.
- An active InnoDB write transaction in another connection produces a dynamic
  row with `TRX_STATE = 'RUNNING'`, lock and modified-row counters, the MySQL
  thread id, current SQL text, and other transaction status values.
- Successful reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports
  `-1`.

Representative metadata probe:

```sh
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names <<'SQL'
SELECT VERSION();
SHOW FULL TABLES FROM INFORMATION_SCHEMA
 WHERE Tables_in_INFORMATION_SCHEMA = 'INNODB_TRX';
SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,
       DATA_LENGTH,AUTO_INCREMENT
  FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_TRX';
SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,
       DATA_TYPE,CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,
       NUMERIC_PRECISION,NUMERIC_SCALE,DATETIME_PRECISION,
       CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES
  FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_TRX'
 ORDER BY ORDINAL_POSITION;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TRX;
SELECT @@warning_count, ROW_COUNT();
SQL
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_TRX` using the existing
  information-schema query subset;
- wildcard projection with the MySQL-observed 25-column order;
- case-insensitive information-schema table name lookup;
- table aliases, predicates, ordering, and `COUNT(*)` through the existing
  metadata query path;
- unqualified `INNODB_TRX` reads while `information_schema` is the selected
  schema;
- stable empty-row behavior, including while a MyLite transaction is open;
- system metadata through `INFORMATION_SCHEMA.TABLES`,
  `INFORMATION_SCHEMA.COLUMNS`, `SHOW TABLES`, `SHOW FULL TABLES`, and
  `SHOW TABLE STATUS` via the existing built-in table directory.

Out of scope:

- dynamic rows for active MyLite transactions;
- lock-wait state, lock identifiers, row-lock counters, transaction weight,
  CATS schedule weight, adaptive-hash status, or current transaction SQL text;
- cross-connection transaction visibility;
- Performance Schema `data_locks` and `data_lock_waits` integration;
- `PROCESS` privilege checks or account-specific visibility;
- physical InnoDB transaction structures, rollback segments, or lock manager
  state;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, identifiers, predicates, and ordering.
- Analyzer/runtime: recognizes `INNODB_TRX` as a supported information-schema
  system view and returns an empty row set.
- Catalog metadata: unchanged. No descriptor rows are introduced by this
  slice.
- Transaction runtime: unchanged. Existing transaction behavior remains
  independent of this metadata-only monitoring view.
- SQLite storage/VFS: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT * FROM INFORMATION_SCHEMA.INNODB_TRX;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TRX;
SELECT t.trx_id
  FROM INFORMATION_SCHEMA.INNODB_TRX AS t
 WHERE t.trx_state = 'RUNNING'
 ORDER BY t.trx_id;
SELECT COUNT(*) FROM INNODB_TRX;
```

## Runtime Semantics

`INNODB_TRX` is registered in the static information-schema table registry.
Row production is intentionally empty:

- system rows for `TABLES` and `COLUMNS` are generated from static
  descriptors;
- no rows are generated from MyLite transaction state because MyLite does not
  yet expose InnoDB transaction ids, lock wait ids, lock counters, transaction
  weights, or current-statement snapshots;
- the view remains empty while a MyLite transaction is open;
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

## Performance

The row set is static and empty. Metadata rows are generated from in-memory
descriptors and do not read or write MyLite catalog descriptors, transaction
state, physical row storage, SQLite tables, or system files.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- wildcard column labels and empty row set;
- row count and representative predicates over lower-case column names;
- case-insensitive table-name lookup;
- alias projection through the existing information-schema query path;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view row;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all 25 columns;
- `USE information_schema` unqualified-table reads;
- stable empty-row behavior while a MyLite transaction is active;
- MySQL 8.4.9 runtime observation of a dynamic active-transaction row, stored
  in the expectation script as an explicit out-of-scope behavior.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_trx_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_trx|transaction_lifecycle|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_trx_expectations.sh
git diff --check
cmake --workflow --preset check
```
