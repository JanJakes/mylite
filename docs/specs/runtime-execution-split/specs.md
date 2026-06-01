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

The fourth split turns the large execution catalog module into a small catalog
module family. The catalog data is immutable and has a narrow accessor surface,
so this split can use true translation-unit boundaries without changing runtime
execution state, planner helper linkage, or SQLite integration behavior.

The fifth split moves JSON session-scalar built-in function evaluation from the
scalar implementation fragment into `mylite_execution_scalar_json`. This is the
first true module extraction from the scalar fragment. It uses a deliberately
small internal scalar execution header for the shared scalar cell type, JSON
mutation kind, JSON scalar entry points, and execution helper wrappers needed
for AST traversal, nested scalar evaluation, and MySQL-compatible diagnostics.

The sixth split keeps the remaining scalar and planning code in the same
translation unit, but breaks the oversized scalar fragment into coarse
scalar-family fragments and two planning fragments that had been left in the
scalar file. This is a mechanical split only: it preserves static linkage,
include order, helper visibility, and runtime behavior while replacing the last
37k-line scalar fragment with files sized for review.

The seventh split applies the same approach to the query-planning fragment. It
keeps row-scalar and SELECT/UPDATE planning in the execution translation unit,
but divides the former 28k-line `mylite_execution_query_planning.inc` into
coarse planner fragments with the original order and static helper visibility.

The eighth split applies the same mechanical same-translation-unit approach to
metadata query execution. It divides the former 23k-line
`mylite_execution_metadata_queries.inc` into high-level SELECT dispatch,
mysql/sys virtual rows, INFORMATION_SCHEMA row synthesis and filtering, table
maintenance, SHOW execution families, SHOW CREATE rendering, and result
completion helpers.

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

The catalog module family owns immutable static catalog data and narrow
lookup/accessor functions. It exposes internal structs because the execution
code reads catalog metadata directly while planning and building result rows.
The modules should prefer accessors over exported arrays so the static data
remains local to the owning catalog translation unit.

The catalog module family includes:

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

The catalog module family does not own:

- Result row construction.
- Predicate evaluation.
- Session state, diagnostics, warnings, or SQL modes.
- Mutable database metadata or user tables.

The fourth split keeps one shared header, `mylite_execution_catalog.h`, and
uses these true C modules:

- `mylite_execution_catalog_charsets.c`: supported character sets, collations,
  scalar collation metadata, and their lookup/accessor functions.
- `mylite_execution_catalog_information_schema.c`: `INFORMATION_SCHEMA`
  keyword rows, table definitions, column definitions, and accessors.
- `mylite_execution_catalog_system_tables.c`: `mysql` and `sys` system table
  definitions, sys configuration trigger metadata, and built-in sys view
  definitions. The `mysql` and `sys` definitions intentionally stay together
  for now because the public accessor returns one ordered system-table catalog.
- `mylite_execution_catalog_builtin.c`: built-in schema descriptors, built-in
  table directories, and placeholder rows for static metadata tables.

These modules may duplicate a tiny file-local ASCII-insensitive lookup helper
when that keeps the helper private and avoids inventing a broader internal
utility API before it earns its cost.

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
- `mylite_execution_metadata_queries.inc`: `DO`, high-level `SELECT`
  dispatch, compound SELECT execution, selected metadata target detection, and
  scalar/row-scalar SELECT routing.
- `mylite_execution_mysql_system_queries.inc`: mysql/sys virtual table SELECT
  execution, built-in row synthesis, mysql system table metadata row helpers,
  and sys view placeholder rows.
- `mylite_execution_information_schema_queries.inc`: INFORMATION_SCHEMA SELECT
  query execution, row-set ownership, static metadata rows, catalog table rows,
  table-status values, and built-in schema table metadata rows.
- `mylite_execution_information_schema_columns.inc`: INFORMATION_SCHEMA column,
  view-usage, InnoDB, constraint, key-usage, referential-constraint, and
  statistics row synthesis.
