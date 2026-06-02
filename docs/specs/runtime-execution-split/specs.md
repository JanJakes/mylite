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

The ninth split applies the same approach to the DML planning fragment. The
former 19k-line `mylite_execution_dml_planning.inc` also contained INSERT
execution, INSERT ... SELECT, UPDATE execution, SELECT planning, aggregate
planning/execution, VALUES, scalar projection SELECT execution, and scalar
warning helpers. The split keeps those routines in the execution translation
unit but gives each family a named fragment.

The tenth split applies the same behavior-preserving same-translation-unit
approach to DDL planning. The former 19k-line DDL planning fragment mixed
CREATE TABLE analysis, constraint/index planning, table option decoding,
CREATE TABLE execution/catalog mutation, schema/table administration, ALTER
TABLE action families, physical rebuild helpers, and LOAD DATA target
planning. The split keeps static helper visibility unchanged while making each
DDL family separately reviewable.

The eleventh split applies the same mechanical treatment to the SQL builder
fragment. The former 18k-line SQL builder fragment contained physical-name
rendering, DDL/admin SQL rendering, INSERT and INSERT ... SELECT SQL, SELECT
and row-scalar rendering, aggregate and predicate rendering, DELETE/UPDATE
rendering, SQLite execution helpers, duplicate-key and foreign-key write
helpers, parameter binding, selected-row extraction, and parser helper tails.
The split keeps these helpers in the execution translation unit and preserves
the exact renderer/binder order.

The twelfth split resumes true module extraction in the catalog module family.
It moves immutable `sys` built-in view SQL definitions, SHOW CREATE text, and
view lookup/count accessors out of the combined mysql/sys system-table catalog
module. The remaining system-table module keeps mysql/sys table column
metadata, sys configuration trigger metadata, and the unified ordered
system-table descriptor array.

The thirteenth split completes the true module extraction of the combined
mysql/sys system-table catalog descriptor file. The former system-table module
becomes a small provider aggregator that preserves the public ordered
descriptor API. The static descriptor data moves into owner modules for mysql
privilege/auth tables, mysql service/help/log/cost metadata, mysql replication
metadata, remaining mysql system tables, sys core tables, sys summary tables,
and sys schema tables. Each owner keeps its column/key/privilege/index arrays
file-local and exposes only internal `count/at` providers.

The fourteenth split starts reducing the mutable catalog monolith by moving the
catalog schema migration ladder into `mylite_catalog_migrations.c`. The main
catalog module still owns catalog initialization, state loading, descriptor
materialization, validation, and mutation APIs. The migration module owns only
the versioned one-step SQL migrations and exposes a single private
`mylite_catalog_migrate_schema_one_step` entry point through
`mylite_catalog_internal.h`.

The fifteenth split extracts the catalog SQLite helper layer into
`mylite_catalog_sqlite.c`. The helper module owns the shared SQLite execution,
prepare/bind/finalize, changed-row, checked-column extraction, and integer
range conversion helpers used by catalog state, mutation, materialization, and
migration code. Catalog object validation and descriptor mutation logic remain
in `mylite_catalog.c`; migrations still expose only their one-step private
entry point.

The sixteenth split moves reusable catalog validation primitives into
`mylite_catalog_validation.c`. This module owns database readiness checks,
identifier length/reservation checks, enum/domain validators, positive id and
generation checks, callback validators, and catalog boolean extraction
validation. Validation that depends on catalog-private value structs, such as
column default semantics, stays in `mylite_catalog.c` until those ownership
types are split deliberately.

The seventeenth split moves catalog handle state, file-backed schema
bootstrap/load/migration coordination, descriptor-cache invalidation, mutation
transaction lifecycle, and catalog-generation transitions into
`mylite_catalog_state.c`. The mutable descriptor row APIs remain in
`mylite_catalog.c`. Descriptor mutation code uses a small private
generation-change helper surface instead of owning catalog-global transaction
details directly.

The eighteenth split continues true-module extraction for immutable
`INFORMATION_SCHEMA` catalog metadata. Keyword rows move into
`mylite_execution_catalog_information_schema_keywords.c`; InnoDB table column
definitions and table definitions move into
`mylite_execution_catalog_information_schema_innodb.c`. The original
`mylite_execution_catalog_information_schema.c` remains the public-order
aggregator for non-InnoDB definitions and keeps public lookup behavior
unchanged.

