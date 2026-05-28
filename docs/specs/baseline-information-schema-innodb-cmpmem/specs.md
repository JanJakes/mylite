# Baseline INFORMATION_SCHEMA InnoDB CMPMEM Tables

## Status

This phase adds `INFORMATION_SCHEMA.INNODB_CMPMEM` and
`INFORMATION_SCHEMA.INNODB_CMPMEM_RESET` as MySQL-shaped synthetic
information-schema system views. Both tables are queryable, expose MySQL
8.4.9 table and column metadata, and return the five observed compressed-page
size rows with zero buffer-pool and relocation counters in MyLite.

The slice is metadata-only. It does not add InnoDB table compression,
compressed-page buffer-pool memory management, relocation accounting,
privilege filtering, or mutable reset counters.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing information-schema implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- MySQL 8.4 Reference Manual, `INNODB_CMPMEM` and
  `INNODB_CMPMEM_RESET`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-cmpmem-table.html
- MySQL 8.4 Reference Manual, InnoDB CMPMEM information-schema usage:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-information-schema-innodb_cmpmem.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_CMPMEM` and
  `INFORMATION_SCHEMA.INNODB_CMPMEM_RESET` exist as `information_schema`
  `SYSTEM VIEW` tables.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL` for both tables.
- `INFORMATION_SCHEMA.COLUMNS` reports six lower-case, non-null columns for
  each table in order: `page_size`, `buffer_pool_instance`, `pages_used`,
  `pages_free`, `relocation_ops`, and `relocation_time`.
- `page_size`, `buffer_pool_instance`, `pages_used`, `pages_free`, and
  `relocation_time` are `int` columns. `relocation_ops` is a `bigint`
  column. All six columns report empty string defaults, SQL `NULL`
  character, numeric, and datetime metadata, and `PRIVILEGES = 'select'`.
- A default fresh server returns five rows from both tables for page sizes
  `1024`, `2048`, `4096`, `8192`, and `16384`, with
  `BUFFER_POOL_INSTANCE = 0`. All observed page-use and relocation counters
  are zero on the fresh runtime.
- Repeated reads from `_RESET` returned the same five zero-counter rows in the
  baseline runtime. MySQL documents that reading `_RESET` resets relocation
  statistics; MyLite has no relocation counters to mutate in this slice.
- Successful reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports
  `-1` after the `SELECT`.
- Official MySQL documentation states that the tables report compressed pages
  in the InnoDB buffer pool, with rows corresponding to page sizes managed by
  InnoDB's compressed-page memory allocator.

Representative probes:

```sh
docker exec mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names -e \
  "SELECT VERSION(); \
   SHOW FULL TABLES FROM INFORMATION_SCHEMA \
    WHERE Tables_in_INFORMATION_SCHEMA IN ('INNODB_CMPMEM','INNODB_CMPMEM_RESET'); \
   SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT \
     FROM INFORMATION_SCHEMA.TABLES \
    WHERE TABLE_SCHEMA='information_schema' \
      AND TABLE_NAME IN ('INNODB_CMPMEM','INNODB_CMPMEM_RESET') \
    ORDER BY TABLE_NAME; \
   SELECT PAGE_SIZE,BUFFER_POOL_INSTANCE,PAGES_USED,PAGES_FREE,RELOCATION_OPS,RELOCATION_TIME \
     FROM INFORMATION_SCHEMA.INNODB_CMPMEM ORDER BY PAGE_SIZE,BUFFER_POOL_INSTANCE; \
   SELECT PAGE_SIZE,BUFFER_POOL_INSTANCE,PAGES_USED,PAGES_FREE,RELOCATION_OPS,RELOCATION_TIME \
     FROM INFORMATION_SCHEMA.INNODB_CMPMEM_RESET ORDER BY PAGE_SIZE,BUFFER_POOL_INSTANCE; \
   SELECT @@warning_count, ROW_COUNT();"

docker exec mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names -e \
  "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, \
          CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION, \
          NUMERIC_SCALE,DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME, \
          COLUMN_TYPE,PRIVILEGES \
     FROM INFORMATION_SCHEMA.COLUMNS \
    WHERE TABLE_SCHEMA='information_schema' \
      AND TABLE_NAME IN ('INNODB_CMPMEM','INNODB_CMPMEM_RESET') \
    ORDER BY TABLE_NAME, ORDINAL_POSITION;"
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_CMPMEM` and
  `INFORMATION_SCHEMA.INNODB_CMPMEM_RESET` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- wildcard reads with the six MySQL column labels;
- table aliases, predicates, and ordering through the existing
  information-schema select path;
- fixed page-size rows for `1024`, `2048`, `4096`, `8192`, and `16384` with
  buffer-pool instance `0` and zero page-use and relocation counters;
- table metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- reads from `_RESET` with no visible side effect because MyLite has no
  compressed-page memory counters;
- file-backed and in-memory handles with no storage mutation beyond opening
  the database.

Out of scope:

- InnoDB table compression or compressed-page storage;
- compressed-page buffer-pool allocation, fragmentation, or relocation state;
- dynamic rows or counter changes from user schemas, tables, indexes, buffer
  pool instances, or workloads;
- counter reset behavior beyond stable zero-counter rows;
- `PROCESS` privilege checks or account-specific visibility;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, identifiers, predicates, and ordering.
- Analyzer/runtime: recognizes both InnoDB CMPMEM views as supported
  information-schema system views and emits fixed static rows.
- Catalog metadata: unchanged. No descriptor rows are introduced by this
  slice.
- Storage/SQLite: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT * FROM INFORMATION_SCHEMA.INNODB_CMPMEM;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CMPMEM_RESET;
SELECT m.page_size, m.buffer_pool_instance, m.pages_used
  FROM INFORMATION_SCHEMA.INNODB_CMPMEM AS m
 WHERE m.page_size IN (1024, 16384)
 ORDER BY m.page_size;
```

## Runtime Semantics

`INNODB_CMPMEM` and `INNODB_CMPMEM_RESET` are registered in the static
information-schema table registry. Row production is fixed:

- system rows for `TABLES` and `COLUMNS` are generated from static
  descriptors;
- direct reads generate five rows in ascending page-size order;
- `BUFFER_POOL_INSTANCE` is always `0`, and page-use and relocation counters
  are always `0` until MyLite implements compressed-page buffer-pool
  instrumentation;
- rows are not generated from MyLite table, index, or storage descriptors
  because the MySQL tables reflect InnoDB buffer-pool compression
  instrumentation, not ordinary data dictionary metadata;
- reading `_RESET` has no stateful effect while counters are synthetic zeros;
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

Both row sets are static and tiny. Metadata and direct rows are generated from
in-memory descriptors and do not read or write MyLite catalog descriptors,
physical row storage, SQLite tables, or system files.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- wildcard column labels and fixed five-row zero-counter row sets for both
  views;
- row counts and representative predicates;
- case-insensitive table-name lookup;
- alias projection through the existing information-schema query path;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view rows for both tables;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all columns in both tables;
- `USE information_schema` unqualified-table reads;
- repeated `_RESET` reads remain stable and warning-free;
- file-backed read behavior and unchanged MyLite file preamble.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_cmpmem_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_cmpmem|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_cmpmem_expectations.sh
git diff --check
cmake --workflow --preset check
```
