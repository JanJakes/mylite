# Baseline INFORMATION_SCHEMA INNODB_BUFFER_PAGE Tables

## Status

This phase adds `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE` and
`INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU` as MySQL-shaped synthetic
information-schema system views. The views are queryable, expose MySQL 8.4.9
table and column metadata, and return no rows in MyLite because MyLite does not
have an InnoDB buffer pool or LRU page list to report.

The slice is metadata-only. It does not add page residency tracking, buffer
pool blocks, LRU positions, InnoDB tablespace page ids, flush state, I/O
state, stale page tracking, or Performance Schema instrumentation.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing buffer-pool statistics specification:
  `docs/specs/baseline-information-schema-innodb-buffer-pool-stats/specs.md`
- Existing empty InnoDB system-view specs and tests, especially
  `docs/specs/baseline-information-schema-innodb-cached-indexes/specs.md`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-buffer-page-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-buffer-page-lru-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE` and
  `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU` exist as `information_schema`
  `SYSTEM VIEW` objects.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL` for both views.
- `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE` reports 21 columns in order.
- `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU` reports 20 columns in order.
- Numeric columns use `bigint unsigned` metadata. Page-state columns use
  `varchar(64)` metadata. Boolean-like columns use `varchar(3)` metadata.
  Table and index name columns use `varchar(1024)` metadata.
- All columns report an empty-string default. Character columns use
  `utf8mb3` / `utf8mb3_general_ci`; numeric columns report SQL `NULL` for
  character, numeric precision, numeric scale, and datetime precision metadata.
- The observed runtime returned 8192 `INNODB_BUFFER_PAGE` rows and 2309
  `INNODB_BUFFER_PAGE_LRU` rows. Those values are dynamic and depend on InnoDB
  buffer-pool state.
- Successful reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports
  `-1`.

Representative probe:

```sh
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names <<'SQL'
SELECT VERSION();
SHOW FULL TABLES FROM INFORMATION_SCHEMA
 WHERE Tables_in_INFORMATION_SCHEMA IN
       ('INNODB_BUFFER_PAGE','INNODB_BUFFER_PAGE_LRU');
SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,
       DATA_LENGTH,AUTO_INCREMENT
  FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA='information_schema'
   AND TABLE_NAME IN ('INNODB_BUFFER_PAGE','INNODB_BUFFER_PAGE_LRU')
 ORDER BY TABLE_NAME;
SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,
       DATA_TYPE,CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,
       NUMERIC_PRECISION,NUMERIC_SCALE,DATETIME_PRECISION,
       CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES
  FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA='information_schema'
   AND TABLE_NAME IN ('INNODB_BUFFER_PAGE','INNODB_BUFFER_PAGE_LRU')
 ORDER BY TABLE_NAME,ORDINAL_POSITION;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_BUFFER_PAGE;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU;
SELECT @@warning_count, ROW_COUNT();
SQL
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE` and
  `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU` using the existing
  information-schema query subset;
- wildcard projection with the MySQL-observed column order for each view;
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

- dynamic buffer-pool page rows;
- buffer-pool block ids, LRU positions, page states, stale page state, flush
  state, I/O fix state, and old/young page state;
- exact MySQL tablespace ids, page numbers, table/index page ownership, record
  counts, data sizes, or compressed page sizes;
- mapping SQLite pages or MyLite descriptor rows to InnoDB buffer-pool pages;
- `PROCESS` privilege checks or account-specific visibility;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, identifiers, predicates, and ordering.
- Analyzer/runtime: recognizes both views as supported information-schema
  system views and returns empty row sets.
- Catalog metadata: unchanged. Existing table, index, and storage descriptors
  are not used to calculate buffer-pool pages in this slice.
- Storage/SQLite: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT * FROM INFORMATION_SCHEMA.INNODB_BUFFER_PAGE;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU;
SELECT b.POOL_ID, b.BLOCK_ID
  FROM INFORMATION_SCHEMA.INNODB_BUFFER_PAGE AS b
 WHERE b.PAGE_TYPE = 'INDEX';
SELECT COUNT(*) FROM INNODB_BUFFER_PAGE_LRU;
```

## Runtime Semantics

Both views are registered in the static information-schema table registry. Row
production is intentionally empty:

- system rows for `TABLES` and `COLUMNS` are generated from static
  descriptors;
- no rows are generated from MyLite table, index, or SQLite page state because
  MyLite does not expose an InnoDB buffer pool, buffer blocks, or LRU page
  list;
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

- wildcard column labels and empty row sets for both views;
- row counts and representative predicates over numeric, state, and name
  columns;
- case-insensitive table-name lookup;
- alias projection through the existing information-schema query path;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view rows for both views;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all 41 total columns;
- `USE information_schema` unqualified-table reads;
- stable empty-row behavior after creating a MyLite table and secondary index;
- MySQL 8.4.9 runtime observation of dynamic page rows, stored in the
  expectation script as explicit out-of-scope behavior.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_buffer_page_tables_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_buffer_page_tables|information_schema_innodb_buffer_pool_stats|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_buffer_page_tables_expectations.sh
git diff --check
cmake --workflow --preset check
```