The nineteenth split extracts mutable catalog read and descriptor
materialization logic into `mylite_catalog_read.c`. The new module owns catalog
SELECT queries, public read/iteration APIs, next-id reads, post-insert
descriptor reads, and descriptor materializers. Shared validation and
SQLite-level lookup helpers use a small private surface in
`mylite_catalog_internal.h`.

The twentieth split moves column descriptor value handling into
`mylite_catalog_column_values.c`. The new module owns column insert/replace
bind helpers, column default-kind storage predicates, and validation for column
defaults and generated-column expression metadata. `mylite_catalog.c` keeps
descriptor mutation orchestration, table descriptor validation, delete/update
helpers, and SQL statements that mutate private catalog tables.

The twenty-first split reduces the sys summary metadata module into a small
ordered aggregator plus family owners. Host summary, InnoDB summary, IO/latest
file summary, memory summary, and instrumentation-check descriptors each live in
their own true C module. The existing `mylite_execution_catalog_sys_summary_tables.c`
entry point preserves normal and `x$` sys table ordering through private
provider arrays.

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
- `mylite_execution_catalog_information_schema.c`: non-InnoDB
  `INFORMATION_SCHEMA` table definitions and column definitions, public-order
  provider aggregation, and table-definition lookup accessors.
- `mylite_execution_catalog_information_schema_internal.h`: private provider
  API for information-schema metadata owner modules.
- `mylite_execution_catalog_information_schema_keywords.c`:
  `INFORMATION_SCHEMA.KEYWORDS` rows and keyword lookup/count accessors.
- `mylite_execution_catalog_information_schema_innodb.c`: InnoDB
  `INFORMATION_SCHEMA` column definitions and table-definition provider.
- `mylite_execution_catalog_system_tables.c`: `mysql` and `sys` system table
  descriptor provider aggregation and public ordered system-table lookup
  accessors.
- `mylite_execution_catalog_system_tables_internal.h`: private provider API
  for system-table descriptor owner modules.
- `mylite_execution_catalog_mysql_auth_tables.c`: mysql privilege/auth table
  descriptors for `user`, global grants, database/table/column/procedure
  privileges, roles, and password history.
- `mylite_execution_catalog_mysql_service_tables.c`: mysql component, cost,
  function, GTID, log, help, and NDB binlog descriptors in public catalog
  order.
- `mylite_execution_catalog_mysql_replication_tables.c`: mysql replication
  metadata table descriptors.
- `mylite_execution_catalog_mysql_misc_tables.c`: remaining mysql plugin,
  cost/server, time-zone, and InnoDB stats descriptors.
- `mylite_execution_catalog_sys_core_tables.c`: `sys.sys_config`,
  `sys.version`, and sys configuration trigger metadata/accessors.
- `mylite_execution_catalog_sys_summary_tables.c`: ordered provider aggregator
  for normal and `x$` sys summary table descriptors.
- `mylite_execution_catalog_sys_summary_host_tables.c`: sys host-summary table
  descriptors and `x$` variants.
- `mylite_execution_catalog_sys_summary_innodb_tables.c`: sys InnoDB summary
  table descriptors and `x$` variants.
- `mylite_execution_catalog_sys_summary_io_tables.c`: sys IO and latest-file
  summary table descriptors and `x$` variants.
- `mylite_execution_catalog_sys_summary_memory_tables.c`: sys memory summary
  table descriptors and `x$` variants.
- `mylite_execution_catalog_sys_summary_instrumentation_tables.c`:
  `sys.ps_check_lost_instrumentation` descriptor ownership.
- `mylite_execution_catalog_sys_summary_tables_internal.h`: private sys summary
  family provider API.
- `mylite_execution_catalog_sys_schema_tables.c`: sys schema/statistics table
  descriptors.
- `mylite_execution_catalog_sys_views.c`: built-in sys view definitions,
  SHOW CREATE text, and sys view lookup/count accessors.
- `mylite_execution_catalog_builtin.c`: built-in schema descriptors, built-in
  table directories, and placeholder rows for static metadata tables.

These modules may duplicate a tiny file-local ASCII-insensitive lookup helper
when that keeps the helper private and avoids inventing a broader internal
utility API before it earns its cost.

### `mylite_catalog`

The mutable catalog module family owns MyLite's private metadata tables and
their C descriptor API. It uses true C modules where the dependency surface is
small enough to keep internal helper exports narrow.

The mutable catalog module family includes:

- `mylite_catalog.c`: descriptor create/update/delete/mutation APIs, table
  descriptor validation, delete/update helpers, and SQL statements that mutate
  private catalog rows.
- `mylite_catalog_column_values.c`: column descriptor value structs,
  insert/replace bind helpers, default-kind storage predicates, and
  default/generated-column validation.
- `mylite_catalog_read.c`: public catalog read/iteration APIs, SQLite-level
  one-row lookup helpers used by mutation code, next-id reads, post-insert
  descriptor reads, and descriptor materializers.
- `mylite_catalog_sqlite.c`: shared SQLite prepare/bind/finalize, checked
  column extraction, changed-row, and integer conversion helpers.
- `mylite_catalog_validation.c`: reusable catalog handle, identifier, enum,
  id, generation, bool, and callback validation.
- `mylite_catalog_state.c`: catalog handle lifecycle, schema bootstrap/load,
  descriptor cache invalidation, mutation transaction lifecycle, and generation
  transactions.
- `mylite_catalog_migrations.c`: versioned schema migration steps.

The mutable catalog module family does not own:

- SQL parser or planner behavior.
- Runtime result-row construction.
- Static MySQL `INFORMATION_SCHEMA`, `mysql`, or `sys` compatibility metadata.
- SQLite fork patches or SQLite virtual table hooks.

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
- `mylite_execution_ddl_planning.inc`: CREATE TABLE planning core, column item
  planning, inline primary/unique/index collection, generated column
  finalization, and generated/CHECK expression rendering.
- `mylite_execution_create_table_constraints.inc`: CREATE TABLE CHECK,
  FOREIGN KEY, secondary-index, primary-key, and AUTO_INCREMENT validation and
  planning helpers.
- `mylite_execution_create_table_variants.inc`: CREATE TABLE LIKE and CREATE
  TABLE ... SELECT planning, descriptor cloning, source-column inference, row
  validation, and copy execution.
- `mylite_execution_table_options_planning.inc`: CREATE/ALTER/SCHEMA table
  option validation, character set and collation option resolution, storage
  statistics option handling, normalized table option decoding, CREATE TABLE
  cleanup, and `sql_require_primary_key` validation.
- `mylite_execution_create_table_execution.inc`: CREATE TABLE catalog
  mutation, persistent and temporary table descriptor construction, physical
  CREATE/DROP TABLE execution helpers, and index/foreign-key/check identifier
  assignment.
- `mylite_execution_schema_table_admin.inc`: CREATE/DROP DATABASE, TRUNCATE
  TABLE, RENAME TABLE, and ALTER TABLE RENAME planning/execution.
- `mylite_execution_alter_table_add_column.inc`: ALTER TABLE ADD COLUMN and
  ADD PRIMARY KEY planning, validation, mutation, warnings, and physical
  ALTER execution.
- `mylite_execution_alter_table_add_index.inc`: ALTER TABLE ADD INDEX and
  CREATE INDEX planning, index option decoding, add-index execution, and
  fulltext/spatial/hash warning handling.
- `mylite_execution_alter_table_foreign_key_index.inc`: ALTER TABLE ADD/DROP
  FOREIGN KEY, DROP CONSTRAINT, DROP INDEX, RENAME INDEX, foreign-key
  dependency rejection, and loaded-index validation helpers.
- `mylite_execution_alter_table_check_constraints.inc`: ALTER TABLE ADD CHECK,
  DROP CHECK, and ALTER CHECK planning, temporary rebuild planning,
  check-catalog row insertion, and physical check rebuild helpers.
- `mylite_execution_alter_table_drop_rename_column.inc`: ALTER TABLE DROP
  PRIMARY KEY, AUTO_INCREMENT option, DROP COLUMN dependency planning, and
  RENAME COLUMN planning/execution.
- `mylite_execution_alter_table_modify_options.inc`: ALTER TABLE MODIFY/
  CHANGE COLUMN, SET/DROP DEFAULT, column/index visibility, default
  charset/collation, CONVERT CHARACTER SET, COMMENT, ORDER BY, FORCE, key
  maintenance, rebuild SQL, and rename collision helpers.
- `mylite_execution_load_data_planning.inc`: LOAD DATA target-column
  resolution, target index collection, and IGNORE line-count parsing.
- `mylite_execution_dml_planning.inc`: INSERT/REPLACE target planning,
  duplicate-key assignment planning, AUTO_INCREMENT planning, and planned value
  cleanup.
