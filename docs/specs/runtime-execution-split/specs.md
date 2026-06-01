# Runtime execution split

## Scope

`packages/libmylite/src/runtime/mylite_execution.c` has grown into a very large
translation unit containing statement planning, execution, generated SQL text
building, MySQL catalog metadata, system table definitions, and assorted local
helpers. This slice is a behavior-preserving refactor whose purpose is to move
cohesive, low-dependency runtime pieces into smaller internal modules, and to
split tightly coupled runtime implementation into named fragments, without
changing MySQL compatibility behavior.

The initial split extracts:

- `mylite_dynamic_string`: growable string construction used by SQL rendering
  and diagnostics.
- `mylite_execution_catalog`: immutable MySQL-compatible metadata for supported
  character sets, collations, `INFORMATION_SCHEMA` tables, MySQL system tables,
  built-in schemas, and built-in metadata placeholder rows.

The second split extracts:

- `mylite_execution_system_variables`: immutable system variable descriptors,
  SHOW STATUS descriptors, SQL mode descriptor parsing/formatting, and system
  variable scope/classification helpers.

The third split keeps the remaining execution implementation in one translation
unit but physically splits it into large include fragments. This is intentional:
the runtime still has broad private helper coupling, and forcing the next split
into separately compiled objects would require exporting hundreds of helpers
before a stable internal API exists. The fragments provide smaller files and
clearer logical ownership while preserving existing `static` linkage.

## Goals

- Reduce the size and review surface of `mylite_execution.c`.
- Use real translation-unit boundaries where the dependency surface is already
  narrow.
- Use same-translation-unit fragments for broad runtime slices until their
  cross-cutting helper API is designed.
- Keep MyLite compatibility logic outside the SQLite fork.
- Preserve all existing runtime behavior, metadata values, diagnostics, and
  tests.
- Keep each split conservative: do not introduce large abstractions,
  generated catalog sources, or many small files until the next extraction
  justifies them.

## Non-goals

- No MySQL behavior changes.
- No SQLite fork changes.
- No migration of planner, executor, expression semantics, or storage mutation
  logic in this slice.
- No conversion of catalog metadata to generated code or external data files.
- No broad renaming beyond names required for the extracted internal modules.

## Module boundaries

### `mylite_dynamic_string`

The dynamic string helper owns only:

- `struct mylite_dynamic_string`.
- initialization, cleanup, append, reserve, quoted identifier append, and take
  operations.

It has no dependency on SQLite, parser nodes, catalog metadata, or database
handles. It may include the public MyLite status constants for return codes and
standard C allocation/string headers.

### `mylite_execution_catalog`

The catalog module owns immutable static catalog data and narrow lookup/accessor
functions. It exposes internal structs because the execution code reads catalog
metadata directly while planning and building result rows. The module should
prefer accessors over exported arrays so the static data remains local to the
catalog translation unit.

The catalog module includes:

- Supported runtime character sets and collations.
- MySQL catalog rows for `INFORMATION_SCHEMA.CHARACTER_SETS` and
  `INFORMATION_SCHEMA.COLLATIONS`.
- Scalar collation lookup metadata.
- `INFORMATION_SCHEMA.KEYWORDS` rows.
- `INFORMATION_SCHEMA` table definitions and their column definitions.
- MySQL system table definitions and their SHOW/DESCRIBE metadata.
- Built-in schema descriptors and table directories.
- Built-in placeholder rows for catalog tables such as `FILES`,
  `INNODB_TABLESPACES`, and `ST_UNITS_OF_MEASURE`.

The catalog module does not own:

- Result row construction.
- Predicate evaluation.
- Session state, diagnostics, warnings, or SQL modes.
- Mutable database metadata or user tables.

### `mylite_execution_system_variables`

The system variables module owns static descriptor data and pure classification
helpers for MySQL-compatible system variable handling. It exposes descriptors
through accessors and keeps descriptor arrays file-local.

The system variables module includes:

- System variable name, kind, and SHOW GLOBAL/SESSION visibility descriptors.
- SHOW STATUS placeholder descriptors and their SHOW GLOBAL/SESSION visibility.
- SQL mode token descriptors, parsing, canonical formatting, and token matching.
- Scope and mutability classification helpers for variable resolution and SET
  validation.

The system variables module does not own:

- Session state mutation.
- Dynamic system variable value rendering.
- Diagnostics and warning emission.
- Database/schema lookup for selected schema character set and collation values.
- Time zone state mutation.

