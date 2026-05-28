# Baseline INFORMATION_SCHEMA OPTIMIZER_TRACE

## Status

This phase adds `INFORMATION_SCHEMA.OPTIMIZER_TRACE` as a MySQL-shaped
synthetic information-schema system view. The table is queryable and has exact
MySQL 8.4.9 metadata, but it returns no rows because MyLite does not implement
optimizer tracing or the mutable `optimizer_trace` system-variable family yet.

The slice is intentionally metadata-only. It does not add optimizer trace
capture, trace memory limits, JSON trace generation, optimizer privilege
filtering, or mutable trace session state.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing information-schema implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- Existing information-schema tests under `packages/libmylite/tests/`
- MySQL 8.4 Reference Manual, `OPTIMIZER_TRACE`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-optimizer-trace-table.html
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

- `INFORMATION_SCHEMA.OPTIMIZER_TRACE` exists as an `information_schema`
  `SYSTEM VIEW`.
- The table has four non-null columns in order: `QUERY`, `TRACE`,
  `MISSING_BYTES_BEYOND_MAX_MEM_SIZE`, and `INSUFFICIENT_PRIVILEGES`.
- `QUERY` and `TRACE` report `DATA_TYPE = 'varchar'`,
  `COLUMN_TYPE = 'varchar(65535)'`, character set `utf8mb3`, collation
  `utf8mb3_general_ci`, maximum character length `21845`, octet length
  `65535`, and `COLUMN_DEFAULT = ''`.
- `MISSING_BYTES_BEYOND_MAX_MEM_SIZE` reports `DATA_TYPE = 'int'`,
  `COLUMN_TYPE = 'int'`, `COLUMN_DEFAULT = ''`, and SQL `NULL` for numeric
  precision, scale, character set, and collation metadata.
- `INSUFFICIENT_PRIVILEGES` reports `DATA_TYPE = 'tinyint'`,
  `COLUMN_TYPE = 'tinyint(1)'`, `COLUMN_DEFAULT = ''`, and SQL `NULL` for
  numeric precision, scale, character set, and collation metadata.
- `INFORMATION_SCHEMA.TABLES` reports the system row with
  `TABLE_SCHEMA = 'information_schema'`, `TABLE_NAME = 'OPTIMIZER_TRACE'`,
  `TABLE_TYPE = 'SYSTEM VIEW'`, `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- The target runtime starts with `@@optimizer_trace =
  'enabled=off,one_line=off'`, and `SELECT COUNT(*) FROM
  INFORMATION_SCHEMA.OPTIMIZER_TRACE` returns `0`.
- When MySQL tracing is enabled with `SET optimizer_trace='enabled=on'`, a
  later optimizable statement can produce an `OPTIMIZER_TRACE` row. MyLite
  intentionally defers this dynamic row-producing behavior.
- Successful supported reads leave `@@warning_count = 0`, and `ROW_COUNT()`
  reports `-1` after the `SELECT`.

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.OPTIMIZER_TRACE` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- table aliases through the existing information-schema select path;
- descriptor metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- stable zero-row behavior in all MyLite sessions.

Out of scope:

- `optimizer_trace`, `optimizer_trace_features`, `optimizer_trace_limit`,
  `optimizer_trace_max_mem_size`, and `optimizer_trace_offset` variable
  semantics;
- optimizer trace collection, JSON trace formatting, memory truncation, or
  privilege-filtered trace visibility;
- dynamic rows for traced statements;
- trace persistence across statements or sessions;
- storage, catalog, VFS, or SQLite fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names and aliases.
- Analyzer/runtime: recognizes `OPTIMIZER_TRACE` as a supported
  information-schema system view and returns an empty row set.
- Information-schema metadata builder: owns `TABLES` and `COLUMNS` synthetic
  rows for this table.
- Optimizer/planner: unchanged. No trace events, JSON documents, or memory
  accounting are produced in this slice.
- SQLite storage/VFS: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT COUNT(*) FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
SELECT * FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
SELECT t.QUERY FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE AS t LIMIT 1;
SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA = 'information_schema'
   AND TABLE_NAME = 'OPTIMIZER_TRACE'
 ORDER BY ORDINAL_POSITION;
```

## Runtime Semantics

`OPTIMIZER_TRACE` is registered in the static information-schema table
registry. Row production is intentionally empty:

- system rows for `TABLES` and `COLUMNS` are generated from static descriptors;
- user rows are not generated because MyLite has no optimizer trace subsystem;
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
descriptors and do not touch durable catalog rows, user storage, or query
planner internals.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- empty row count in the default MySQL and MyLite session state;
- case-insensitive table-name lookup;
- alias projection through the existing information-schema query path;
- column names for `SELECT *`;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view row;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all four columns;
- MySQL-runtime observation that enabling `optimizer_trace` can produce a
  dynamic row, which this baseline defers;
- reopen/file preamble preservation and independent file-backed handles.

Verification before commit:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\.runtime\.information_schema_(optimizer_trace|static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_optimizer_trace_expectations.sh
cmake --workflow --preset check
```
