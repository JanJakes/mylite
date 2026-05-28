# Baseline INFORMATION_SCHEMA InnoDB CMP Per Index Tables

## Status

This phase adds `INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX` and
`INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX_RESET` as MySQL-shaped synthetic
information-schema system views. Both tables are queryable, expose MySQL
8.4.9 table and column metadata, and return empty row sets in MyLite because
MyLite does not model InnoDB compressed-page statistics or the
`innodb_cmp_per_index_enabled` collection path.

The slice is metadata-only. It does not add InnoDB table compression,
per-index compression counters, compressed page storage, buffer-pool
instrumentation, privilege filtering, or mutable reset counters.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing information-schema implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- MySQL 8.4 Reference Manual, `INNODB_CMP_PER_INDEX` and
  `INNODB_CMP_PER_INDEX_RESET`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-cmp-per-index-table.html
- MySQL 8.4 Reference Manual, monitoring InnoDB table compression at
  runtime:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-compression-tuning-monitoring.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `@@innodb_cmp_per_index_enabled` is `0` on the fresh runtime.
- `INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX` and
  `INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX_RESET` exist as
  `information_schema` `SYSTEM VIEW` tables.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL` for both tables.
- `INFORMATION_SCHEMA.COLUMNS` reports eight lower-case columns for each
  table in order: `database_name`, `table_name`, `index_name`,
  `compress_ops`, `compress_ops_ok`, `compress_time`, `uncompress_ops`, and
  `uncompress_time`.
- The first three columns are non-null `varchar(192)`, report character
  maximum length `64`, octet length `192`, character set `utf8mb3`,
  collation `utf8mb3_general_ci`, empty string defaults, and
  `PRIVILEGES = 'select'`.
- The five counter columns are non-null `int` columns, report empty string
  defaults, SQL `NULL` character, numeric, and datetime metadata, and
  `PRIVILEGES = 'select'`.
- A default fresh server returns zero rows from both tables. Supported reads
  leave `@@warning_count = 0`, and `ROW_COUNT()` reports `-1` after the
  `SELECT`.
- Official MySQL documentation states that per-index compression statistics
  are not gathered by default because of overhead and require
  `innodb_cmp_per_index_enabled` before relevant compressed-table operations.
  MyLite does not implement that collection mechanism in this slice.

Representative probes:

```sh
docker exec mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names -e \
  "SELECT VERSION(); SELECT @@innodb_cmp_per_index_enabled; \
   SHOW FULL TABLES FROM INFORMATION_SCHEMA \
    WHERE Tables_in_INFORMATION_SCHEMA IN ('INNODB_CMP_PER_INDEX','INNODB_CMP_PER_INDEX_RESET'); \
   SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT \
     FROM INFORMATION_SCHEMA.TABLES \
    WHERE TABLE_SCHEMA='information_schema' \
      AND TABLE_NAME IN ('INNODB_CMP_PER_INDEX','INNODB_CMP_PER_INDEX_RESET') \
    ORDER BY TABLE_NAME; \
   SELECT 'CMP', COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX; \
   SELECT 'RESET', COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX_RESET; \
   SELECT @@warning_count, ROW_COUNT();"

docker exec mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names -e \
  "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, \
          CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION, \
          NUMERIC_SCALE,DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME, \
          COLUMN_TYPE,PRIVILEGES \
     FROM INFORMATION_SCHEMA.COLUMNS \
    WHERE TABLE_SCHEMA='information_schema' \
      AND TABLE_NAME IN ('INNODB_CMP_PER_INDEX','INNODB_CMP_PER_INDEX_RESET') \
    ORDER BY TABLE_NAME, ORDINAL_POSITION;"
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX` and
  `INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX_RESET` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- wildcard reads with the eight MySQL column labels;
- table aliases through the existing information-schema select path;
- table metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- stable empty-row behavior in all MyLite sessions;
- reads from `_RESET` with no visible side effect because no counters exist;
- file-backed and in-memory handles with no storage mutation beyond opening
  the database.

Out of scope:

- InnoDB table compression or compressed-page storage;
- `innodb_cmp_per_index_enabled` as a mutable statistics collection switch;
- dynamic rows for MyLite schemas, tables, or indexes;
- counter reset behavior beyond stable empty rows;
- `PROCESS` privilege checks or account-specific visibility;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, and identifiers.
- Analyzer/runtime: recognizes both InnoDB compression per-index views as
  supported information-schema system views and returns empty row sets.
- Catalog metadata: unchanged. No descriptor rows are introduced by this
  slice.
- Storage/SQLite: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT * FROM INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX_RESET;
SELECT c.database_name, c.table_name, c.index_name
  FROM INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX AS c
 WHERE c.compress_ops = 1;
```

## Runtime Semantics

`INNODB_CMP_PER_INDEX` and `INNODB_CMP_PER_INDEX_RESET` are registered in the
static information-schema table registry. Row production is intentionally
empty:

- system rows for `TABLES` and `COLUMNS` are generated from static
  descriptors;
- no rows are generated from MyLite table or index descriptors because the
  MySQL tables reflect InnoDB compressed-page instrumentation, not ordinary
  data-dictionary metadata;
- reading `_RESET` has no stateful effect while the row set is empty;
- successful reads introduce no warnings;
- `ROW_COUNT()` after a successful `SELECT` remains the existing query value
  `-1`.

## Diagnostics

The feature relies on existing information-schema diagnostics:

- unknown selected columns fail with the current unknown-column diagnostic;
- unsupported expressions, joins, grouping, predicates, ordering, and limits
  retain the current information-schema query subset behavior;
- allocation failures use existing MyLite runtime diagnostics.

Successful reads introduce no warnings.

## Performance

Both row sets are static and empty. Metadata rows are generated from in-memory
descriptors and do not read or write MyLite catalog descriptors, physical row
storage, SQLite tables, or system files.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- wildcard column labels and empty row sets for both views;
- row counts and representative empty predicates;
- case-insensitive table-name lookup;
- alias projection through the existing information-schema query path;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view rows for both tables;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all columns in both tables;
- `USE information_schema` unqualified-table reads;
- repeated `_RESET` reads remain empty and warning-free;
- file-backed read behavior and unchanged MyLite file preamble.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_cmp_per_index_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_cmp_per_index|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_cmp_per_index_expectations.sh
git diff --check
cmake --workflow --preset check
```
