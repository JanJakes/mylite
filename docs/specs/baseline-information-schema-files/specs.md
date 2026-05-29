# Baseline INFORMATION_SCHEMA FILES

## Status

This phase adds `INFORMATION_SCHEMA.FILES` as a MySQL-shaped synthetic
information-schema system view. The view is queryable, exposes MySQL 8.4.9
table and column metadata, and returns the six default InnoDB file rows
observed in a fresh MySQL 8.4.9 runtime.

The slice is metadata-only. It does not add real InnoDB tablespace files,
NDB Disk Data files, file-per-table storage, temporary tablespace accounting,
or physical MySQL data-dictionary state.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing InnoDB tablespace metadata specification:
  `docs/specs/baseline-information-schema-innodb-tablespace-metadata/specs.md`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.FILES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-files-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.FILES` exists as an `information_schema` `SYSTEM VIEW`.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL` for the view.
- `FILES` has 38 columns in order:
  `FILE_ID`, `FILE_NAME`, `FILE_TYPE`, `TABLESPACE_NAME`, `TABLE_CATALOG`,
  `TABLE_SCHEMA`, `TABLE_NAME`, `LOGFILE_GROUP_NAME`,
  `LOGFILE_GROUP_NUMBER`, `ENGINE`, `FULLTEXT_KEYS`, `DELETED_ROWS`,
  `UPDATE_COUNT`, `FREE_EXTENTS`, `TOTAL_EXTENTS`, `EXTENT_SIZE`,
  `INITIAL_SIZE`, `MAXIMUM_SIZE`, `AUTOEXTEND_SIZE`, `CREATION_TIME`,
  `LAST_UPDATE_TIME`, `LAST_ACCESS_TIME`, `RECOVER_TIME`,
  `TRANSACTION_COUNTER`, `VERSION`, `ROW_FORMAT`, `TABLE_ROWS`,
  `AVG_ROW_LENGTH`, `DATA_LENGTH`, `MAX_DATA_LENGTH`, `INDEX_LENGTH`,
  `DATA_FREE`, `CREATE_TIME`, `UPDATE_TIME`, `CHECK_TIME`, `CHECKSUM`,
  `STATUS`, and `EXTRA`.
- A fresh runtime returns six InnoDB rows ordered by `FILE_NAME`:
  `./ibdata1`, `./ibtmp1`, `./mysql.ibd`, `./sys/sys_config.ibd`,
  `./undo_001`, and `./undo_002`.
- Fresh-runtime row values include InnoDB file ids, file types, tablespace
  names, `ENGINE = 'InnoDB'`, free and total extent counts, `EXTENT_SIZE =
  1048576`, initial sizes, autoextend sizes, data-free values, and
  `STATUS = 'NORMAL'`.
- Successful reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports
  `-1`.

Representative probe:

```sh
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names <<'SQL'
SELECT VERSION();
SHOW FULL TABLES FROM INFORMATION_SCHEMA
 WHERE Tables_in_INFORMATION_SCHEMA = 'FILES';
SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,
       DATA_LENGTH,AUTO_INCREMENT
  FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA='information_schema' AND TABLE_NAME='FILES';
SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,
       DATA_TYPE,CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,
       NUMERIC_PRECISION,NUMERIC_SCALE,DATETIME_PRECISION,
       CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES
  FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA='information_schema' AND TABLE_NAME='FILES'
 ORDER BY ORDINAL_POSITION;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.FILES;
SELECT FILE_ID,FILE_NAME,FILE_TYPE,TABLESPACE_NAME,ENGINE,FREE_EXTENTS,
       TOTAL_EXTENTS,EXTENT_SIZE,INITIAL_SIZE,AUTOEXTEND_SIZE,DATA_FREE,STATUS
  FROM INFORMATION_SCHEMA.FILES
 ORDER BY FILE_NAME;
SELECT @@warning_count, ROW_COUNT();
SQL
```

MySQL adds rows for ordinary persistent InnoDB user tables and for other
tablespace/file features. MyLite does not add those rows in this slice because
MyLite does not create physical InnoDB `.ibd`, `ibdata`, `ibtmp`, undo, or NDB
Disk Data files for user objects.

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.FILES` using the existing
  information-schema query subset;
- wildcard projection with the MySQL-observed 38-column order;
- case-insensitive information-schema table name lookup;
- table aliases, predicates, ordering, and `COUNT(*)` through the existing
  metadata query path;
- unqualified reads while `information_schema` is the selected schema;
- stable six-row default metadata, including after MyLite user tables and
  indexes are created;
- system metadata through `INFORMATION_SCHEMA.TABLES`,
  `INFORMATION_SCHEMA.COLUMNS`, `SHOW TABLES`, `SHOW FULL TABLES`, and
  `SHOW TABLE STATUS` via the existing built-in table directory.

Out of scope:

- descriptor-owned user table file rows;
- physical InnoDB tablespace ids, file sizes, free-space accounting, extent
  tracking, import status, or file state mutation;
- global temporary tablespace mutation or session temporary table file rows;
- general tablespace, undo tablespace, encryption, or NDB Disk Data DDL;
- `PROCESS` privilege checks or account-specific visibility;
- physical MySQL data dictionary tables;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, identifiers, predicates, and ordering.
- Analyzer/runtime: recognizes `FILES` as a supported information-schema
  system view and appends static rows.
- Catalog metadata: unchanged. Existing table, index, and storage descriptors
  are not used to calculate file rows in this slice.
- Storage/SQLite: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT * FROM INFORMATION_SCHEMA.FILES;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.FILES WHERE ENGINE = 'InnoDB';
SELECT f.FILE_NAME, f.STATUS
  FROM INFORMATION_SCHEMA.FILES AS f
 WHERE f.FILE_TYPE = 'UNDO LOG'
 ORDER BY f.FILE_NAME;
SELECT COUNT(*) FROM FILES;
```

## Runtime Semantics

`FILES` is registered in the static information-schema table registry. Row
production emits static MySQL 8.4.9 observed default metadata for:

- the InnoDB system tablespace file;
- the global temporary tablespace file;
- the `mysql` general tablespace file;
- the `sys/sys_config` file-per-table tablespace file;
- the two default undo tablespace files.

The row set is independent of database contents and does not interact with
MyLite table descriptors, `TABLESPACES_EXTENSIONS` descriptor rows,
`INNODB_DATAFILES`, or physical storage.

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

- wildcard column labels and the representative `./ibdata1` full row;
- exact ordered default row contents for core file columns;
- row counts and representative predicates;
- case-insensitive table-name lookup;
- alias projection through the existing information-schema query path;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view row;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all 38 columns in the MySQL
  expectation script and representative C coverage;
- `USE information_schema` unqualified-table reads;
- stable six-row behavior after creating a MyLite table and secondary index.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_files_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_files|information_schema_innodb_tablespace_metadata|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_files_expectations.sh
git diff --check
cmake --workflow --preset check
```