- `mylite_execution_information_schema_filtering.inc`: INFORMATION_SCHEMA
  result columns, result rows, COUNT(*) rows, WHERE predicate validation and
  evaluation, ORDER/LIMIT planning, source resolution, text comparison, and
  descriptor type metadata helpers.
- `mylite_execution_table_maintenance_queries.inc`: CHECKSUM TABLE and
  ANALYZE/CHECK/OPTIMIZE/REPAIR TABLE placeholder execution.
- `mylite_execution_show_general.inc`: SHOW TABLES, SHOW TABLE STATUS, SHOW
  CHARACTER SET, SHOW COLLATION, SHOW VARIABLES, SHOW STATUS, SHOW TRIGGERS,
  SHOW EVENTS, SHOW OPEN TABLES, SHOW ROUTINE STATUS, SHOW PROCESSLIST, SHOW
  GRANTS, SHOW PRIVILEGES, binary-log/replica SHOW placeholders, and
  SHOW WARNINGS/ERRORS execution.
- `mylite_execution_show_columns_indexes.inc`: SHOW COLUMNS and SHOW INDEX
  execution for user tables and supported mysql system tables.
- `mylite_execution_show_create.inc`: SHOW CREATE TABLE/VIEW/DATABASE,
  SHOW ENGINES, SHOW ENGINE STATUS, SHOW PLUGINS, SHOW DATABASES, and SHOW
  CREATE SQL rendering helpers.
- `mylite_execution_result_completion.inc`: completed-statement row-count
  classification, diagnostics snapshot preservation, and successful-result
  finalization helpers.
- `mylite_execution_ddl_planning.inc`: DDL planning, table/index/constraint
  validation, and catalog mutation helpers.
- `mylite_execution_dml_planning.inc`: DML planning, value conversion, insert
  row execution, duplicate-key handling, and DML validation.
- `mylite_execution_scalar.inc`: scalar dispatch, `LAST_INSERT_ID`, `RAND`,
  and current date/time scalar core support.
- `mylite_execution_scalar_string_core.inc`: basic string length, codepoint,
  case, and trim scalar support.
- `mylite_execution_scalar_temporal_core.inc`: UNIX timestamp, timestamp
  difference, temporal constructor, period, time zone, and temporal extraction
  scalar support.
- `mylite_execution_scalar_string_extended.inc`: string slice, padding,
  bitmask, search, replacement, regular expression, character set, collation,
  and coercibility scalar support.
- `mylite_execution_scalar_misc.inc`: scalar subquery, concatenation,
  `ELT`, `FIELD`, `GREATEST`, `LEAST`, and `INTERVAL` scalar support.
- `mylite_execution_scalar_numeric.inc`: numeric, bitwise, base conversion,
  UUID, Base64, `HEX`, `UNHEX`, `CHAR`, `FORMAT`, and `TRUNCATE` scalar
  support.
- `mylite_execution_scalar_conversion.inc`: scalar `CAST`, `CONVERT`, and
  `COLLATE` support.
- `mylite_execution_scalar_temporal_format.inc`: date/time formatting, parsing,
  interval, and time arithmetic scalar support.
- `mylite_execution_scalar_expression_eval.inc`: scalar arithmetic, logical,
  comparison, and bitwise expression evaluators.
- `mylite_execution_scalar_control.inc`: IF/CASE scalar control flow,
  literal projection, system-variable scalar values, and control-flow
  validation.
- `mylite_execution_scalar_projection.inc`: scalar projection classifiers,
  parenthesis unwrapping, and source-span copying helpers.
- `mylite_execution_delete_planning.inc`: DELETE planning and execution helpers
  that were previously housed in the scalar fragment.
- `mylite_execution_column_planning.inc`: column planning, default handling,
  and column descriptor helpers that were previously housed in the scalar
  fragment.
- `mylite_execution_catalog_loading.inc`: runtime catalog table/column/index,
  foreign-key, and check-constraint loading helpers.
