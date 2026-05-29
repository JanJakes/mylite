# Baseline INFORMATION_SCHEMA INNODB_CACHED_INDEXES

## Status

This phase adds `INFORMATION_SCHEMA.INNODB_CACHED_INDEXES` as a MySQL-shaped
synthetic information-schema system view. The view is queryable, exposes MySQL
8.4.9 table and column metadata, and returns no rows in MyLite because MyLite
does not have an InnoDB buffer pool or index-page cache to report.

The slice is metadata-only. It does not add buffer-pool accounting, cached page
tracking, physical InnoDB index identifiers, page eviction state, or
Performance Schema instrumentation.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing InnoDB index dictionary specification:
  `docs/specs/baseline-information-schema-innodb-indexes/specs.md`
- Existing empty InnoDB system-view specs and tests, especially
  `docs/specs/baseline-information-schema-innodb-temp-table-info/specs.md`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.INNODB_CACHED_INDEXES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-cached-indexes-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_CACHED_INDEXES` exists as an
  `information_schema` `SYSTEM VIEW`.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` reports three non-null columns in order:
  `SPACE_ID` as `int unsigned`, `INDEX_ID` as `bigint unsigned`, and
  `N_CACHED_PAGES` as `bigint unsigned`.
- The columns report empty-string defaults and SQL `NULL` for character,
  numeric precision, numeric scale, and datetime precision metadata.
- A default MySQL 8.4.9 runtime exposes dynamic rows for cached InnoDB index
  pages. The observed runtime returned 131 rows, including rows for system
  space id `4294967294`.
- Successful reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports
  `-1`.

Representative probe:

```sh
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names <<'SQL'
SELECT VERSION();
SHOW FULL TABLES FROM INFORMATION_SCHEMA
 WHERE Tables_in_INFORMATION_SCHEMA = 'INNODB_CACHED_INDEXES';
SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,
       DATA_LENGTH,AUTO_INCREMENT
  FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA='information_schema'
   AND TABLE_NAME='INNODB_CACHED_INDEXES';
SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,
       DATA_TYPE,CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,
       NUMERIC_PRECISION,NUMERIC_SCALE,DATETIME_PRECISION,
       CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES
  FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA='information_schema'
   AND TABLE_NAME='INNODB_CACHED_INDEXES'
 ORDER BY ORDINAL_POSITION;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CACHED_INDEXES;
SELECT SPACE_ID, INDEX_ID, N_CACHED_PAGES
  FROM INFORMATION_SCHEMA.INNODB_CACHED_INDEXES
 ORDER BY SPACE_ID, INDEX_ID LIMIT 8;
SELECT @@warning_count, ROW_COUNT();
SQL
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_CACHED_INDEXES` using the existing
  information-schema query subset;
- wildcard projection with the MySQL-observed three-column order;
- case-insensitive information-schema table name lookup;
- table aliases, predicates, ordering, and `COUNT(*)` through the existing
  metadata query path;
- unqualified `INNODB_CACHED_INDEXES` reads while `information_schema` is the
  selected schema;
- stable empty-row behavior, including after MyLite tables and indexes are
  created;
- system metadata through `INFORMATION_SCHEMA.TABLES`,
  `INFORMATION_SCHEMA.COLUMNS`, `SHOW TABLES`, `SHOW FULL TABLES`, and
  `SHOW TABLE STATUS` via the existing built-in table directory.

Out of scope:

- dynamic rows for cached MyLite index pages;
- InnoDB buffer-pool pages, page states, LRU position, eviction, or residency
  counts;
- exact MySQL `SPACE_ID` and `INDEX_ID` values for physical InnoDB indexes;
- mapping MyLite descriptor indexes to cached-page counts;
- `PROCESS` privilege checks or account-specific visibility;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, identifiers, predicates, and ordering.
- Analyzer/runtime: recognizes `INNODB_CACHED_INDEXES` as a supported
  information-schema system view and returns an empty row set.
- Catalog metadata: unchanged. Existing index descriptors are not used for
  cached-page rows in this slice.
- Storage/SQLite: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT * FROM INFORMATION_SCHEMA.INNODB_CACHED_INDEXES;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CACHED_INDEXES;
SELECT c.INDEX_ID
  FROM INFORMATION_SCHEMA.INNODB_CACHED_INDEXES AS c
 WHERE c.N_CACHED_PAGES > 0
 ORDER BY c.INDEX_ID;
SELECT COUNT(*) FROM INNODB_CACHED_INDEXES;
```

## Runtime Semantics

`INNODB_CACHED_INDEXES` is registered in the static information-schema table
registry. Row production is intentionally empty:

- system rows for `TABLES` and `COLUMNS` are generated from static
  descriptors;
- no rows are generated from MyLite table or index descriptors because MyLite
  does not expose an InnoDB buffer pool or cached index-page counters;
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
- row count and representative predicates over the numeric columns;
- case-insensitive table-name lookup;
- alias projection through the existing information-schema query path;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view row;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all three columns;
- `USE information_schema` unqualified-table reads;
- stable empty-row behavior after creating a MyLite table and secondary index;
- MySQL 8.4.9 runtime observation of nonempty dynamic cached-index rows, stored
  in the expectation script as explicit out-of-scope behavior.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_cached_indexes_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_cached_indexes|information_schema_innodb_indexes|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_cached_indexes_expectations.sh
git diff --check
cmake --workflow --preset check
```
