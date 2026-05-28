# Baseline INFORMATION_SCHEMA InnoDB CMP Tables

## Status

This phase adds `INFORMATION_SCHEMA.INNODB_CMP` and
`INFORMATION_SCHEMA.INNODB_CMP_RESET` as MySQL-shaped synthetic
information-schema system views. Both tables are queryable, expose MySQL
8.4.9 table and column metadata, and return the five observed compressed-page
size rows with zero counters in MyLite.

The slice is metadata-only. It does not add InnoDB table compression,
compressed-page storage, buffer-pool instrumentation, privilege filtering, or
mutable reset counters.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing information-schema implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- MySQL 8.4 Reference Manual, `INNODB_CMP` and `INNODB_CMP_RESET`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-cmp-table.html
- MySQL 8.4 Reference Manual, InnoDB compression information-schema usage:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-information-schema-innodb_cmp.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_CMP` and
  `INFORMATION_SCHEMA.INNODB_CMP_RESET` exist as `information_schema`
  `SYSTEM VIEW` tables.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL` for both tables.
- `INFORMATION_SCHEMA.COLUMNS` reports six lower-case, non-null `int`
  columns for each table in order: `page_size`, `compress_ops`,
  `compress_ops_ok`, `compress_time`, `uncompress_ops`, and
  `uncompress_time`.
- Each column reports an empty string default, SQL `NULL` character,
  numeric, and datetime metadata, `COLUMN_TYPE = 'int'`, and
  `PRIVILEGES = 'select'`.
- A default fresh server returns five rows from both tables for page sizes
  `1024`, `2048`, `4096`, `8192`, and `16384`. All observed counters are
  zero on the fresh runtime.
- Repeated reads from `_RESET` returned the same five zero-counter rows in the
  baseline runtime. MySQL documents that reading `_RESET` resets compression
  and uncompression counters; MyLite has no counters to mutate in this slice.
- Successful reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports
  `-1` after the `SELECT`.
- Official MySQL documentation states that the tables report operations
  related to compressed InnoDB tables, and that `_RESET` has the same visible
  contents while resetting the accumulated compression and uncompression
  statistics.

Representative probes:

```sh
docker exec mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names -e \
  "SELECT VERSION(); \
   SHOW FULL TABLES FROM INFORMATION_SCHEMA \
    WHERE Tables_in_INFORMATION_SCHEMA IN ('INNODB_CMP','INNODB_CMP_RESET'); \
   SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT \
     FROM INFORMATION_SCHEMA.TABLES \
    WHERE TABLE_SCHEMA='information_schema' \
      AND TABLE_NAME IN ('INNODB_CMP','INNODB_CMP_RESET') \
    ORDER BY TABLE_NAME; \
   SELECT PAGE_SIZE,COMPRESS_OPS,COMPRESS_OPS_OK,COMPRESS_TIME,UNCOMPRESS_OPS,UNCOMPRESS_TIME \
     FROM INFORMATION_SCHEMA.INNODB_CMP ORDER BY PAGE_SIZE; \
   SELECT PAGE_SIZE,COMPRESS_OPS,COMPRESS_OPS_OK,COMPRESS_TIME,UNCOMPRESS_OPS,UNCOMPRESS_TIME \
     FROM INFORMATION_SCHEMA.INNODB_CMP_RESET ORDER BY PAGE_SIZE; \
   SELECT @@warning_count, ROW_COUNT();"

docker exec mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names -e \
  "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, \
          CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION, \
          NUMERIC_SCALE,DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME, \
          COLUMN_TYPE,PRIVILEGES \
     FROM INFORMATION_SCHEMA.COLUMNS \
    WHERE TABLE_SCHEMA='information_schema' \
      AND TABLE_NAME IN ('INNODB_CMP','INNODB_CMP_RESET') \
    ORDER BY TABLE_NAME, ORDINAL_POSITION;"
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_CMP` and
  `INFORMATION_SCHEMA.INNODB_CMP_RESET` using the existing information-schema
  query subset;
- case-insensitive information-schema table name lookup;
- wildcard reads with the six MySQL column labels;
- table aliases, predicates, and ordering through the existing
  information-schema select path;
- fixed page-size rows for `1024`, `2048`, `4096`, `8192`, and `16384` with
  zero compression and uncompression counters;
- table metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- reads from `_RESET` with no visible side effect because MyLite has no
  compression counters;
- file-backed and in-memory handles with no storage mutation beyond opening
  the database.

Out of scope:

- InnoDB table compression or compressed-page storage;
- dynamic rows or counter changes from user schemas, tables, indexes, or
  workloads;
- counter reset behavior beyond stable zero-counter rows;
- `PROCESS` privilege checks or account-specific visibility;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, identifiers, predicates, and ordering.
- Analyzer/runtime: recognizes both InnoDB compression views as supported
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
SELECT * FROM INFORMATION_SCHEMA.INNODB_CMP;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CMP_RESET;
SELECT c.page_size, c.compress_ops
  FROM INFORMATION_SCHEMA.INNODB_CMP AS c
 WHERE c.page_size IN (1024, 16384)
 ORDER BY c.page_size;
```

## Runtime Semantics

`INNODB_CMP` and `INNODB_CMP_RESET` are registered in the static
information-schema table registry. Row production is fixed:

- system rows for `TABLES` and `COLUMNS` are generated from static
  descriptors;
- direct reads generate five rows in ascending page-size order;
- counter columns are always `0` until MyLite implements compressed-page
  statistics;
- rows are not generated from MyLite table or index descriptors because the
  MySQL tables reflect InnoDB compression instrumentation, not ordinary data
  dictionary metadata;
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
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_cmp_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_cmp|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_cmp_expectations.sh
git diff --check
cmake --workflow --preset check
```