- `mylite_execution_query_planning.inc`: row-scalar planning core, special
  expression dispatch, window functions, integer arithmetic planning, temporal
  dispatch, and JSON dispatch.
- `mylite_execution_row_scalar_string_planning.inc`: row-scalar string and
  regexp function planning.
- `mylite_execution_row_scalar_json_planning.inc`: row-scalar JSON function and
  JSON operator planning.
- `mylite_execution_row_scalar_value_planning.inc`: row-scalar HEX, UNHEX,
  Base64, UUID, binary string, `CHAR`, charset/collation, control-flow,
  conversion, literal, column, concat, and `CONCAT_WS` planning.
- `mylite_execution_row_scalar_temporal_planning.inc`: row-scalar date/time
  formatting, temporal arithmetic, temporal extraction, constructor, period,
  time zone, difference, UNIX timestamp, and timestamp planning.
- `mylite_execution_row_scalar_misc_planning.inc`: row-scalar `FIELD`,
  `GREATEST`, `LEAST`, `INTERVAL`, row-scalar cleanup, descriptor checks, and
  expression containment helpers.
- `mylite_execution_select_column_planning.inc`: SELECT result-column planning
  and result metadata descriptor population.
- `mylite_execution_select_predicate_planning.inc`: SELECT predicate planning,
  subquery predicates, predicate conversion, and predicate value normalization.
- `mylite_execution_select_order_planning.inc`: SELECT ORDER BY, SELECT LIMIT,
  and DELETE LIMIT planning helpers.
- `mylite_execution_update_planning_helpers.inc`: UPDATE assignment, UPDATE
  value conversion, UPDATE validation, and UPDATE LIMIT planning helpers.
- `mylite_execution_show_helpers.inc`: `SHOW` filtering, sorting, and display
  formatting helpers.
- `mylite_execution_sql_builders.inc`: physical SQLite SQL rendering, statement
  preparation, binding, stepping, and result extraction helpers.
- `mylite_execution_diagnostics.inc`: MySQL-compatible diagnostics, warnings,
  notes, and parse-status mapping helpers.

Fragment boundaries should remain coarse and logical. New work may move a
fragment into a real `.c` module only after the required private helper surface
is narrow enough to be reviewed as an intentional internal API.

### `mylite_execution_scalar_json`

The JSON scalar module owns session-scalar implementations for:

- `JSON_VALID`
- `JSON_EXTRACT`
- `JSON_VALUE`
- `JSON_CONTAINS`
- `JSON_CONTAINS_PATH`
- `JSON_LENGTH`
- `JSON_KEYS`
- `JSON_TYPE`
- `JSON_QUOTE`
- `JSON_UNQUOTE`
- `JSON_ARRAY`
- `JSON_OBJECT`
- `JSON_SET`
- `JSON_INSERT`
- `JSON_REPLACE`
- `JSON_REMOVE`

The module may call a small internal execution helper surface for:

- AST child access and parenthesis unwrapping.
- Generic nested scalar expression evaluation.
- Unsigned integer literal parsing used by JSON constructor integer support.
- MySQL-compatible diagnostics and JSON warning/error mapping.

The module must not own row-scalar SQL rendering, row-scalar planning, generic
session-scalar dispatch, or non-JSON scalar function families. The exported
entry points are internal to the `mylite` static library and remain outside the
public MyLite ABI.

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
- JSON scalar function result values, NULL handling, unsupported-shape errors,
  warnings, SQLSTATEs, and parameter-count diagnostics must remain identical to
  the previous scalar fragment behavior.

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
10. Split the execution catalog data into a small family of true C modules:
    character sets/collations, `INFORMATION_SCHEMA`, `mysql`/`sys` system
    tables, and built-in static placeholder catalogs.

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
- Catalog-family modules keep their static arrays file-local and expose only
  the existing accessor surface.
- Splitting catalog modules does not introduce generated sources, dynamic
  catalog loading, or runtime table materialization.
- Tests prove behavior preservation.
