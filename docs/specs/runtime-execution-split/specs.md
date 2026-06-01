# Runtime execution split

## Scope

`packages/libmylite/src/runtime/mylite_execution.c` has grown into a very large
translation unit containing statement planning, execution, generated SQL text
building, MySQL catalog metadata, system table definitions, and assorted local
helpers. This slice is a behavior-preserving refactor whose purpose is to move
cohesive, low-dependency runtime pieces into smaller internal modules without
changing MySQL compatibility behavior.

The initial split extracts:

- `mylite_dynamic_string`: growable string construction used by SQL rendering
  and diagnostics.
- `mylite_execution_catalog`: immutable MySQL-compatible metadata for supported
  character sets, collations, `INFORMATION_SCHEMA` tables, MySQL system tables,
  built-in schemas, and built-in metadata placeholder rows.

## Goals

- Reduce the size and clang-tidy surface of `mylite_execution.c` with real
  translation-unit boundaries.
- Keep MyLite compatibility logic outside the SQLite fork.
- Preserve all existing runtime behavior, metadata values, diagnostics, and
  tests.
- Keep the first split conservative: do not introduce large abstractions,
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

## Compatibility requirements

- Existing MySQL 8.4.9-shaped metadata must be byte-for-byte equivalent at the
  result surface covered by current tests.
- Lookup behavior remains ASCII case-insensitive where the old implementation
  used `text_equals_ascii_case_insensitive`.
- Built-in schema fast paths keep the same ordering assumptions:
  `information_schema`, `mysql`, `performance_schema`, then `sys`.
- Empty placeholder catalog rows remain placeholders; this refactor does not add
  loaded MySQL server data.

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

## Review checklist

- Headers are self-contained and include only what they use.
- New non-static internal symbols use `mylite_`-prefixed names.
- Moved catalog arrays remain immutable and file-local.
- No data copies or runtime table materialization are introduced by the split.
- The execution monolith still owns behavior; the catalog module owns data.
- Tests prove behavior preservation.
