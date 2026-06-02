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

The twenty-second split reduces the remaining non-InnoDB
`INFORMATION_SCHEMA` metadata monolith into ordered owner modules. The public
lookup module keeps only the provider aggregator and case-insensitive lookup
helpers. Extension/prefix tables, core metadata tables, routine/operational
metadata, access/constraint metadata, and trigger/view metadata each own their
static column definitions and table-definition provider. The provider list
preserves the existing `INFORMATION_SCHEMA` table order around the existing
InnoDB provider.

The twenty-third split continues reducing the mutable catalog module by moving
key and constraint mutation ownership into `mylite_catalog_key_constraints.c`.
The new module owns index, index-column, foreign-key, foreign-key-column, and
check-constraint insert/update/delete APIs plus their SQL bind indexes and row
helpers. `mylite_catalog.c` retains table, column, view, schema, and lifecycle
mutation orchestration; it reaches key/constraint cleanup only through a small
private SQLite-level helper surface used by table and schema deletion.

The twenty-fourth split targets the standalone JSON runtime module, which had
become the slowest single clang-tidy translation unit. Public JSON entry points
remain in `mylite_json.c`; private JSON value/parser/writer structures and
helpers move behind `mylite_json_internal.h`. Document parsing, validation,
path lookup, containment checks, mutation application, and DOM/emission helpers
each move into focused runtime modules without changing the public JSON API,
diagnostics, path support, normalization order, or memory ownership rules.

The twenty-fifth split targets scalar JSON mutation execution. JSON_SET,
JSON_INSERT, JSON_REPLACE, and JSON_REMOVE scalar argument evaluation moves out
of `mylite_execution_scalar_json.c` into
`mylite_execution_scalar_json_mutation.c`. A narrow private
`mylite_execution_scalar_json_internal.h` header exposes only the shared scalar
JSON constructor, path, and JSON_EXTRACT finish helpers needed by mutation
value coercion. This split preserves the existing SQL scalar function ABI,
diagnostics, NULL propagation, unsupported-shape behavior, and JSON runtime API
calls.

The twenty-sixth split targets loaded catalog descriptor ownership. The
loaded table-column, primary-key, secondary-index, foreign-key, and CHECK
constraint structures and loader/deinitializer APIs move into
`mylite_execution_loaded_catalog.h/.c`. This removes metadata loading and
index/FK/check lifetime management from the execution translation unit while
leaving column-reference resolution, INSERT planning, DML value coercion, and
row-scalar select-item planning in the existing execution fragments until their
own dependency surfaces are designed.

The twenty-seventh split targets DML numeric string parsing support. The shared
integer-string parser, signed-integer parser, and numeric token scanner used by
INSERT, UPDATE, column defaults, DECIMAL conversion, and approximate numeric
conversion move into `mylite_execution_dml_numeric.h/.c`. DML value conversion,
decimal type metadata, range diagnostics, warnings, and value materialization
remain in execution fragments.

The twenty-eighth split targets scalar base-conversion and binary/encoding
function execution. `BIN()`, `OCT()`, `CONV()`, `BIT_COUNT()`, `CRC32()`,
`HEX()`, `WEIGHT_STRING()`, `UNHEX()`, `TO_BASE64()`, `FROM_BASE64()`,
UUID conversion, `CHAR()`, and shared hex-byte formatting move into
`mylite_execution_scalar_binary.c`. The new module keeps conversion-specific
argument decoding, byte ownership, warning staging, UUID swap handling,
conversion-specific unsupported-error message formatting, and CRC32/hex/base
conversion formatting file-local. Existing scalar arithmetic, bitwise,
temporal, system-variable, binary cast/convert, JSON, shared diagnostic sinks,
and row-scalar unknown-column behavior remain owned by the execution runtime
and are reached through narrow internal scalar helper wrappers. The module
intentionally reads borrowed session scalar identity values for `DATABASE()`,
`USER()`, `CURRENT_USER()`, `CONNECTION_ID()`, row-count, found-rows, and
last-insert-id argument conversion paths; it does not own or mutate session
state.

The twenty-ninth split targets scalar string, regular-expression, and
charset/collation function execution as an initial true-module boundary under
the scalar string module. Core string scalar functions
(`LENGTH()`, `BIT_LENGTH()`, `CHAR_LENGTH()`, `ASCII()`, `ORD()`, `LOWER()`,
`UPPER()`, `TRIM()` variants, `LEFT()`, `RIGHT()`, and `SUBSTRING()`),
extended string functions (`LPAD()`, `RPAD()`, `REPEAT()`, `SPACE()`,
`EXPORT_SET()`, `MAKE_SET()`, `LOCATE()`, `INSTR()`, `POSITION()`,
`CONCAT_WS()`, `REPLACE()`, `INSERT()`, `REVERSE()`, `SOUNDEX()`, `QUOTE()`,
`SUBSTRING_INDEX()`, `FIND_IN_SET()`, and `STRCMP()`) initially stay in
`mylite_execution_scalar_string.c`. Round 32 moves REGEXP scalar functions and
scalar `CHARSET()`, `COLLATION()`, and `COERCIBILITY()` metadata execution into
dedicated scalar modules. Round 33 splits the remaining scalar string module
into core string length/codepoint/case/trim execution, position/search/padding
execution, and string transformation execution. Row-scalar planning, predicate
planning, scalar projection descriptor construction, generic session-scalar
dispatch, table-option validation, shared diagnostics, and session state remain
in the execution runtime and call these modules through explicit internal
scalar helper wrappers.

The thirty-fourth split targets scalar numeric execution. Numeric scalar
families (`/`, bitwise projection rendering, `ABS()`, `SIGN()`,
`CEIL()`/`CEILING()`/`FLOOR()`/`ROUND()`, `SQRT()`, trigonometric functions,
`ATAN()`/`ATAN2()`, `EXP()`/`LN()`/`LOG()`/`LOG10()`/`LOG2()`,
`POW()`/`POWER()`, `FORMAT()`, and `TRUNCATE()`) move into
`mylite_execution_scalar_numeric.c`. The module also owns the shared
locale-stable double formatting helpers used by `RAND()` and catalog numeric
normalization. Generic arithmetic/logical/comparison evaluators, scalar
projection classifiers, warning appenders, diagnostics, generic scalar
dispatch, and session state remain in the execution runtime and are reached
through explicit internal helper declarations.

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
  `INFORMATION_SCHEMA` public-order provider aggregation and table-definition
  lookup accessors.