### Runtime implementation fragments

The `mylite_execution_*.inc` files are implementation fragments included only
by `mylite_execution.c`. They are not headers, are not separately compiled, and
must not be included by other files. They preserve the previous single
translation unit and its file-local symbols while shrinking the physical
working files.

The fragments are:

- `mylite_execution_statement_core.inc`: statement completion, dispatch,
  session state, prepared statements, SET, transaction, savepoint, and lock
  handling.
- `mylite_execution_ddl_statements.inc`: high-level DDL/schema statement
  execution.
- `mylite_execution_dml_statements.inc`: high-level INSERT/REPLACE/LOAD
  DATA/DELETE/UPDATE execution.
- `mylite_execution_metadata_queries.inc`: `DO`, `SELECT` entry points,
  `INFORMATION_SCHEMA` SELECT execution, read-only `mysql`/`sys` virtual table
  row synthesis, and tabular `SHOW` execution.
- `mylite_execution_ddl_planning.inc`: DDL planning, table/index/constraint
  validation, and catalog mutation helpers.
- `mylite_execution_dml_planning.inc`: DML planning, value conversion, insert
  row execution, duplicate-key handling, and DML validation.
- `mylite_execution_scalar.inc`: session scalar expression evaluation and
  scalar built-in function support.
- `mylite_execution_catalog_loading.inc`: runtime catalog table/column/index,
  foreign-key, and check-constraint loading helpers.
- `mylite_execution_query_planning.inc`: row-scalar, predicate, ordering,
  aggregate, and query planning helpers.
- `mylite_execution_show_helpers.inc`: `SHOW` filtering, sorting, and display
  formatting helpers.
- `mylite_execution_sql_builders.inc`: physical SQLite SQL rendering, statement
  preparation, binding, stepping, and result extraction helpers.
- `mylite_execution_diagnostics.inc`: MySQL-compatible diagnostics, warnings,
  notes, and parse-status mapping helpers.

Fragment boundaries should remain coarse and logical. New work may move a
fragment into a real `.c` module only after the required private helper surface
is narrow enough to be reviewed as an intentional internal API.

## Compatibility requirements

- Existing MySQL 8.4.9-shaped metadata must be byte-for-byte equivalent at the
  result surface covered by current tests.
- Lookup behavior remains ASCII case-insensitive where the old implementation
  used `text_equals_ascii_case_insensitive`.
- Built-in schema fast paths keep the same ordering assumptions:
  `information_schema`, `mysql`, `performance_schema`, then `sys`.
- Empty placeholder catalog rows remain placeholders; this refactor does not add
  loaded MySQL server data.
- System variable names, scope handling, warning behavior, placeholder values,
  and SQL mode canonical text must stay identical to the previous runtime
  behavior.
- Metadata SELECT and SHOW result columns, row order, warnings, errors,
  SQLSTATEs, and placeholder rows must remain identical to the previous runtime
  behavior.

## Implementation plan

1. Add this spec and a task checklist under
   `docs/specs/runtime-execution-split/`.
2. Extract `mylite_dynamic_string.h/.c` and update generated-SQL callers to use
   the module-prefixed type and functions.
3. Extract catalog structs, metadata arrays, and lookup/accessor functions into
   `mylite_execution_catalog.h/.c`.
4. Replace direct monolith references to moved static arrays with catalog
   accessors.
5. Register the new sources in `packages/libmylite/CMakeLists.txt`.
6. Run focused build/tests and the full check workflow.
7. Perform a release-gate style review of the split and fix any findings before
   committing.
8. Extract system variable descriptors, SHOW STATUS descriptors, SQL mode
   descriptor helpers, and pure system variable classification helpers into
   `mylite_execution_system_variables.h/.c`.
9. Split the tightly coupled execution implementation into large
   same-translation-unit fragments, avoiding new exported helper surfaces until
   a later true-module boundary is ready.

## Review checklist

- Headers are self-contained and include only what they use.
- New non-static internal symbols use `mylite_`-prefixed names.
- Moved catalog arrays remain immutable and file-local.
- No data copies or runtime table materialization are introduced by the split.
- The execution monolith still owns behavior; the catalog module owns data.
- System variable session mutation and value rendering stay in the execution
  monolith until a later runtime-state boundary is designed.
- Runtime fragments do not introduce new exported symbols, writable metadata
  state, or a second query engine.
- `mylite_execution.c` is the only file that includes runtime implementation
  fragments.
- Tests prove behavior preservation.
