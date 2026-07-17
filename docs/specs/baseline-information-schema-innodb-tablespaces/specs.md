# Baseline INFORMATION_SCHEMA InnoDB Tablespaces

## Status

This phase adds a limited queryable synthetic
`INFORMATION_SCHEMA.INNODB_TABLESPACES` system view. The view exposes
MySQL 8.4.9-shaped table and column metadata and returns no rows because MyLite
does not own InnoDB tablespaces.

The slice is metadata-only. It does not add physical InnoDB tablespaces,
file-per-table `.ibd` files for MyLite tables, general tablespace DDL,
temporary tablespace descriptors, encryption metadata, or physical
system-schema tables.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing `INFORMATION_SCHEMA.INNODB_DATAFILES` and
  `INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF` implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.INNODB_TABLESPACES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-tablespaces-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_TABLESPACES` exists as a `SYSTEM VIEW`.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL` for the system view row.
- The view has 15 columns: `SPACE`, `NAME`, `FLAG`, `ROW_FORMAT`,
  `PAGE_SIZE`, `ZIP_PAGE_SIZE`, `SPACE_TYPE`, `FS_BLOCK_SIZE`, `FILE_SIZE`,
  `ALLOCATED_SIZE`, `AUTOEXTEND_SIZE`, `SERVER_VERSION`, `SPACE_VERSION`,
  `ENCRYPTION`, and `STATE`.
- `SPACE`, `FLAG`, `PAGE_SIZE`, `ZIP_PAGE_SIZE`, `FS_BLOCK_SIZE`, and
  `SPACE_VERSION` are non-null `int unsigned`; `FILE_SIZE`,
  `ALLOCATED_SIZE`, and `AUTOEXTEND_SIZE` are non-null `bigint unsigned`.
- `NAME`, `ROW_FORMAT`, `SPACE_TYPE`, `SERVER_VERSION`, `ENCRYPTION`, and
  `STATE` are `varchar` columns using `utf8mb3_general_ci`. MySQL reports
  `NAME varchar(655)` with `CHARACTER_MAXIMUM_LENGTH = 218`; `ROW_FORMAT`
  `varchar(22)` with max length `7`; `SPACE_TYPE varchar(10)` with max length
  `3`; `SERVER_VERSION varchar(10)` with max length `3`; `ENCRYPTION
  varchar(1)` with max length `0`; and `STATE varchar(10)` with max length
  `3`.
- The system-view column metadata reports empty-string `COLUMN_DEFAULT` values
  for all 15 columns.
- A runtime with no user tablespace rows currently returns five built-in rows:
  `sys/sys_config`, `innodb_undo_002`, `innodb_undo_001`,
  `innodb_temporary`, and `mysql`.
- Creating a persistent InnoDB table in MySQL adds a file-per-table row. MyLite
  does not add user-table rows in this slice because it does not create
  physical InnoDB `.ibd` tablespaces.
- Successful reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports
  `-1` after the `SELECT`.

Representative probe:

```sh
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names <<'SQL'
SHOW FULL TABLES FROM INFORMATION_SCHEMA
 WHERE Tables_in_INFORMATION_SCHEMA = 'INNODB_TABLESPACES';
SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
       CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
       NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
       COLUMN_TYPE, PRIVILEGES
  FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA = 'information_schema'
   AND TABLE_NAME = 'INNODB_TABLESPACES'
 ORDER BY ORDINAL_POSITION;
SELECT SPACE, NAME, FLAG, ROW_FORMAT, PAGE_SIZE, ZIP_PAGE_SIZE, SPACE_TYPE,
       FS_BLOCK_SIZE, FILE_SIZE, ALLOCATED_SIZE, AUTOEXTEND_SIZE,
       SERVER_VERSION, SPACE_VERSION, ENCRYPTION, STATE
  FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
 ORDER BY SPACE;
SQL
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_TABLESPACES` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- table aliases, predicates, ordering, and `COUNT(*)` through the existing
  metadata query path;
- unqualified `INNODB_TABLESPACES` reads while `information_schema` is the
  selected schema;
- table metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- a stable empty row set that does not claim built-in or user tablespaces.

Out of scope:

- descriptor-owned user table rows;
- `INFORMATION_SCHEMA.FILES`, `INFORMATION_SCHEMA.INNODB_TABLESTATS`,
  `INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES`, or physical
  tablespace instrumentation;
- mutable general tablespaces, undo tablespace mutation, encryption state, or
  `CREATE TABLESPACE` / `ALTER TABLESPACE` / `DROP TABLESPACE`;
- physical file sizes derived from MyLite storage, `.ibd` paths, or data
  dictionary state;
- privilege filtering or account-specific visibility;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, identifiers, predicates, and ordering.
- Analyzer/runtime: recognizes `INNODB_TABLESPACES` as a supported
  information-schema system view and returns no storage rows.
- Catalog metadata: unchanged. MyLite table descriptors are intentionally not
  projected into this physical InnoDB tablespace view.
- Storage/SQLite: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TABLESPACES;
SELECT NAME, SPACE_TYPE, STATE
  FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
 WHERE SPACE_TYPE IN ('Undo', 'Single')
 ORDER BY SPACE;
SELECT t.NAME, t.FILE_SIZE
  FROM INFORMATION_SCHEMA.INNODB_TABLESPACES AS t
 WHERE t.NAME = 'sys/sys_config';
```

## Runtime Semantics

`INNODB_TABLESPACES` is registered in the static information-schema table
registry. Direct reads return no rows. The view does not synthesize built-in
MySQL tablespaces, derive rows from MyLite table descriptors, or inspect
physical storage.

## Diagnostics

The feature relies on existing information-schema diagnostics:

- unknown selected columns fail with the current unknown-column diagnostic;
- unsupported expressions, joins, grouping, predicates, and limits retain the
  current information-schema query subset behavior;
- allocation failures use existing MyLite runtime diagnostics.

Successful reads introduce no warnings.

## Tests

Extend the existing focused InnoDB tablespace metadata C test and MySQL
expectation script. Coverage must include:

- `SHOW FULL TABLES` and `INFORMATION_SCHEMA.TABLES` metadata for the system
  view;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all 15 columns;
- wildcard column labels with an empty result;
- zero row count, case-insensitive table lookup, aliases, predicates, and
  unqualified reads after `USE information_schema`;
- `@@warning_count` and `ROW_COUNT()` status after a successful read;
- file-backed read behavior through the existing tablespace metadata safety
  test;
- absence of false file sizes, allocation state, versions, and status rows.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_tablespace_metadata_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_tablespace_metadata|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_tablespace_metadata_expectations.sh
git diff --check
cmake --workflow --preset check
```