- `mylite_execution_catalog_information_schema_internal.h`: private provider
  API for information-schema metadata owner modules.
- `mylite_execution_catalog_information_schema_keywords.c`:
  `INFORMATION_SCHEMA.KEYWORDS` rows and keyword lookup/count accessors.
- `mylite_execution_catalog_information_schema_extension_tables.c`:
  extension/prefix `INFORMATION_SCHEMA` descriptors that precede the InnoDB
  provider in public order.
- `mylite_execution_catalog_information_schema_innodb.c`: InnoDB
  `INFORMATION_SCHEMA` column definitions and table-definition provider.
- `mylite_execution_catalog_information_schema_metadata_tables.c`: core
  metadata descriptors in the first post-InnoDB ordered provider block.
- `mylite_execution_catalog_information_schema_routine_tables.c`:
  routine, event, optimizer, partition, plugin, process, and profiling
  metadata descriptors.
- `mylite_execution_catalog_information_schema_access_tables.c`: role,
  privilege, constraint, key, statistic, spatial, resource-group, and
  referential metadata descriptors.
- `mylite_execution_catalog_information_schema_view_tables.c`: trigger,
  user-attribute, view, and view-usage metadata descriptors.
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
  descriptor validation, table/column/view/schema mutation APIs, and SQL
  statements that mutate private catalog rows.
- `mylite_catalog_column_values.c`: column descriptor value structs,
  insert/replace bind helpers, default-kind storage predicates, and
  default/generated-column validation.
- `mylite_catalog_key_constraints.c`: index, index-column, foreign-key,
  foreign-key-column, and check-constraint mutation APIs plus table/schema
  key/constraint cleanup helpers.
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

### `mylite_json`

The JSON runtime module family owns MySQL-compatible JSON document parsing,
normalization, path lookup, containment, mutation, construction, quoting, and
validation. It uses true C modules because the JSON value model is standalone
and has a small private helper surface.

The JSON runtime module family includes:

- `mylite_json.c`: public JSON API entry points, SQL-value conversion for JSON
  array/object constructors, and public error-message text.
- `mylite_json_internal.h`: private JSON value, parser, path, mutation, and
  writer types plus the helper surface shared by JSON runtime modules.
- `mylite_json_dom.c`: JSON value ownership, array/object storage helpers,
  member ordering/lookups, result text copying, parser primitives, and writer
  lifecycle.
- `mylite_json_parse.c`: iterative JSON document parsing and decoded string,
  literal, integer-number, object, and array materialization.
- `mylite_json_validate.c`: allocation-free JSON document validation.
- `mylite_json_path.c`: JSON path lookup parsing, mutable mutation path
  parsing, and path lifetime cleanup.
- `mylite_json_contains.c`: JSON containment comparison traversal.
- `mylite_json_mutation.c`: JSON_SET, JSON_INSERT, JSON_REPLACE, and
  JSON_REMOVE path application. Mutation result emission stays with the public
  JSON API entry points in `mylite_json.c`.

The JSON runtime module family does not own:

- SQL parser or scalar-function dispatch.
- MySQL diagnostic text rendering beyond JSON parser result details.
- SQLite function registration wrappers.

### `mylite_execution_scalar_json`

The scalar JSON execution module family owns SQL AST argument handling for
MySQL-compatible JSON scalar functions and maps those arguments into the
standalone JSON runtime API.

The scalar JSON execution module family includes:

- `mylite_execution_scalar_json.c`: JSON_VALID, JSON_EXTRACT, JSON_VALUE,
  JSON_CONTAINS, JSON_CONTAINS_PATH, JSON_LENGTH, JSON_KEYS, JSON_TYPE,
  JSON_QUOTE, JSON_UNQUOTE, JSON_ARRAY, and JSON_OBJECT scalar entry points
  plus shared constructor, path, argument-list, and result-finish helpers.
- `mylite_execution_scalar_json_mutation.c`: JSON_SET, JSON_INSERT,
  JSON_REPLACE, and JSON_REMOVE scalar entry points, mutation argument-count
  validation, document/path/value coercion, NULL propagation, and mutation
  result diagnostics.
- `mylite_execution_scalar_json_internal.h`: private helper declarations shared
  only inside the scalar JSON execution family.

The scalar JSON execution module family does not own:

- JSON document parsing, normalization, mutation application, or path traversal;
  those remain in the standalone `mylite_json` module family.
- Row-scalar planning and function-dispatch decisions.
- Public client ABI beyond the existing internal runtime scalar declarations.

### `mylite_execution_loaded_catalog`

The loaded catalog execution module owns runtime materialization of table
metadata descriptors used by execution planning, SHOW/metadata queries, and DML
constraint checks.

The loaded catalog execution module includes:

- `mylite_execution_loaded_catalog.h`: internal loaded descriptor structs for
  primary keys, indexes, foreign keys, CHECK constraints, and small spans plus
  loader, deinitializer, column-key, and alter-rejection helper declarations.
- `mylite_execution_loaded_catalog.c`: table column loading, primary-key
  loading, secondary-index loading, foreign-key loading, CHECK constraint
  loading, loaded descriptor cleanup, column-key classification, and simple
  index/check presence rejection helpers.

The loaded catalog execution module does not own:

- Parser, planner, or SQL AST column-reference resolution.
- DML value conversion, default materialization, SQL literal decoding, or UTF-8
  validation.
- SQL statement execution or SQLite write helpers.

### `mylite_execution_dml_numeric`

The DML numeric execution helper module owns MySQL-compatible scanning of
quoted numeric strings used by DML coercion and column default parsing.

The DML numeric module includes:

- `mylite_execution_dml_numeric.h`: parse-result enum, scanned numeric token
  shape, and parser/scanner declarations.
- `mylite_execution_dml_numeric.c`: integer-string parsing with decimal/exponent
  rounding, exact signed-integer parsing, numeric token scanning, whitespace and
  truncation detection, and overflow classification.

