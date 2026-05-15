# Baseline INFORMATION_SCHEMA VIEW_TABLE_USAGE

## Status

This phase adds `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` as a MySQL-shaped
synthetic information-schema system view. MyLite does not yet implement
`CREATE VIEW` or durable view descriptors, so the table has exact metadata and
returns zero user rows for now.

The slice is intentionally metadata-only. It does not add view DDL, stored view
definitions, view dependency extraction, privilege filtering, or query
rewriting.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing information-schema implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- Existing information-schema tests under `packages/libmylite/tests/`
- MySQL 8.4 Reference Manual, `VIEW_TABLE_USAGE`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-view-table-usage-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` table reference:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-reference.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` exists as an `information_schema`
  `SYSTEM VIEW`.
- The table has six non-null `varchar(64)` columns:
  `VIEW_CATALOG`, `VIEW_SCHEMA`, `VIEW_NAME`, `TABLE_CATALOG`, `TABLE_SCHEMA`,
  and `TABLE_NAME`.
- All six columns report character set `utf8mb3`, collation `utf8mb3_bin`,
  maximum character length `64`, octet length `192`, no default, no numeric or
  datetime metadata, and `PRIVILEGES = 'select'` through
  `INFORMATION_SCHEMA.COLUMNS`.
- `INFORMATION_SCHEMA.TABLES` reports the system row with
  `TABLE_SCHEMA = 'information_schema'`, `TABLE_NAME = 'VIEW_TABLE_USAGE'`,
  `TABLE_TYPE = 'SYSTEM VIEW'`, `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- Querying the table for a schema with no views returns zero rows, leaves
  `@@warning_count = 0`, and `ROW_COUNT() = -1`.
- In MySQL, a view referencing a table produces one dependency row. MyLite
  intentionally does not expose such rows until it implements stored view
  descriptors and view dependency analysis.

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- table aliases through the existing information-schema select path;
- descriptor metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- stable zero-row behavior for all user schemas.

Out of scope:

- `CREATE VIEW`, `ALTER VIEW`, `DROP VIEW`, `SHOW CREATE VIEW`, or view
  execution;
- dependency rows for stored views;
- view dependency parsing or normalization;
- privilege filtering;
- `VIEW_ROUTINE_USAGE`;
- generated rows for temporary tables or unsupported SQLite-native views;
- storage, catalog, VFS, or SQLite fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names and aliases.
- Analyzer/runtime: recognizes `VIEW_TABLE_USAGE` as a supported
  information-schema system view and returns an empty row set.
- Information-schema metadata builder: owns `TABLES` and `COLUMNS` synthetic
  rows for this table.
- Catalog: unchanged. There are no stored view descriptors in this slice.
- SQLite storage/VFS: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE;
SELECT * FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE WHERE VIEW_SCHEMA = 'app';
SELECT u.VIEW_NAME FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE AS u LIMIT 1;
SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA = 'information_schema'
   AND TABLE_NAME = 'VIEW_TABLE_USAGE'
 ORDER BY ORDINAL_POSITION;
```

## Runtime Semantics

`VIEW_TABLE_USAGE` is registered in the same static information-schema table
registry as the existing empty `VIEWS`, `TRIGGERS`, `EVENTS`, `ROUTINES`, and
privilege metadata views.

Row production is intentionally empty:

- system rows for `TABLES` and `COLUMNS` are generated from static descriptors;
- user rows are not generated because MyLite has no view catalog yet;
- successful queries return `warning_count == 0`;
- `ROW_COUNT()` after a successful `SELECT` remains the existing query value
  `-1`.

## Diagnostics

The feature relies on existing information-schema diagnostics:

- unknown selected columns fail with the current unknown-column diagnostic;
- unsupported expressions, joins, ordering, predicates, and limits retain the
  current information-schema query subset behavior;
- allocation failures use existing MyLite allocation diagnostics.

No new public diagnostics are introduced.

## Performance

The row set is static and empty. Metadata rows are generated from in-memory
descriptors and do not touch durable catalog rows or SQLite user storage.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- empty row count for user schemas;
- case-insensitive table-name lookup;
- alias projection through the existing information-schema query path;
- column names for `SELECT *`;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view row;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all six columns;
- reopen/file preamble preservation and independent file-backed handles, if a
  new test binary follows the existing information-schema pattern.

Verification before commit:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\.runtime\.information_schema_(views|view_table_usage)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_view_table_usage_expectations.sh
cmake --workflow --preset check
```
