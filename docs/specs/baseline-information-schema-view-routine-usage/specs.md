# Baseline INFORMATION_SCHEMA VIEW_ROUTINE_USAGE

## Status

This phase adds `INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` as a MySQL-shaped
synthetic information-schema system view. The table is queryable and has exact
MySQL 8.4.9 metadata, but it returns no rows until MyLite implements stored
routine descriptors and view-to-routine dependency analysis.

The slice is intentionally metadata-only for MyLite user objects. Existing view
descriptors can record direct table dependencies, but they do not record stored
function dependencies, and MyLite does not yet implement `CREATE FUNCTION`.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing information-schema implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- Existing view metadata specs:
  `docs/specs/baseline-information-schema-views/specs.md` and
  `docs/specs/baseline-information-schema-view-table-usage/specs.md`
- MySQL 8.4 Reference Manual, `VIEW_ROUTINE_USAGE`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-view-routine-usage-table.html
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

- `INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` exists as an `information_schema`
  `SYSTEM VIEW`.
- The table has six non-null `varchar(64)` columns:
  `TABLE_CATALOG`, `TABLE_SCHEMA`, `TABLE_NAME`, `SPECIFIC_CATALOG`,
  `SPECIFIC_SCHEMA`, and `SPECIFIC_NAME`.
- All six columns report character set `utf8mb3`, maximum character length
  `64`, octet length `192`, no default, no numeric or datetime metadata, and
  `PRIVILEGES = 'select'` through `INFORMATION_SCHEMA.COLUMNS`.
- The first five columns report collation `utf8mb3_bin`; `SPECIFIC_NAME`
  reports collation `utf8mb3_general_ci`.
- `INFORMATION_SCHEMA.TABLES` reports the system row with
  `TABLE_SCHEMA = 'information_schema'`, `TABLE_NAME = 'VIEW_ROUTINE_USAGE'`,
  `TABLE_TYPE = 'SYSTEM VIEW'`, `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- A view that uses a native function such as `ABS()` produces no
  `VIEW_ROUTINE_USAGE` row.
- A view that uses a stored function produces one row naming the view and the
  stored function specific schema/name.
- Successful supported reads leave `@@warning_count = 0`, and `ROW_COUNT()`
  reports `-1` after the `SELECT`.

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- table aliases through the existing information-schema select path;
- descriptor metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- stable zero-row behavior for all MyLite schemas and views.

Out of scope:

- `CREATE FUNCTION`, `ALTER FUNCTION`, `DROP FUNCTION`, stored routine
  execution, or routine descriptors;
- dependency rows for stored functions used by views;
- dependency rows for native or loadable functions, which MySQL also omits;
- privilege filtering;
- generated rows for temporary views or unsupported SQLite-native views;
- storage, catalog, VFS, or SQLite fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names and aliases.
- Analyzer/runtime: recognizes `VIEW_ROUTINE_USAGE` as a supported
  information-schema system view and returns an empty row set.
- Information-schema metadata builder: owns `TABLES` and `COLUMNS` synthetic
  rows for this table.
- Catalog: unchanged. Existing view descriptors are read for other metadata
  views, but this slice does not add stored-routine dependency descriptors.
- SQLite storage/VFS: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE;
SELECT * FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE WHERE TABLE_SCHEMA = 'app';
SELECT u.TABLE_NAME FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE AS u LIMIT 1;
SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA = 'information_schema'
   AND TABLE_NAME = 'VIEW_ROUTINE_USAGE'
 ORDER BY ORDINAL_POSITION;
```

## Runtime Semantics

`VIEW_ROUTINE_USAGE` is registered in the static information-schema table
registry beside the existing `VIEWS` and `VIEW_TABLE_USAGE` definitions.

Row production is intentionally empty:

- system rows for `TABLES` and `COLUMNS` are generated from static descriptors;
- user rows are not generated because MyLite has no stored routine catalog or
  routine dependency descriptors;
- existing MyLite views do not add rows, including views that contain native
  function calls;
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
- empty row count after a MyLite view exists;
- case-insensitive table-name lookup;
- alias projection through the existing information-schema query path;
- column names for `SELECT *`;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view row;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all six columns;
- MySQL-runtime observation that native functions are omitted and stored
  function dependencies are the deferred future row-producing case;
- reopen/file preamble preservation and independent file-backed handles.

Verification before commit:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\.runtime\.information_schema_(views|view_table_usage|view_routine_usage)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_view_routine_usage_expectations.sh
cmake --workflow --preset check
```