The DML numeric module does not own:

- Column descriptor range checks, strict-mode behavior, or MySQL diagnostics.
- DECIMAL type metadata, DECIMAL canonicalization, or approximate-number
  storage conversion.
- INSERT/UPDATE/LOAD DATA row planning or value materialization.

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

- `mylite_execution_statement_entry.inc`: top-level statement entry,
  completion, affected/found-row bookkeeping, and statement dispatch.
- `mylite_execution_statement_session_handlers.inc`: lightweight session
  statement handlers such as `DO`, `RESET`, `FLUSH`, and `KILL`.
- `mylite_execution_prepared_statement_execution.inc`: PREPARE, EXECUTE, and
  DEALLOCATE statement execution entry points.
- `mylite_execution_transaction_characteristics.inc`: transaction access mode
  and isolation-level parsing and session-state mutation.
- `mylite_execution_statement_transaction_boundaries.inc`: statement-level
  implicit transaction boundary helpers.
- `mylite_execution_transaction_statements.inc`: START TRANSACTION, BEGIN,
  COMMIT, ROLLBACK, and related transaction statements.
- `mylite_execution_lock_tables.inc`: LOCK TABLES and UNLOCK TABLES session
  state handling.
- `mylite_execution_statement_implicit_commits.inc`: statement classes that
  require implicit commit behavior.
- `mylite_execution_session_savepoints.inc`: SAVEPOINT, ROLLBACK TO
  SAVEPOINT, and RELEASE SAVEPOINT handling.
- `mylite_execution_statement_sqlite_transactions.inc`: SQLite transaction
  helpers used by statement execution.
- `mylite_execution_set_connection_charset.inc`: SET NAMES and connection
  character-set/collation handling.
- `mylite_execution_set_assignments.inc`: SET assignment dispatch and user
  variable mutation.
- `mylite_execution_prepared_statement_support.inc`: prepared-statement
  metadata, parameter binding, and value conversion support.
- `mylite_execution_set_session_snapshot.inc`: statement-local SET session
  snapshot capture and restore helpers.
- `mylite_execution_set_system_variable_dispatch.inc`: system-variable SET
  validation and setter dispatch.
- `mylite_execution_set_boolean_variables.inc`: boolean system-variable
  setters.
- `mylite_execution_set_numeric_transaction_variables.inc`: numeric and
  transaction-related system-variable setters.
- `mylite_execution_set_limit_size_expiry_variables.inc`: limit, size, packet,
  cache, and expiry system-variable setters.
- `mylite_execution_set_timeout_variables.inc`: timeout system-variable
  setters.
- `mylite_execution_set_sql_mode_timestamp_time_zone.inc`: SQL mode,
  timestamp, and time-zone system-variable setters.
- `mylite_execution_ddl_create_table_statements.inc`: high-level
  `CREATE TABLE`, temporary table, `CREATE TABLE LIKE`, and
  `CREATE TABLE ... SELECT` statement execution.
- `mylite_execution_ddl_create_view_statements.inc`: high-level `CREATE VIEW`
  planning, catalog mutation, and view SQL synthesis.
- `mylite_execution_ddl_create_schema_index_statements.inc`: high-level
  `CREATE INDEX` and `CREATE SCHEMA` entry-point execution.
- `mylite_execution_ddl_drop_existence_statements.inc`: high-level `DROP`
  table/view/schema execution and DDL `IF EXISTS`/`IF NOT EXISTS` completion
  helpers.
- `mylite_execution_ddl_table_action_statements.inc`: `TRUNCATE`, rename,
  `ALTER TABLE ADD COLUMN`, and multi-action ALTER TABLE execution.
- `mylite_execution_ddl_alter_table_index_constraint_statements.inc`:
  ALTER TABLE index, primary key, foreign key, check constraint, and
  `DROP INDEX` execution.
- `mylite_execution_ddl_alter_table_column_statements.inc`: ALTER TABLE column,
  default, and visibility statement execution.
- `mylite_execution_ddl_alter_table_schema_option_statements.inc`: ALTER TABLE
  default charset/collation, `CONVERT TO CHARACTER SET`, comment, and
  ALTER SCHEMA default charset/collation execution.
- `mylite_execution_ddl_alter_table_maintenance_statements.inc`: ALTER TABLE
  `ORDER BY`, `FORCE`, key-maintenance no-op execution, and ALGORITHM/LOCK
  validation.
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
- `mylite_execution_information_schema_columns_system_rows.inc`: static
  INFORMATION_SCHEMA and mysql system table COLUMNS rows plus static
  COLUMNS_EXTENSIONS rows.
- `mylite_execution_information_schema_columns_base_rows.inc`: user-table
  COLUMNS, ST_GEOMETRY_COLUMNS, COLUMNS_EXTENSIONS, VIEWS, VIEW_TABLE_USAGE,
  and column default/display metadata rows.
- `mylite_execution_information_schema_innodb_virtual_rows.inc`: InnoDB
  virtual generated-column dependency rows.
- `mylite_execution_information_schema_innodb_column_rows.inc`: InnoDB column
  rows and InnoDB column type/precision/prtype metadata helpers.
- `mylite_execution_information_schema_innodb_table_rows.inc`: InnoDB table
  and table-statistics rows plus table flag/column-count/page-size helpers.
- `mylite_execution_information_schema_innodb_index_rows.inc`: InnoDB index
  and field rows plus generated clustered-index and index-part helpers.
- `mylite_execution_information_schema_innodb_foreign_rows.inc`: InnoDB
  foreign-key and foreign-key-column rows plus foreign identifier/action
  helpers.
- `mylite_execution_information_schema_constraint_rows.inc`: table
  constraints, table-constraint extensions, and check-constraint rows.
- `mylite_execution_information_schema_key_constraint_rows.inc`: key column
  usage and referential constraint rows.
- `mylite_execution_information_schema_statistics_rows.inc`: mysql system
  table and user-table STATISTICS rows.
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
- `mylite_execution_grouped_aggregate_entry.inc`: GROUP BY detection,
  grouped aggregate planning orchestration, planned aggregate cleanup, and
  optional-clause collection.
- `mylite_execution_grouped_aggregate_source_planning.inc`: grouped aggregate
  table/join source planning and grouped projection reference validation.
