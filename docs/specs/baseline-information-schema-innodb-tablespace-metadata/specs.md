# Baseline INFORMATION_SCHEMA InnoDB Tablespace Metadata

## Status

This phase adds limited queryable synthetic
`INFORMATION_SCHEMA.INNODB_DATAFILES` and
`INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF` system views. The views expose
MySQL 8.4.9-shaped table and column metadata plus the default InnoDB
tablespace rows observed in a fresh MySQL 8.4.9 runtime.

The slice is metadata-only. It does not add real InnoDB tablespaces,
file-per-table storage, InnoDB data dictionary descriptors, temporary
tablespace tracking, or physical system-schema tables.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing information-schema and `TABLESPACES_EXTENSIONS` implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.INNODB_DATAFILES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-datafiles-table.html
- MySQL 8.4 Reference Manual,
  `INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-tablespaces-brief-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- Both tables exist as `information_schema` `SYSTEM VIEW` rows.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL` for both views.
- `INNODB_DATAFILES` has two columns: nullable `SPACE varbinary(256)` and
  non-null `PATH varchar(512)` with character set `utf8mb3` and collation
  `utf8mb3_bin`.
- `INNODB_TABLESPACES_BRIEF` has five columns: nullable
  `SPACE varbinary(256)`, non-null `NAME varchar(268)` with `utf8mb3_bin`,
  non-null `PATH varchar(512)` with `utf8mb3_bin`, nullable
  `FLAG varbinary(256)`, and non-null `SPACE_TYPE varchar(7)` with
  `utf8mb3_general_ci`.
- A fresh runtime returns four `INNODB_DATAFILES` rows:
  `0 / ibdata1`, `1 / ./sys/sys_config.ibd`,
  `4294967279 / ./undo_001`, and `4294967278 / ./undo_002`.
- A fresh runtime returns four `INNODB_TABLESPACES_BRIEF` rows:
  `innodb_system / ibdata1 / 18432 / System`,
  `innodb_undo_001 / ./undo_001 / 0 / Single`,
  `innodb_undo_002 / ./undo_002 / 0 / Single`, and
  `sys/sys_config / ./sys/sys_config.ibd / 16417 / Single`.
- Supported reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports `-1`
  after the `SELECT`.

MySQL adds rows for ordinary persistent InnoDB user tables. MyLite does not add
those rows in this slice because MyLite does not create physical InnoDB
tablespace files and must not imply a per-table `.ibd` storage model.

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_DATAFILES` and
  `INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- table aliases through the existing information-schema select path;
- table metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- the observed fresh-runtime default InnoDB datafile and brief tablespace rows;
- file-backed and in-memory handles with no storage mutation beyond opening
  the database.

Out of scope:

- descriptor-owned user table rows in these two InnoDB views;
- `INFORMATION_SCHEMA.FILES`, `INFORMATION_SCHEMA.INNODB_TABLESPACES`, or the
  broader InnoDB system-view family;
- real InnoDB space identifiers, file flags, file paths, or data dictionary
  state;
- temporary tablespace rows and session temporary tablespace tracking;
- tablespace DDL, general tablespaces, undo tablespace mutation, or encryption
  metadata;
- privilege filtering or account-specific visibility;
- physical MySQL data dictionary tables;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names and aliases.
- Analyzer/runtime: recognizes both InnoDB metadata views as supported
  synthetic information-schema system views and appends static rows.
- Catalog metadata: unchanged. No descriptor rows are introduced by this
  slice.
- SQLite storage/VFS: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_DATAFILES;
SELECT SPACE, PATH
  FROM INFORMATION_SCHEMA.INNODB_DATAFILES
 ORDER BY PATH;
SELECT NAME, PATH, SPACE_TYPE
  FROM INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF
 WHERE SPACE_TYPE = 'Single'
 ORDER BY NAME;
```

## Runtime Semantics

Both views are registered in the static information-schema table registry.
Row production emits static MySQL 8.4.9 observed default metadata:

- `INNODB_DATAFILES.SPACE`: displayed space id text;
- `INNODB_DATAFILES.PATH`: displayed datafile path text;
- `INNODB_TABLESPACES_BRIEF.SPACE`: displayed space id text;
- `INNODB_TABLESPACES_BRIEF.NAME`: displayed tablespace name;
- `INNODB_TABLESPACES_BRIEF.PATH`: displayed datafile path text;
- `INNODB_TABLESPACES_BRIEF.FLAG`: displayed flag text;
- `INNODB_TABLESPACES_BRIEF.SPACE_TYPE`: displayed type text.

The row set is independent of database contents and does not interact with
MyLite table descriptors, `TABLESPACES_EXTENSIONS` descriptor rows, or
physical storage.

## Diagnostics

The feature relies on existing information-schema diagnostics:

- unknown selected columns fail with the current unknown-column diagnostic;
- unsupported expressions, joins, grouping, predicates, ordering, and limits
  retain the current information-schema query subset behavior;
- allocation failures use existing MyLite runtime diagnostics.

Successful reads introduce no warnings.

## Performance

Both views emit four static rows. They do not read or write MyLite catalog
descriptors, physical row storage, SQLite tables, or system files.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- wildcard column labels for both views;
- exact ordered default row contents for both views;
- row counts and representative predicates;
- case-insensitive table-name lookup;
- alias projection for representative rows;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view rows;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all seven columns across both
  views;
- file-backed read behavior and unchanged MyLite file preamble.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_tablespace_metadata_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_tablespace_metadata|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_tablespace_metadata_expectations.sh
git diff --check
cmake --workflow --preset check
```
