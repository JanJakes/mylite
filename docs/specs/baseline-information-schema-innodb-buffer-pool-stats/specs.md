# Baseline INFORMATION_SCHEMA INNODB_BUFFER_POOL_STATS

## Status

This phase adds `INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS` as a
MySQL-shaped synthetic information-schema system view. The view is queryable,
exposes MySQL 8.4.9 table and column metadata, and returns one deterministic
`POOL_ID = 0` row with zero-valued buffer-pool counters.

The slice is a compatibility placeholder for applications that probe InnoDB
buffer-pool statistics. It does not add an InnoDB buffer pool, page residency
tracking, LRU state, physical I/O counters, hit-rate accounting, or Performance
Schema instrumentation.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing InnoDB compression and transaction system-view specs, especially
  `docs/specs/baseline-information-schema-innodb-cmpmem/specs.md` and
  `docs/specs/baseline-information-schema-innodb-trx/specs.md`
- MySQL 8.4 Reference Manual,
  `INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-buffer-pool-stats-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS` exists as an
  `information_schema` `SYSTEM VIEW`.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` reports 32 non-null columns in order.
- The counter columns use `bigint unsigned` metadata. The rate columns use
  `float(12,0)` metadata.
- All 32 columns report an empty-string default and SQL `NULL` for character,
  numeric precision, numeric scale, and datetime precision metadata.
- The observed default runtime exposed one row for `POOL_ID = 0`. The numeric
  values are dynamic and changed with server activity.
- Successful reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports
  `-1`.

Representative probe:

```sh
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names <<'SQL'
SELECT VERSION();
SHOW FULL TABLES FROM INFORMATION_SCHEMA
 WHERE Tables_in_INFORMATION_SCHEMA = 'INNODB_BUFFER_POOL_STATS';
SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,
       DATA_LENGTH,AUTO_INCREMENT
  FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA='information_schema'
   AND TABLE_NAME='INNODB_BUFFER_POOL_STATS';
SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,
       DATA_TYPE,CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,
       NUMERIC_PRECISION,NUMERIC_SCALE,DATETIME_PRECISION,
       CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES
  FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA='information_schema'
   AND TABLE_NAME='INNODB_BUFFER_POOL_STATS'
 ORDER BY ORDINAL_POSITION;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS;
SELECT * FROM INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS
 ORDER BY POOL_ID LIMIT 4;
SELECT @@warning_count, ROW_COUNT();
SQL
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS` using the
  existing information-schema query subset;
- wildcard projection with the MySQL-observed 32-column order;
- one deterministic row for buffer-pool instance `0`, with all size, counter,
  rate, and hit-rate fields set to `0`;
- case-insensitive information-schema table name lookup;
- table aliases, predicates, ordering, and `COUNT(*)` through the existing
  metadata query path;
- unqualified `INNODB_BUFFER_POOL_STATS` reads while `information_schema` is
  the selected schema;
- stable zero-row values after MyLite tables and indexes are created;
- system metadata through `INFORMATION_SCHEMA.TABLES`,
  `INFORMATION_SCHEMA.COLUMNS`, `SHOW TABLES`, `SHOW FULL TABLES`, and
  `SHOW TABLE STATUS` via the existing built-in table directory.

Out of scope:

- dynamic InnoDB buffer-pool sizing, page states, LRU position, eviction,
  dirty-page state, read-ahead state, decompression state, or I/O accounting;
- exact MySQL page counters, hit-rate values, and rate calculations;
- multiple buffer-pool instances;
- mapping MyLite descriptor tables or SQLite pages to InnoDB buffer-pool
  statistics;
- `PROCESS` privilege checks or account-specific visibility;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, identifiers, predicates, and ordering.
- Analyzer/runtime: recognizes `INNODB_BUFFER_POOL_STATS` as a supported
  information-schema system view and appends the synthetic row.
- Catalog metadata: unchanged. Existing table, index, and storage descriptors
  are not used to calculate buffer-pool statistics in this slice.
- Storage/SQLite: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT * FROM INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS;
SELECT s.POOL_ID, s.POOL_SIZE, s.HIT_RATE
  FROM INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS AS s
 WHERE s.POOL_ID = 0;
SELECT COUNT(*) FROM INNODB_BUFFER_POOL_STATS;
```

## Runtime Semantics

`INNODB_BUFFER_POOL_STATS` is registered in the static information-schema table
registry. Row production appends one synthetic row:

- `POOL_ID` is `0`;
- `POOL_SIZE`, free-page, data-page, pending, read/write, hit-rate, LRU, and
  decompression counters are `0`;
- rate fields are `0`, reported through the same text result path as the
  existing information-schema rows;
- the row is independent of MyLite base tables, secondary indexes, and stored
  data;
- successful reads introduce no warnings;
- `ROW_COUNT()` after a successful `SELECT` remains the existing query value
  `-1`.

The zero row is deliberately conservative. It preserves the one-instance shape
that applications commonly expect from MySQL while avoiding false claims about
InnoDB buffer-pool internals that MyLite does not implement.

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

- wildcard column labels and the deterministic `POOL_ID = 0` zero row;
- row count and representative predicates over size, counter, and rate columns;
- case-insensitive table-name lookup;
- alias projection through the existing information-schema query path;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view row;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all 32 columns;
- `USE information_schema` unqualified-table reads;
- stable zero values after creating a MyLite table, secondary index, and rows;
- MySQL 8.4.9 runtime observation of a dynamic `POOL_ID = 0` row, stored in
  the expectation script as explicit out-of-scope behavior.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_buffer_pool_stats_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_buffer_pool_stats|information_schema_innodb_cached_indexes|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_buffer_pool_stats_expectations.sh
git diff --check
cmake --workflow --preset check
```