- `mylite_execution_grouped_aggregate_group_columns.inc`: GROUP BY key
  resolution, SELECT-alias group-key resolution, group-key counting, and
  group-column validation.
- `mylite_execution_grouped_aggregate_projection_columns.inc`: grouped
  projection planning, wildcard expansion, visible-column collection, and
  grouped projection validity checks.
- `mylite_execution_grouped_aggregate_function_planning.inc`: grouped
  aggregate item planning, aggregate column resolution, GROUP_CONCAT option
  planning, aggregate function classification, grouped key/projection lookup,
  and source-resolution lookup.
- `mylite_execution_grouped_aggregate_having_planning.inc`: grouped HAVING
  predicate planning, operand resolution, aggregate operand planning, and
  HAVING literal conversion entry points.
- `mylite_execution_grouped_aggregate_literal_conversion.inc`: grouped HAVING
  date, time, datetime, timestamp, YEAR, and integer literal conversion
  helpers.
- `mylite_execution_grouped_aggregate_order_planning.inc`: grouped ORDER BY
  planning, item-list handling, alias lookup, aggregate alias lookup, and
  unaliased grouped/projection column matching.
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
- `mylite_execution_scalar_string_core.inc`: tombstone include; scalar string
  core support lives in `mylite_execution_scalar_string.c`.
- `mylite_execution_scalar_temporal_core.inc`: tombstone include; scalar
  temporal core support lives in `mylite_execution_scalar_temporal.c`.
- `mylite_execution_scalar_string_extended.inc`: tombstone include; scalar
  string position/search/padding support lives in
  `mylite_execution_scalar_string_position.c`, scalar string transformation
  support lives in `mylite_execution_scalar_string_transform.c`, REGEXP support
  lives in `mylite_execution_scalar_regexp.c`, and charset/collation/coercibility
  support lives in `mylite_execution_scalar_charset_collation.c`.
- `mylite_execution_scalar_misc.inc`: scalar subquery, concatenation,
  `ELT`, `FIELD`, `GREATEST`, `LEAST`, and `INTERVAL` scalar support.
- `mylite_execution_scalar_numeric.inc`: removed; scalar division, bitwise
  result formatting, numeric scalar functions, approximate math, double-format,
  `FORMAT`, and `TRUNCATE` scalar support lives in
  `mylite_execution_scalar_numeric.c` with internal declarations in
  `mylite_execution_scalar_numeric.h`.
- `mylite_execution_scalar_conversion.inc`: scalar `CAST`, `CONVERT`, and
  `COLLATE` support.
- `mylite_execution_scalar_temporal_format.inc`: tombstone include; scalar
  temporal formatting, interval, and time arithmetic support lives in
  `mylite_execution_scalar_temporal_format.c` with internal declarations in
  `mylite_execution_scalar_temporal_format.h`.
- `mylite_execution_scalar_expression_eval.inc`: scalar arithmetic, logical,
  comparison, and bitwise expression evaluators.
- `mylite_execution_scalar_control.inc`: IF/CASE scalar control flow,
  literal projection, system-variable scalar values, and control-flow
  validation.
- `mylite_execution_scalar_projection.inc`: scalar projection classifiers,
  parenthesis unwrapping, and source-span copying helpers.
- `mylite_execution_delete_planning.inc`: DELETE planning and execution helpers
  that were previously housed in the scalar fragment.
- `mylite_execution_column_plan_entry.inc`: column planning entry,
  column-level attribute validation, generated-column attributes,
  charset/collation handling, and binary string normalization.
- `mylite_execution_column_default_finalization.inc`: column default
  validation and typed default materialization.
- `mylite_execution_column_default_text.inc`: default expression
  classification, default text rendering, and expression default conversion
  helpers.
- `mylite_execution_column_default_integer_eval.inc`: integer default literal
  conversion and expression evaluation.
- `mylite_execution_column_type_mapping.inc`: SQL AST column type mapping for
  text, binary, ENUM/SET, bit, year, decimal, approximate, and integer types.
- `mylite_execution_column_type_predicates.inc`: logical/planned/catalog
  descriptor type predicates.
- `mylite_execution_column_descriptor_parsing.inc`: logical descriptor parsing
  for ENUM/SET, decimal, approximate, text, binary, and bit metadata.
- `mylite_execution_column_row_size_validation.inc`: planned/catalog row-size
  validation and row-size contribution accounting.
- `mylite_execution_column_key_modify_validation.inc`: string-key validation
  and MODIFY/CHANGE COLUMN replacement compatibility predicates.
- `mylite_execution_descriptor_helpers.inc`: loaded descriptor lookup,
  descriptor cleanup, and shared descriptor-copy helpers.
- `mylite_execution_insert_row_planning.inc`: INSERT/REPLACE row planning,
  target-column mapping, and planned-row cleanup helpers.
- `mylite_execution_insert_value_conversion.inc`: INSERT/REPLACE planned-value
  conversion dispatch and type-family conversion entry points.
- `mylite_execution_dml_default_values.inc`: DML default value
  materialization, AUTO_INCREMENT handling, and generated/default timestamp
  helpers.
- `mylite_execution_dml_integer_conversion.inc`: DML integer value conversion.
- `mylite_execution_dml_enum_set_conversion.inc`: DML ENUM and SET value
  conversion.
- `mylite_execution_dml_string_binary_conversion.inc`: DML text, binary,
  BIT, JSON, and spatial value conversion.
- `mylite_execution_dml_decimal_approx_conversion.inc`: DML DECIMAL, FLOAT,
  DOUBLE, and approximate numeric conversion.
- `mylite_execution_dml_temporal_defaults.inc`: temporal DML default
  materialization helpers.
- `mylite_execution_dml_value_helpers.inc`: planned-value initialization,
  copying, cleanup, and null/text/blob helpers.
- `mylite_execution_dml_string_validation.inc`: DML string length, UTF-8,
  CHAR/VARCHAR/TEXT/BINARY validation, and key-value validation helpers.
- `mylite_execution_dml_implicit_values.inc`: implicit DML value assignment
  helpers.