- `mylite_execution_insert_execution.inc`: INSERT/REPLACE row execution,
  LOAD DATA row import and conversion, unique/foreign-key conflict handling,
  and REPLACE conflicting-row deletion.
- `mylite_execution_insert_select.inc`: INSERT ... SELECT and REPLACE ...
  SELECT planning, materialization, row validation, and source compatibility
  checks.
- `mylite_execution_update_planning.inc`: single-table and joined UPDATE
  planning, assignment planning, target validation, and executable update
  plan setup.
- `mylite_execution_update_execution.inc`: UPDATE execution, matched-row
  accounting, non-strict adjustment diagnostics, parent foreign-key update
  actions, arithmetic assignment preparation, and table AUTO_INCREMENT/mtime
  maintenance.
- `mylite_execution_select_planning_core.inc`: SELECT source planning, joined
  SELECT planning, index-hint validation, row-scalar SELECT planning, and
  SELECT plan cleanup.
- `mylite_execution_grouped_aggregate_planning.inc`: grouped aggregate
  planning, grouped HAVING/ORDER handling, and temporal literal conversion
  helpers shared by grouped predicates.
- `mylite_execution_select_execution.inc`: SELECT and row-scalar SELECT
  execution, row-scalar result metadata, FOUND_ROWS accounting, and row-scalar
  JSON error mapping.
- `mylite_execution_aggregate_execution.inc`: COUNT, column aggregate, and
  grouped aggregate execution, aggregate result metadata, and aggregate
  result formatting.
- `mylite_execution_scalar_projection_queries.inc`: scalar projection SELECT,
  VALUES statement execution, scalar result metadata, session scalar warning
  emission, SELECT modifier warnings, and function argument-count diagnostics.
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
- `mylite_execution_sql_builders.inc`: physical table/view/index/check names,
  CREATE/DROP/ALTER/TRUNCATE SQL rendering, CREATE TABLE constraints/indexes,
  ALTER TABLE rebuild SQL, key-part rendering, and quoted default helpers.
- `mylite_execution_insert_sql_builders.inc`: INSERT, INSERT ... SELECT,
  REPLACE ... SELECT, temporary materialization, compound branch, validation,
  and CREATE TABLE ... SELECT SQL rendering.
- `mylite_execution_select_sql_builders.inc`: SELECT statement SQL,
  projection, FROM/JOIN rendering, source aliases, and row-scalar SELECT
  wrapper SQL.
- `mylite_execution_row_scalar_sql_core.inc`: generic row-scalar expression
  rendering, window functions, RAND, conversion, integer arithmetic,
  concatenation, FIELD/GREATEST/LEAST/INTERVAL, and shared stack helpers.
- `mylite_execution_row_scalar_sql_functions.inc`: row-scalar temporal,
  string, regexp, UUID, UNHEX, Base64, character, and registered-function SQL
  rendering.
- `mylite_execution_row_scalar_sql_json_control.inc`: row-scalar JSON,
  control-flow, substring/slice, HEX, and related argument SQL rendering.
- `mylite_execution_aggregate_predicate_sql_builders.inc`: FOUND_ROWS,
  COUNT, column aggregates, GROUP BY aggregate SQL, SELECT predicates,
  EXISTS/IN subquery SQL, ORDER BY, and LIMIT rendering.
- `mylite_execution_dml_sql_builders.inc`: DELETE and UPDATE SQL rendering,
  joined/limited rowid filters, update assignment SQL, auto-update column
  rendering, and changed-row predicates.
- `mylite_execution_sqlite_write_helpers.inc`: SQLite schema/control
  execution, statement preparation/finalization, INSERT stepping, duplicate-key
  handling, foreign-key validation, unique-key conflict SQL, and duplicate-key
  display formatting.
- `mylite_execution_row_scalar_parameter_binding.inc`: SELECT, INSERT SELECT,
  and row-scalar expression parameter binding for scalar, temporal, string,
  JSON, control-flow, UUID, and character expressions.
- `mylite_execution_predicate_dml_parameter_binding.inc`: predicate, aggregate,
  DELETE, UPDATE, changed-condition, and planned-value parameter binding.
- `mylite_execution_sqlite_result_extraction.inc`: selected SQLite row/value
  extraction, rowid alias selection, AST child helper tails, script statement
  counting, and parse-error mapping.
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
