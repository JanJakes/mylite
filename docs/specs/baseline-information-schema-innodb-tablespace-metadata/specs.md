# Baseline INFORMATION_SCHEMA InnoDB Tablespace Metadata

## Status

This phase adds limited queryable synthetic
`INFORMATION_SCHEMA.INNODB_DATAFILES` and
`INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF` system views. The views expose
MySQL 8.4.9-shaped table and column metadata and return no rows because MyLite
does not own InnoDB datafiles or tablespaces.

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
- Reused MySQL comparison runtimes can also contain user-table datafile and
  tablespace rows from other probes. The MySQL expectation artifact filters by
  the default names above so it verifies the target default metadata without
  requiring a fresh server instance.
- Supported reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports `-1`
  after the `SELECT`.

MySQL adds the observed default rows and rows for ordinary persistent InnoDB
user tables. MyLite reproduces neither category because it does not create
physical InnoDB tablespace files and must not imply an InnoDB storage model.

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_DATAFILES` and
  `INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- table aliases through the existing information-schema select path;
- table metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- a stable empty row set for both views;
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
  synthetic information-schema system views and returns no storage rows.
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
Direct reads return no rows. The views do not synthesize MySQL default
tablespaces, derive rows from MyLite table descriptors, or inspect physical
storage.

## Diagnostics

The feature relies on existing information-schema diagnostics:

- unknown selected columns fail with the current unknown-column diagnostic;
- unsupported expressions, joins, grouping, predicates, ordering, and limits
  retain the current information-schema query subset behavior;
- allocation failures use existing MyLite runtime diagnostics.

Successful reads introduce no warnings.

## Performance

Both views emit no rows and do not read MyLite catalog descriptors, physical
row storage, SQLite tables, or system files.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- wildcard column labels for both views;
- empty row sets and zero counts for representative predicates;
- case-insensitive table-name lookup;
- alias projection for representative rows;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view rows;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all seven columns across both
  views;
- file-backed read behavior and unchanged MyLite file preamble.
- absence of false paths, identifiers, flags, and tablespace types.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_tablespace_metadata_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_tablespace_metadata|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_tablespace_metadata_expectations.sh
git diff --check
cmake --workflow --preset check
```