- `mylite_execution_row_scalar_select_items.inc`: row-scalar select-item
  append and cleanup helpers shared by DML and SELECT planning.
- `mylite_execution_query_planning.inc`: row-scalar planning core, special
  expression dispatch, window functions, integer arithmetic planning, temporal
  dispatch, and JSON dispatch.
- `mylite_execution_row_scalar_string_basic_planning.inc`: row-scalar string
  length, codepoint, and case function planning.
- `mylite_execution_row_scalar_string_shape_planning.inc`: row-scalar string
  trim, slice, and padding function planning.
- `mylite_execution_row_scalar_string_bitmask_search_planning.inc`:
  row-scalar string bitmask and search function planning.
- `mylite_execution_row_scalar_string_edit_planning.inc`: row-scalar
  `REPLACE` and `INSERT` string function planning.
- `mylite_execution_row_scalar_string_transform_planning.inc`: row-scalar
  `REVERSE`, `SOUNDEX`, and `QUOTE` planning.
- `mylite_execution_row_scalar_string_compare_set_planning.inc`: row-scalar
  `SUBSTRING_INDEX`, `FIND_IN_SET`, and `STRCMP` planning.
- `mylite_execution_row_scalar_string_regexp_planning.inc`: row-scalar
  REGEXP predicate/string function planning.
- `mylite_execution_row_scalar_json_planning.inc`: row-scalar JSON function and
  JSON operator planning.
- `mylite_execution_row_scalar_binary_value_planning.inc`: row-scalar HEX,
  UNHEX, Base64, UUID conversion, and binary-string dispatcher planning.
- `mylite_execution_row_scalar_char_charset_planning.inc`: row-scalar `CHAR`,
  `CHARSET`, `COLLATION`, and `COERCIBILITY` planning.
- `mylite_execution_row_scalar_control_flow_planning.inc`: row-scalar
  top-level and nested IF, IFNULL, COALESCE, NULLIF, and ISNULL planning.
- `mylite_execution_row_scalar_conversion_value_planning.inc`: row-scalar
  non-concat dispatcher, CAST/CONVERT, literal, session value, and descriptor
  column planning.
- `mylite_execution_row_scalar_concat_planning.inc`: row-scalar CONCAT,
  `||`, and CONCAT_WS planning.
- `mylite_execution_row_scalar_temporal_format_planning.inc`: row-scalar
  `DATE_FORMAT`, `TIME_FORMAT`, `GET_FORMAT`, `STR_TO_DATE`, and DATE_FORMAT
  numeric equality planning.
- `mylite_execution_row_scalar_temporal_interval_extract_planning.inc`:
  row-scalar interval-second arithmetic and temporal extraction planning.
- `mylite_execution_row_scalar_temporal_conversion_planning.inc`: row-scalar
  `SEC_TO_TIME`, `FROM_UNIXTIME`, and temporal constructor planning.
- `mylite_execution_row_scalar_temporal_period_timezone_weight_planning.inc`:
  row-scalar period, time zone, and `WEIGHT_STRING` planning.
- `mylite_execution_row_scalar_temporal_diff_planning.inc`: row-scalar
  `DATEDIFF`, `TIMEDIFF`, and `TIMESTAMPDIFF` planning.
- `mylite_execution_row_scalar_temporal_timestamp_planning.inc`: row-scalar
  `UNIX_TIMESTAMP` and `TIMESTAMP` planning.
- `mylite_execution_row_scalar_misc_planning.inc`: row-scalar `FIELD`,
  `GREATEST`, `LEAST`, `INTERVAL`, row-scalar cleanup, descriptor checks, and
  expression containment helpers.
- `mylite_execution_select_column_planning.inc`: SELECT result-column planning
  and result metadata descriptor population.
- `mylite_execution_select_predicate_entry.inc`: SELECT predicate planning
  entry points, predicate cleanup, iterative work-stack planning, logical
  predicate finishing, and top-level predicate dispatch.
- `mylite_execution_select_predicate_leaf_comparison.inc`: predicate leaf
  dispatch, column/literal comparison planning, scalar literal truth planning,
  comparison helpers, and scalar predicate classifiers.
- `mylite_execution_select_predicate_temporal_extract.inc`: DATE_FORMAT
  numeric predicate planning and temporal extractor predicate planning.
- `mylite_execution_select_predicate_string_functions.inc`: string-length,
  substring, and FIND_IN_SET predicate planning.
- `mylite_execution_select_predicate_json_regexp_functions.inc`: REGEXP_LIKE,
  JSON_VALID, and JSON_CONTAINS predicate planning.
- `mylite_execution_select_predicate_subquery_correlation.inc`: comparison
  column validation, EXISTS subquery planning, correlated column resolution,
  and comparison operator classification.
- `mylite_execution_select_predicate_special_in.inc`: IS NULL, boolean,
  BETWEEN, IN list, IN subquery predicate planning, and IN-list value
  conversion.
- `mylite_execution_select_predicate_work_helpers.inc`: predicate node append,
  work-stack append/pop, result-index, and column-source resolution helpers.
- `mylite_execution_select_predicate_value_conversion.inc`: integer, ENUM,
  SET, YEAR, string, string-pattern, and REGEXP-pattern predicate literal
  conversion.
- `mylite_execution_select_predicate_temporal_literals.inc`: date, time,
  datetime, timestamp, and temporal-offset predicate literal conversion and
  normalization.
- `mylite_execution_select_order_planning.inc`: SELECT ORDER BY, SELECT LIMIT,
  and DELETE LIMIT planning helpers.
- `mylite_execution_update_planning_helpers.inc`: UPDATE assignment, UPDATE
  value conversion, UPDATE validation, and UPDATE LIMIT planning helpers.
- `mylite_execution_show_tables_helpers.inc`: `SHOW TABLES` row append and
  output-column WHERE predicate evaluation helpers.
- `mylite_execution_show_table_status_rows_helpers.inc`: `SHOW TABLE STATUS`
  row append and built-in table status timestamp helpers.
- `mylite_execution_show_table_status_where_helpers.inc`: `SHOW TABLE STATUS`
  output-column WHERE predicate evaluation and comparison helpers.
- `mylite_execution_show_columns_helpers.inc`: `SHOW COLUMNS`/`SHOW FULL
  COLUMNS` row append and output-column WHERE predicate helpers.
- `mylite_execution_show_index_rows_helpers.inc`: `SHOW INDEX` row append and
  index-part display helpers.
- `mylite_execution_show_index_where_helpers.inc`: `SHOW INDEX` output-column
  WHERE predicate evaluation, shared SHOW metadata REGEXP matching, and index
  comparison helpers.
- `mylite_execution_show_column_display_helpers.inc`: shared `SHOW COLUMNS`
  type and collation display helpers.
- `mylite_execution_show_databases_helpers.inc`: `SHOW DATABASES` collection,
  sorting, deduplication, and output-column WHERE predicate helpers.
- `mylite_execution_show_filter_helpers.inc`: shared `SHOW LIKE` filter
  construction, deinit, and matching helpers.
- `mylite_execution_show_result_name_helpers.inc`: `SHOW DATABASES` and `SHOW
  TABLES` dynamic result column-name builders.
- `mylite_execution_show_table_status_count_helpers.inc`: shared table-status
  row-count SQL and integer-format helpers.
- `mylite_execution_show_like_pattern_helpers.inc`: `SHOW LIKE` pattern
  decoding and ASCII pattern matching helpers.
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
- `mylite_execution_diagnostics.h/.c`: MySQL-compatible diagnostics, warnings,
  notes, and parse-status mapping helpers, with short-name aliases for
  remaining include-based runtime fragments.

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

### `mylite_execution_scalar_string`

The scalar string core module owns session-scalar implementations for:

- `LENGTH`
- `OCTET_LENGTH`
- `BIT_LENGTH`
- `CHAR_LENGTH`
- `CHARACTER_LENGTH`
- `ASCII`
- `ORD`
- `LOWER`
- `LCASE`
- `UPPER`
- `UCASE`
- `TRIM`, `LTRIM`, and `RTRIM`

The module also owns the shared session-scalar text conversion helper used by
the scalar string family for borrowed session values such as schema, user,
connection id, version, row counts, date/time values, and system variables.

The module must not own string slice, padding, bitmask, search, transform,
REGEXP, charset/collation, row-scalar planning, predicate SQL rendering, or
generic scalar dispatch.

### `mylite_execution_scalar_string_position`

The scalar string position/search module owns session-scalar implementations for:

- `LEFT`
- `RIGHT`
- `SUBSTRING`, `SUBSTR`, and `MID`
- `LPAD`
- `RPAD`
- `REPEAT`
- `SPACE`
- `EXPORT_SET`
- `MAKE_SET`
- `LOCATE`
- `INSTR`
- `POSITION`
- `FIND_IN_SET`
- `STRCMP`

The module owns the string slice, padding, bitmask, and search function-kind
enums and planner helper declarations in
`mylite_execution_scalar_string_position.h`. It may call scalar string core
helpers for admitted scalar text arguments and borrowed session-scalar text
conversion.

The module must not own string transform functions, REGEXP, charset/collation,
row-scalar planning, predicate SQL rendering, generic scalar dispatch, or
session-state mutation.

### `mylite_execution_scalar_string_transform`

The scalar string transform module owns session-scalar implementations for:

- `CONCAT_WS`
- `REPLACE`
- string `INSERT`
- `REVERSE`
- `SOUNDEX`
- `QUOTE`
- `SUBSTRING_INDEX`

The module owns transform-specific argument classification helpers in
`mylite_execution_scalar_string_transform.h`. It may call scalar string core
helpers for scalar text conversion and scalar string position helpers for
signed integer and NULL slice/count argument parsing.

The module must not own core string length/codepoint/case/trim execution,
position/search/padding/bitmask functions, REGEXP, charset/collation,
row-scalar planning, predicate SQL rendering, generic scalar dispatch, or
session-state mutation.

### `mylite_execution_scalar_regexp`

The scalar REGEXP module owns session-scalar implementations for:

- `REGEXP_LIKE`
- `REGEXP_INSTR`
- `REGEXP_SUBSTR`
- `REGEXP_REPLACE`

The module also exposes narrow internal helpers for row-scalar REGEXP planning:
REGEXP string function classification, argument-list normalization,
match-type decoding, pattern validation, and literal text normalization.

The module may call scalar string position helpers for admitted scalar text
arguments and scalar string core helpers for session scalar argument
evaluation. It must not own ordinary string scalar functions, row-scalar
planning, predicate SQL rendering, generic scalar dispatch, or session state.

### `mylite_execution_scalar_charset_collation`

The scalar charset/collation module owns session-scalar implementations and
metadata helpers for:

- `CHARSET`
- `COLLATION`
- `COERCIBILITY`
- scalar expression charset/collation metadata used by `COLLATE`
- scalar collation lookup for planning and result metadata helpers.

The module may call shared scalar helpers for AST traversal, binary/charset
conversion execution, `COLLATE` execution, session scalar cleanup, and
MySQL-compatible diagnostics. It must not own row-scalar value planning,
column descriptor resolution, table charset/collation option handling, generic
scalar dispatch, or session system-variable state.

### `mylite_execution_scalar_numeric`

The scalar numeric module owns session-scalar implementations for:

- top-level `/` scalar division
- bitwise scalar projection result formatting
- `ABS`
- `SIGN`
- `CEIL`, `CEILING`, `FLOOR`, and `ROUND`
- `SQRT`
- `DEGREES` and `RADIANS`
- `ACOS` and `ASIN`
- `SIN`, `COS`, `TAN`, and `COT`
- `ATAN` and `ATAN2`
- `EXP`, `LN`, `LOG`, `LOG10`, `LOG2`, `POW`, and `POWER`
- `FORMAT`
- `TRUNCATE`

It also owns reusable locale-stable double parsing/formatting helpers used by
`RAND()` scalar execution and catalog numeric normalization. The module may
call shared scalar helpers for AST traversal, scalar projection classification,
arithmetic/bitwise operand evaluation, warning count accumulation, warning
emission, literal normalization, and MySQL-compatible diagnostics.

The module must not own generic scalar dispatch, scalar arithmetic/logical
expression evaluators, row-scalar planning, predicate SQL rendering, catalog
loading policy, system-variable state, or session-state mutation.

### `mylite_execution_diagnostics`

The execution diagnostics module owns MySQL-compatible runtime diagnostics for
the execution layer:

- MySQL numeric error, warning, and note codes used by execution diagnostics
- formatted error, warning, and note messages for runtime, DDL, DML, scalar,
  parser-status, and metadata paths
- conversion from parser status codes to MyLite public status codes
- small diagnostic-message helpers for system-variable source spans and
  multi-table DROP diagnostics.

The module exposes prefixed internal functions in
`mylite_execution_diagnostics.h`. While `mylite_execution.c` still includes
runtime fragments, the header may provide short-name aliases for the existing
fragment call sites; those aliases must stay local to internal runtime code and
must not become public ABI. The module may depend on shared planning/diagnostic
types in `mylite_execution_plan_types.h` and shared MySQL diagnostic constants
in `mylite_mysql_error_codes.h`.

The module must not own statement execution, planner decisions, catalog
mutation, SQL mode storage, warning-count staging, or scalar/result value
evaluation. It formats diagnostics from already-classified runtime facts.

### `mylite_execution_scalar_temporal`

The scalar temporal module owns session-scalar implementations for the temporal
function families whose execution can be isolated from row-scalar SQL
rendering:

- `UNIX_TIMESTAMP`
- `TIMESTAMP`
- `DATEDIFF`
- `TIMESTAMPDIFF`
- `TIMEDIFF`
- calendar extraction functions, including `EXTRACT`
- `SEC_TO_TIME`
- `FROM_UNIXTIME`
- `FROM_DAYS`, `MAKEDATE`, and `MAKETIME`
- `PERIOD_ADD` and `PERIOD_DIFF`
- `CONVERT_TZ`

The module may call a small internal execution helper surface for AST child
access, parenthesis unwrapping, literal decoding, identifier copying, current
timestamp epoch lookup, MySQL-compatible diagnostics, and temporal constructor
function metadata shared with row-scalar planning.

The module also exposes narrow internal helpers for planner code that already
shared temporal execution classification: temporal extract call resolution,
temporal extract kind detection, `TIMESTAMPDIFF` unit parsing, and
`FROM_UNIXTIME` integer-literal parsing.

The module must not own row-scalar temporal planning, predicate planning,
date-format execution, date-interval execution, time arithmetic, generic scalar
dispatch, SQL mode storage, session timestamp state, or loaded time-zone table
data. `DATE_FORMAT`, `GET_FORMAT`, `TIME_FORMAT`, `STR_TO_DATE`,
`DATE_ADD`/`DATE_SUB`, `ADDDATE`/`SUBDATE`, `TIMESTAMPADD`, `ADDTIME`, and
`SUBTIME` belong to `mylite_execution_scalar_temporal_format`.

### `mylite_temporal_arithmetic`

The temporal arithmetic helper module owns reusable, diagnostic-free calendar
arithmetic primitives used by temporal scalar execution and temporal predicate
normalization:

- canonical `YYYY-MM-DD` and `YYYY-MM-DD HH:MM:SS` parsing into date/time
  parts.
- checked signed 64-bit addition and multiplication with success/failure
  polarity suitable for temporal range checks.
- calendar-month addition with MySQL-compatible end-of-month clipping for the
  currently supported range.
- conversion between civil date parts and day counts, including floor
  day/second division.

The module must not own MySQL diagnostics, SQL AST traversal, function
argument parsing, session state, or result formatting. Callers keep MySQL
compatibility policy and use this module only for pure arithmetic.

### `mylite_execution_scalar_temporal_format`

The scalar temporal format module owns session-scalar implementations for:

- `DATE_FORMAT`
- `GET_FORMAT`
- `TIME_FORMAT`
- `STR_TO_DATE`
- `DATE_ADD` and `DATE_SUB`
- `ADDDATE` and `SUBDATE`
- `TIMESTAMPADD`
- `ADDTIME` and `SUBTIME`
- the DATE_FORMAT numeric-comparison special case used by scalar projection
  planning.

The module may call the shared internal execution helper surface for AST child
access, parenthesis unwrapping, literal decoding, identifier copying,
MySQL-compatible diagnostics, and scalar cell cleanup. It may call
`mylite_temporal_arithmetic` for pure calendar math. It also exposes narrow
internal helpers for planner code that already shared temporal-format
classification: STR_TO_DATE NULL/identifier child checks, DATE_FORMAT
numeric-comparison side detection and numeric-format admission, date-interval
function shape/unit parsing, and ADDTIME/SUBTIME kind detection. Those helpers
belong in `mylite_execution_scalar_temporal_format.h`; the catch-all scalar
header should not grow this planner-specific temporal-format surface.

The module must not own row-scalar temporal planning, predicate SQL rendering,
generic scalar dispatch, SQL mode storage, loaded time-zone table data, or
current timestamp/session state.

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
- DML numeric string parsing must continue to return the same OK, invalid,
  truncated, and overflow classifications so INSERT, UPDATE, LOAD DATA, and
  default-expression diagnostics remain unchanged.
- Scalar temporal function result values, NULL propagation, unsupported-shape
  diagnostics, parameter-count errors, and planner classification behavior must
  remain identical to the previous scalar fragment behavior.
- Scalar temporal formatting, date-interval, and time arithmetic function
  values, NULL propagation, unsupported-shape diagnostics, and planner helper
  behavior must remain identical to the previous scalar temporal format
  fragment behavior.
- Scalar string core, position/search/padding, transform, REGEXP,
  charset/collation, and coercibility function values, NULL propagation,
  unsupported-shape diagnostics, planner helper behavior, and metadata
  classification must remain identical to the previous scalar string module
  behavior.
- Scalar numeric result values, NULL propagation, warning staging,
  unsupported-shape diagnostics, approximate double formatting, and
  catalog-loading numeric normalization must remain identical to the previous
  scalar numeric fragment behavior.
- Runtime diagnostics must preserve existing MySQL error/warning/note numeric
  codes, SQLSTATEs, formatted messages, warning ordering, parser-status
  translation, and MyLite public status-code returns.
- DML descriptor helper and reference resolution, insert row planning, value
  conversion, default materialization, implicit value creation, and row-scalar
  select-item planning must remain in the original call order and preserve all
  previous MySQL-compatible values, warnings, errors, SQLSTATEs, and ownership
  rules.
- Statement dispatch, prepared statements, transaction/savepoint/lock control,
  connection character-set handling, `SET` assignment application, session
  snapshotting, and system-variable setters must remain behavior-preserving
  private execution fragments with no new exported runtime surface.

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
11. Extract execution diagnostics into a true C module, keeping short-name
    aliases only as a temporary compatibility bridge for remaining include-based
    runtime fragments.
12. Split the misnamed catalog-loading implementation fragment into cohesive
    same-translation-unit descriptor-helper, INSERT planning, DML conversion,
    default, validation, implicit-value, and row-scalar select-item fragments
    without exporting a new helper surface.
13. Split the oversized statement-core implementation fragment into ordered
    same-translation-unit statement entry/completion, session handlers,
    prepared-statement execution and support, transaction characteristics,
    statement transaction boundaries, transaction statements, lock handling,
    statement implicit commits, session savepoints, SQLite transaction helpers,
    connection character-set handling, SET assignment, session-snapshot,
    system-variable dispatch, and focused system-variable setter fragments.
14. Split the oversized column-planning implementation fragment into ordered
    same-translation-unit column entry/attribute, default finalization, default
    text, default integer evaluation, type mapping, type predicates, descriptor
    parsing, row-size validation, and key/modify validation fragments.
15. Split the oversized SELECT predicate planning implementation fragment into
    ordered same-translation-unit predicate entry/work, leaf comparison,
    temporal extractor, string function, JSON/REGEXP function, subquery
    correlation, special predicate/IN, work-helper, value-conversion, and
    temporal literal-conversion fragments.
16. Split the oversized grouped aggregate planning implementation fragment into
    ordered same-translation-unit entry, source, group-column, projection,
    aggregate-function, HAVING, literal-conversion, and ORDER BY fragments.
17. Split the oversized INFORMATION_SCHEMA column/constraint/statistics row
    synthesis fragment into ordered same-translation-unit COLUMNS, InnoDB
    virtual/column/table/index/foreign, constraint, key/referential, and
    statistics row fragments.
18. Split the oversized row-scalar string planning fragment into ordered
    same-translation-unit basic string, shape, bitmask/search, edit,
    transform/quote, compare/set, and REGEXP planning fragments.
19. Split the oversized row-scalar temporal planning fragment into ordered
    same-translation-unit format, interval/extract, conversion, period/time
    zone/weight, diff, and timestamp planning fragments.
20. Split the oversized DDL statement execution fragment into ordered
    same-translation-unit create-table, create-view, create-schema/index,
    drop/existence, table-action, index/constraint, column, schema/table-option,
    and table-maintenance fragments.
21. Split the oversized SHOW helper fragment into ordered
    same-translation-unit table, table-status row, table-status WHERE, column,
    index-row, index-WHERE, column-display, database, filter, result-name,
    table-status-count, and LIKE pattern helper fragments.
22. Split the oversized row-scalar value planning fragment into ordered
    same-translation-unit binary/encoding/UUID, CHAR/charset, control-flow,
    conversion/value/column, and concat planning fragments.

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
- Shared temporal arithmetic helpers remain pure and do not emit diagnostics,
  allocate result cells, or inspect SQL AST nodes.
- Diagnostics helpers remain a formatting/reporting service; planner and
  execution modules decide which diagnostic applies.
- Internal diagnostic symbols use prefixed names even when the remaining
  include-based runtime fragments still use short-name aliases.
- The old catalog-loading bucket no longer hides unrelated DML conversion and
  row-scalar planning code; new fragments are named by behavior and remain
  private includes until narrower true-module boundaries are designed.
- Statement-core fragments preserve the central dispatch and session mutation
  ownership in `mylite_execution.c`; the split must not create a second
  dispatcher, duplicate session state, or export transaction/system-variable
  mutation helpers.
- Column-planning fragments keep column descriptor planning and validation in
  the execution translation unit; the split must not introduce separate schema
  mutation paths, duplicate type descriptors, or materialize catalog metadata
  outside the existing planner/runtime ownership.
- SELECT predicate fragments preserve the existing planner ownership and
  iterative work-stack behavior; the split must not introduce a second
  predicate evaluator, change predicate conversion rules, or materialize
  subquery results outside the existing planned predicate structures.
- Grouped aggregate fragments preserve the existing GROUP BY planner ownership
  and same-translation-unit helper linkage; the split must not change GROUP
  BY/HAVING/ORDER BY compatibility behavior or export grouped temporal literal
  conversion as a separate API.
- INFORMATION_SCHEMA row-family fragments preserve the existing row-set
  ownership and same-translation-unit helper linkage; the split must not
  change MySQL 8.4-shaped column, InnoDB, constraint, key usage, referential
  constraint, or statistics metadata.
- Row-scalar string planning fragments preserve the existing planner
  ownership and same-translation-unit helper linkage; the split must not
  change accepted row-scalar string argument families, column diagnostics,
  unsupported-function messages, or REGEXP pattern validation behavior.
- Row-scalar temporal planning fragments preserve the existing planner
  ownership and same-translation-unit helper linkage; the split must not
  change temporal formatting, interval arithmetic, extraction, conversion,
  period/time zone, difference, UNIX timestamp, or timestamp diagnostics.
- DDL statement fragments preserve the existing statement execution ownership
  in `mylite_execution.c`; the split must not introduce separate schema
  mutation paths, duplicate DDL completion/result handling, or change implicit
  commit, temporary-table, existence, ALTER TABLE, or warning behavior.
- SHOW helper fragments preserve the existing SHOW statement ownership,
  output-column predicate evaluation, LIKE/REGEXP behavior, table-status
  counting, and display formatting; the split must not introduce a second
  metadata query engine, materialize catalog data, or change sorting,
  deduplication, or MySQL-shaped result metadata.
- Row-scalar value planning fragments preserve the existing planner ownership
  and same-translation-unit helper linkage; the split must not change accepted
  binary, UUID, CHAR, charset/collation, control-flow, conversion, literal,
  column, CONCAT, or CONCAT_WS argument families or diagnostics.
