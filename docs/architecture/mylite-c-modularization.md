# `mylite.c` Modularization

`packages/libmylite/src/mylite.c` has become the runtime integration point for
connection state, statement plans, catalog SQL, DDL, DML, SELECT execution,
metadata, diagnostics, and utility code. The direction is correct for MySQL
compatibility, but the implementation needs smaller private modules before more
runtime surface is added.

This refactor must preserve behavior. Move code behind private interfaces first,
then split statement families one at a time with the existing test suite green
after every step.

## Architecture Assessment

The current direction is right: the recent commits are extracting runtime
ownership by concern, preserving the public ABI, and keeping behavior stable.
That is the correct migration path for a compatibility layer because large
rewrites would make MySQL behavior regressions hard to isolate.

The main remaining risk is not line count by itself. The main risk is that
`mylite_runtime.h` is becoming a broad shared object-model header while
`mylite.c` still owns prepare dispatch, execution dispatch, statement-family
planning, and most runtime semantics. That is acceptable during extraction, but
future moves should avoid turning private headers into a second monolith.

The next architecture step should keep this layering:

1. Diagnostics and connection state stay at the bottom.
2. Catalog owns persisted `__mylite_*` metadata access and physical object
   naming.
3. Statement owns public statement lifetime, row/result access, and eventually
   prepare/step dispatch.
4. Statement-family modules own their plan copy, validation, execution, and
   cleanup.
5. SELECT and expression-heavy code move last, after metadata inference and
   reusable catalog loaders have clear homes.

Good next extraction order:

1. Split result metadata and descriptor inference into focused metadata and
   expression-descriptor modules.
2. Move SELECT output planning, DISTINCT validation, grouping validation, and
   clause binding into focused SELECT planning modules.
3. Move UNION preparation and execution into a SELECT/UNION module once result
   metadata attachment has a narrow API.
4. Move table SELECT row materialization in slices: row copying, sort/limit,
   distinct, aggregate state, then join/outer-join execution.
5. Move scalar SELECT and subquery evaluation after SELECT entry points and
   expression callbacks are module-owned.
6. Move `mylite_prepare()` and `mylite_statement_execute_custom()` dispatch into
   `mylite_statement` once statement-family modules own their prepare and
   execution entry points.

## Runtime Header Guardrails

`mylite_runtime.h` is a transitional shared object-model header. It is already
large enough that moving code out of `mylite.c` without splitting type ownership
would only move the monolith from implementation to object model.

Keep `mylite_runtime.h` limited to:

- `struct mylite_db`
- `struct mylite_stmt`
- `enum mylite_stmt_kind`
- tiny cross-runtime primitives needed by those objects

Move feature-owned structs into focused internal headers as ownership becomes
clear:

- `mylite_error_codes.h` for MySQL-compatible condition codes.
- `mylite_field_descriptor.h` for field descriptors and metadata lengths.
- `mylite_schema_types.h` for schema option/default/presence structs.
- `mylite_table_ddl_types.h` for create/drop/rename/truncate/alter/index DDL
  plans.
- `mylite_dml_types.h` for insert/update/delete plans and execution structs.
- `mylite_select_types.h` for SELECT plans, joins, rows, groups, aggregates,
  and subquery scan state.
- `mylite_metadata_types.h` for result metadata structs.
- `mylite_transaction_types.h` for transaction/savepoint/atomicity structs.
- `mylite_show_types.h` for `SHOW` query and target structs.

Rules for future moves:

- If a module exposes `*_deinit`, that module should own the struct definition.
- Do not add new statement-family plan/result structs to `mylite_runtime.h`
  unless a move is temporarily blocked.
- Prefer forward declarations where pointers are enough.
- Include `mylite_runtime.h` only when complete `mylite_db` or `mylite_stmt`
  layout is required.
- Avoid `sqlite3.h` in broad headers unless a struct contains SQLite types by
  value; pointer users can forward declare.
- If a module needs one field from `mylite_db` or `mylite_stmt`, prefer a narrow
  helper/accessor over exposing another broad struct dependency.
- If a module starts mutating another module's plan internals, stop and move
  that plan type to the owning module before moving more behavior.
- Headers should form a one-way dependency graph: diagnostics and connection
  stay low, catalog sits above them, statement dispatch depends on
  statement-family modules, and statement-family modules depend on catalog,
  metadata, diagnostics, and span helpers as needed.
- Track progress by ownership clarity, not only by reducing `mylite.c` line
  count.
- After every extraction, check whether declarations in `mylite_runtime.h` can
  move into a narrower type header.

## Current `mylite.c` Map

`mylite.c` is now about 19k lines after the initial type, diagnostics,
connection, catalog, SHOW/information-schema, DDL, transaction, DML, SELECT
planning, and ALTER/SELECT helper slices. The remaining major regions are:

- Lines 1-53: includes and small process-wide constants. Split only when a
  concrete owner needs each constant.
- Lines 54-1709: file-local prototype wall. Treat this as a symptom, not a
  module. It should shrink naturally as statement families move.
- Lines 1710-2384: public `mylite_prepare()`, parsed statement dispatch,
  SQLite fallback translation, and family prepare wrappers. Move to
  `mylite_statement` after family-owned prepare entry points are stable.
- Lines 2385-3084: table SELECT, scalar SELECT, and UNION preparation. Move
  after SELECT planning, scalar-select, and union boundaries are narrower than
  the current implementation.
- Lines 3085-7103: result metadata attachment, descriptor inference, function
  descriptor inference, catalog-column descriptor loading, and scalar/text
  helper predicates. Extract metadata inference before larger SELECT runtime
  moves.
- Lines 7104-11148: SELECT output expansion, predicate binding,
  grouping/order validation, reference resolution, and subquery validation.
  Move into focused SELECT planning modules instead of one broad select
  runtime.
- Lines 11149-11352: table SELECT expression clone/remap and aggregate binding
  collection. Move with SELECT prepared-statement ownership.
- Lines 11353-11592: custom statement allocation plus `mylite_statement_execute_custom()`
  dispatch. Move allocation/dispatch to `mylite_statement` after every
  statement family exposes narrow prepare and execute APIs.
- Lines 11593-12734: scalar SELECT execution, session functions, `STRCMP()`,
  charset/collation/coercibility evaluation, and collation inference. Split
  into session, string, and collation modules before moving larger SELECT
  execution.
- Lines 12735-13130: table SELECT and UNION execution entry points plus UNION
  materialization, de-duplication, ordering, and warning propagation. Move UNION
  into a focused SELECT/UNION module.
- Lines 13131-16633: table SELECT materialization: joins, outer joins, grouping,
  aggregates, sorting, distinct, limits, row copying, expression callbacks, and
  predicate diagnostics. Split by rowset, join, aggregate, sort/limit, and
  expression-runtime concerns.
- Lines 16634-17236: scalar SELECT statement copy/evaluation helpers and
  scalar aggregate evaluation. Move to a small scalar-select module after
  metadata inference is split.
- Lines 17237-18735: subquery preparation/scanning/evaluation, row-value
  comparison, and subquery diagnostics. Move after SELECT entry points and
  expression callback APIs are stable.
- Lines 18736-18964: remaining utility/classifier tail: table-select group
  cleanup, row-subquery classifiers, and parse/translate status mapping. Move
  each helper with its owning family; do not create a generic catch-all utility
  module.

## Target Layout

- `src/runtime/mylite_runtime.h`
  Private runtime object model: `mylite_db`, `mylite_stmt`, statement kind,
  statement plans, result metadata, row materialization state, and shared runtime
  constants needed across runtime modules.
- `src/runtime/mylite_diagnostics.{h,c}`
  Error message ownership, warnings, notes, MySQL condition promotion, and
  public diagnostic accessors.
- `src/runtime/mylite_connection.{h,c}`
  Connection lifecycle, selected schema, charset/collation session state,
  transaction release state, and public connection accessors.
- `src/runtime/mylite_statement.{h,c}`
  Public statement lifecycle, `mylite_prepare()`, `mylite_finalize()`,
  `mylite_step()`, statement dispatch, and public result accessors.
- `src/runtime/mylite_catalog.{h,c}`
  `__mylite_*` catalog bootstrap, catalog lookup helpers, metadata loading,
  and physical table naming.
- `src/runtime/mylite_schema.{h,c}`
  `CREATE/ALTER/DROP/USE DATABASE` and schema defaults.
- `src/runtime/mylite_table_ddl.{h,c}` and focused companions such as
  `mylite_table_ddl_alter.{h,c}`
  `CREATE/ALTER/DROP/RENAME/TRUNCATE TABLE` and index DDL.
- `src/runtime/mylite_dml.{h,c}`
  `INSERT`, `REPLACE`, `UPDATE`, `DELETE`, affected rows, auto-increment, and
  statement atomicity.
- `src/runtime/mylite_select.{h,c}`
  SELECT planning, joins, filtering, grouping, ordering, limits, unions, and
  subqueries.
- `src/runtime/mylite_metadata.{h,c}`
  Field descriptors, result metadata inference, and column accessor helpers.
- `src/runtime/mylite_show.{h,c}`
  `SHOW` statements.
- `src/runtime/mylite_information_schema.{h,c}`
  Dynamic `information_schema` result construction.
- `src/runtime/mylite_transactions.{h,c}`
  Transaction statements, savepoints, statement atomicity, and transaction
  release behavior.
- `src/runtime/mylite_span.{h,c}`
  AST span copying, identifier normalization, and small AST child helpers.

## Remaining Runtime Header Ownership

After the first runtime type split, `src/runtime/mylite_runtime.h` should keep
only the core object model and transitional shared helpers listed here:

- `enum mylite_stmt_kind`
  Core statement dispatch state. Keep with `struct mylite_stmt` until statement
  family dispatch is split out of `mylite.c`.
- `enum mylite_information_schema_table`
  Information-schema routing. Move to an information-schema type header when
  that module is extracted.
- `struct mylite_charset_collation_info`
  Expression/metadata collation coercibility helper. Move with collation-aware
  metadata inference.
- `struct mylite_strcmp_compare_options`
  String-comparison runtime helper. Move with the string comparison or collation
  evaluation code.
- `struct mylite_db`
  Core connection object layout. Keep here until a later private
  `mylite_runtime_objects.h` split is justified.
- `struct mylite_statement_timestamp`
  Core statement execution timestamp state. Keep with `struct mylite_stmt`.
- `struct mylite_stmt`
  Core statement object layout. Keep here until feature-family execution plans
  are no longer embedded directly.

## Task List

- [x] Document the intended runtime module layout and migration order.
- [x] Document the current `mylite.c` map and extraction direction.
- [x] Document rules to keep `mylite_runtime.h` from becoming the next
  monolith.
- [x] Move shared runtime structs and enums from `mylite.c` into
  `src/runtime/mylite_runtime.h`.
- [x] Classify every remaining type in `mylite_runtime.h` by owner module.
- [x] Add `mylite_error_codes.h`; move `enum mylite_mysql_condition_code` out
  of `mylite_runtime.h`.
- [x] Add `mylite_field_descriptor.h`; move `struct mylite_field_descriptor`
  and `enum mylite_format_metadata_length`.
- [x] Move field-descriptor nullability flag helpers into
  `src/runtime/mylite_field_descriptor.{h,c}`.
- [x] Add `mylite_schema_types.h`; move schema option/default/presence structs.
- [x] Add `mylite_transaction_types.h`; move transaction enums, savepoint
  state, pending auto-increment state, and statement atomicity structs.
- [x] Add `mylite_table_ddl_types.h`; move create/drop/rename/truncate/alter
  table and index DDL plan structs.
- [x] Add `mylite_dml_types.h`; move insert/update/delete plan and execution
  structs.
- [x] Add `mylite_metadata_types.h`; move result metadata structs.
- [x] Add `mylite_select_types.h`; move SELECT plan, row, join, group,
  aggregate, union, and subquery scan structs.
- [x] Add `mylite_show_types.h`; move `SHOW` query/target/info structs.
- [x] Add an include-discipline check or documented review rule to prevent new
  broad `mylite_runtime.h` dependencies.
- [x] Add a lightweight CI guard for runtime header line count or forbidden
  feature-owned type prefixes in `mylite_runtime.h`.
- [ ] Move immutable runtime constants from `mylite.c` into focused private
  modules without creating unused-header warnings.
- [x] Move MySQL display-length constants used by metadata inference into
  `mylite_metadata`.
- [x] Move charset/collation constant names and ids into charset/metadata
  boundaries.
- [x] Move storage-engine registry rows into `mylite_show` or
  `mylite_information_schema`.
- [x] Extract diagnostics into `src/runtime/mylite_diagnostics.{h,c}`.
- [x] Move reusable charset/collation diagnostic helpers into
  `src/runtime/mylite_diagnostics`.
- [x] Extract public connection state accessors into
  `src/runtime/mylite_connection.{h,c}`.
- [x] Extract public connection opening into `src/runtime/mylite_connection`.
- [x] Extract public connection close after transaction cleanup moves out of
  `mylite.c`.
- [x] Extract catalog bootstrap DDL and system schema seeding into
  `src/runtime/mylite_catalog.{h,c}`.
- [x] Extract reusable span, string, and AST-child helpers into
  `src/runtime/mylite_span.{h,c}`.
- [x] Extract transaction statements, state, savepoints, pending auto-increment
  tracking, and statement atomicity into `src/runtime/mylite_transactions.{h,c}`.
- [x] Move savepoint statement cleanup into `src/runtime/mylite_transactions`.
- [ ] Extract public statement lifecycle and result accessors into
  `src/runtime/mylite_statement.{h,c}` while keeping statement-family execution
  in `mylite.c`.
- [x] Start `src/runtime/mylite_statement.{h,c}` with affected-row access and
  row-count bookkeeping.
- [x] Move table-select row/current-result cleanup into
  `src/runtime/mylite_statement`.
- [x] Move scalar SELECT result and select-constant cleanup into
  `src/runtime/mylite_statement`.
- [x] Move UNION plan cleanup into `src/runtime/mylite_statement`.
- [x] Move statement finalization and public row-value accessors into
  `src/runtime/mylite_statement`.
- [x] Move public statement stepping into `src/runtime/mylite_statement`,
  keeping statement-family execution dispatch in `mylite.c`.
- [x] Move SQLite statement preparation into `src/runtime/mylite_statement`.
- [x] Move statement write-kind and diagnostics-preservation classifiers into
  `src/runtime/mylite_statement`.
- [x] Extract public result metadata accessors into
  `src/runtime/mylite_metadata.{h,c}`.
- [x] Move result metadata cleanup into `src/runtime/mylite_metadata`.
- [x] Move result metadata text-copy ownership into
  `src/runtime/mylite_metadata`.
- [x] Split catalog lookup helpers into `src/runtime/mylite_catalog`.
- [x] Move physical table naming into `src/runtime/mylite_catalog`.
- [x] Move selected schema mutation helpers into `src/runtime/mylite_connection`.
- [x] Move connection charset state mutation helpers into
  `src/runtime/mylite_connection`.
- [x] Move `SET NAMES` execution into `src/runtime/mylite_connection`.
- [x] Move `SET CHARACTER SET` execution into `src/runtime/mylite_connection`.
- [x] Move connection charset AST copy/prepare helpers into the connection
  module or a small connection-statement module.
- [x] Split `SHOW` and `information_schema` dynamic result builders.
- [x] Create `src/runtime/mylite_show.{h,c}` and wire it into CMake.
- [x] Move `SHOW DATABASES`/`SHOW SCHEMAS` SQL builders into `mylite_show`.
- [x] Move `SHOW DATABASES` prepare code into `mylite_show`.
- [x] Move `SHOW VARIABLES` builder into `mylite_show`.
- [x] Move `SHOW VARIABLES` prepare code into `mylite_show`.
- [x] Move `SHOW STATUS` builder into `mylite_show`.
- [x] Move `SHOW STATUS` prepare code into `mylite_show`.
- [x] Move `SHOW ENGINES` prepare/build code and metadata into `mylite_show`.
- [x] Move `SHOW ENGINES` storage-engine SQL builders and result metadata into
  `mylite_show`.
- [x] Move `SHOW CHARACTER SET` and `information_schema.CHARACTER_SETS`
  builders into `mylite_show`.
- [x] Move `SHOW CHARACTER SET` prepare code into `mylite_show`.
- [x] Move `SHOW COLLATION`, `information_schema.COLLATIONS`, and
  `information_schema.COLLATION_CHARACTER_SET_APPLICABILITY` builders into
  `mylite_show`.
- [x] Move `SHOW COLLATION` prepare code into `mylite_show`.
- [x] Move `information_schema.KEYWORDS` builder into `mylite_show`.
- [x] Move `SHOW TABLES` builder into `mylite_show`.
- [x] Move `SHOW TABLES` prepare code into `mylite_show`.
- [x] Move `SHOW TABLE STATUS` builder into `mylite_show`.
- [x] Move `SHOW TABLE STATUS` prepare code into `mylite_show`.
- [x] Move `SHOW COLUMNS`/`DESCRIBE` builder into `mylite_show`.
- [x] Move `SHOW COLUMNS` and `DESCRIBE` prepare code into `mylite_show`.
- [x] Move `SHOW INDEX` builder into `mylite_show`.
- [x] Move `SHOW INDEX` prepare code into `mylite_show`.
- [x] Move `SHOW CREATE TABLE` prepare/build code into `mylite_show`.
- [x] Move `SHOW CREATE DATABASE` prepare/build code into `mylite_show`.
- [x] Move `SHOW WARNINGS`, `SHOW ERRORS`, and diagnostic count display into
  `mylite_show` or diagnostics.
- [x] Create `src/runtime/mylite_information_schema.{h,c}` and wire it into
  CMake.
- [x] Move information-schema table detection into
  `mylite_information_schema`.
- [x] Move static information-schema SQL strings into
  `mylite_information_schema`.
- [x] Move dynamic information-schema character set/collation/keyword/engine
  builders into `mylite_information_schema`.
- [x] Move information-schema SELECT passthrough preparation into
  `mylite_information_schema`.
- [ ] Split schema lifecycle and table/index DDL plans and execution.
- [x] Start `src/runtime/mylite_schema.{h,c}` with schema option cleanup.
- [x] Move schema option normalization into `mylite_schema`.
- [x] Move schema AST copy helpers into `mylite_schema`.
- [x] Move `CREATE DATABASE` execution into `mylite_schema`.
- [x] Move `ALTER DATABASE` execution into `mylite_schema`.
- [x] Move `DROP DATABASE` execution into `mylite_schema`.
- [x] Move `USE DATABASE` execution into `mylite_schema`.
- [x] Start `src/runtime/mylite_table_ddl.{h,c}` with table/index DDL plan
  cleanup.
- [x] Move create-table AST copy helpers into `mylite_table_ddl`.
- [x] Move create-table normalization and validation into `mylite_table_ddl`.
- [x] Move create-table catalog writes behind a narrow table DDL/catalog API.
- [x] Move create-table physical SQL construction into `mylite_table_ddl`.
- [x] Move `CREATE TABLE` execution into `mylite_table_ddl`.
- [x] Move drop-table AST copy helpers into `mylite_table_ddl`.
- [x] Move `DROP TABLE` validation and execution into `mylite_table_ddl`.
- [x] Move rename-table AST copy helpers into `mylite_table_ddl`.
- [x] Move `RENAME TABLE` validation and execution into `mylite_table_ddl`.
- [x] Move truncate-table AST copy helpers into `mylite_table_ddl`.
- [x] Move `TRUNCATE TABLE` validation and execution into `mylite_table_ddl`.
- [x] Move alter-table AST copy helpers into `mylite_table_ddl`.
- [x] Move alter-table catalog model loading behind table DDL/catalog APIs.
- [x] Move alter-table column operations into `mylite_table_ddl`.
- [x] Move alter-table key/index operations into `mylite_table_ddl`.
- [x] Move alter-table final model validation into `mylite_table_ddl`.
- [x] Move alter-table unique-index data validation into a focused table DDL
  validation companion module.
- [x] Move create/drop index AST copy helpers into `mylite_table_ddl`.
- [x] Move `CREATE INDEX` and `DROP INDEX` validation/execution into
  `mylite_table_ddl`.
- [x] Move generated index-name logic into `mylite_table_ddl`.
- [x] Move table DDL prepared-statement wrappers for rename, truncate, and
  standalone index statements into a focused table DDL statement module.
- [x] Move ALTER TABLE prepared execution, rebuild transactions, shadow-table
  SQL, and diagnostics into a focused table DDL alter execution module.
- [x] Move ALTER TABLE catalog rewrite helpers into a focused table DDL catalog
  companion module.
- [x] Move ALTER TABLE warning generation into a focused table DDL warnings
  companion module.
- [ ] Split DML plans and execution.
- [x] Start `src/runtime/mylite_dml.{h,c}` with DML plan and row cleanup.
- [x] Move insert/replacement AST copy helpers into `mylite_dml`.
- [x] Move update/delete AST copy helpers into `mylite_dml`.
- [x] Move insert target validation into `mylite_dml`.
- [x] Move insert column-list, row-alias, and `INSERT ... SET` target
  validation into `mylite_dml`.
- [x] Move insert-table metadata loading behind catalog/metadata helper APIs.
- [x] Move insert bound-value conversion into `mylite_dml`.
- [x] Move shared insert row-write helpers into `mylite_dml`.
- [x] Move non-ODKU insert row execution into `mylite_dml`.
- [x] Move insert row execution into `mylite_dml`.
- [x] Move insert unique-conflict checks into `mylite_dml`.
- [x] Move `ON DUPLICATE KEY UPDATE` row writeback helpers into `mylite_dml`.
- [x] Move `ON DUPLICATE KEY UPDATE` `VALUES()` deprecation warnings into
  `mylite_dml`.
- [x] Move `ON DUPLICATE KEY UPDATE` assignment validation into `mylite_dml`.
- [x] Move `ON DUPLICATE KEY UPDATE` assignment evaluation into `mylite_dml`.
- [x] Split `ON DUPLICATE KEY UPDATE` assignment validation/evaluation into a
  focused DML companion module.
- [x] Move `ON DUPLICATE KEY UPDATE` conflict/update row branch into
  `mylite_dml`.
- [x] Move `ON DUPLICATE KEY UPDATE` row value resolution into `mylite_dml`.
- [x] Move insert auto-increment persistence behind a stmt-free transaction
  helper while keeping rollback hooks in transactions.
- [x] Move `INSERT ... ON DUPLICATE KEY UPDATE` execution into `mylite_dml`.
- [x] Move INSERT/REPLACE transaction orchestration into `mylite_dml`
  without coupling DML execution to `mylite_stmt`.
- [x] Split INSERT/REPLACE transaction orchestration into a focused DML
  companion module.
- [x] Move INSERT/REPLACE statement execution wrappers into a focused DML
  statement module.
- [ ] Move `INSERT IGNORE` warning/error downgrade logic into `mylite_dml`.
- [x] Move `REPLACE` delete-then-insert behavior into `mylite_dml`.
- [x] Move update target copy and assignment target binding into `mylite_dml`.
- [x] Move remaining update clause and assignment value validation into
  `mylite_dml`.
- [x] Move update/delete rowset sorting and LIMIT trimming into a focused DML
  companion module.
- [x] Move shared UPDATE/DELETE order-key append helper into a focused DML
  companion module.
- [x] Move UPDATE assignment, WHERE, and ORDER binding into a focused DML
  companion module without depending on `mylite_stmt`.
- [x] Move DELETE WHERE, LIMIT, and ORDER binding into a focused DML companion
  module without depending on `mylite_stmt`.
- [x] Move UPDATE/DELETE row materialization into a focused DML companion module
  behind narrow session-function and WHERE-diagnostic callbacks.
- [x] Move UPDATE row write transaction and assignment evaluation into a
  focused DML execution module behind the same session-function callback.
- [x] Move update/delete rowset scan and update write SQL builders into a
  focused DML companion module.
- [x] Move update unique-check SQL and SQLite value binding into a focused DML
  companion module.
- [x] Move update/delete rowset population helpers into a focused DML companion
  module.
- [x] Move update candidate value copy, default conversion, row-change
  comparison, and auto-increment value extraction into a focused DML companion
  module.
- [x] Move update default resolution, assignment value normalization, and
  auto-increment advancement into a focused DML companion module.
- [x] Move update unique-index conflict validation and duplicate diagnostics
  into a focused DML companion module.
- [x] Move shared DML expression warning promotion and condition-error
  selection into a focused DML companion module.
- [x] Move UPDATE/DELETE row-expression identifier resolution into a focused
  DML companion module.
- [x] Move UPDATE/DELETE prepare-time plan cloning into a focused DML companion
  module.
- [x] Move UPDATE-specific diagnostics into a focused DML companion module.
- [x] Move update row materialization logic into `mylite_dml`.
- [x] Move update row writeback into `mylite_dml`.
- [x] Move delete target copy into `mylite_dml`.
- [x] Move remaining delete target validation into `mylite_dml`.
- [x] Move delete row materialization/sorting/limit logic into `mylite_dml`.
- [x] Move delete physical row execution into `mylite_dml`.
- [x] Move DELETE-specific diagnostics into a focused DML companion module.
- [x] Move shared DML NOT NULL column diagnostics into a focused DML companion
  module.
- [x] Move `REPLACE DELAYED` warning generation into the focused DML statement
  module while preserving dispatch-time warning order.
- [x] Move UPDATE/DELETE statement execution wrappers into a focused DML
  statement module while keeping expression callbacks local.
- [ ] Move remaining DML-specific diagnostics into `mylite_dml`.
- [ ] Split SELECT, UNION, aggregate, and subquery planning/execution.
- [x] Start `src/runtime/mylite_select.{h,c}` with SELECT plan cleanup.
- [x] Move SELECT plan accessors and plan container mutation helpers into
  `mylite_select`.
- [x] Move shared table-target and column-reference resolution helpers into
  `mylite_select`.
- [ ] Move SELECT AST copy/bind helpers into `mylite_select`.
- [x] Move table-column catalog loading behind a focused `mylite_select_catalog`
  helper.
- [ ] Move table target resolution and remaining column loading behind catalog/metadata
  helper APIs.
- [ ] Move wildcard expansion and output-column planning into `mylite_select`.
- [ ] Move SELECT predicate binding into `mylite_select`.
- [x] Move join planning and `USING` resolution into `mylite_select`.
- [x] Move shared SELECT `USING` column resolution helpers into
  `mylite_select`.
- [x] Move shared SELECT plan predicate helpers into `mylite_select`.
- [ ] Move `ORDER BY`, `GROUP BY`, `HAVING`, and `LIMIT` binding into
  `mylite_select`.
- [x] Move SELECT LIMIT binding and row-keeping helpers into `mylite_select`.
- [x] Move SELECT duplicate-mode and custom-runtime policy helpers into
  `mylite_select`.
- [ ] Move DISTINCT validation into `mylite_select`.
- [ ] Move grouping validation into `mylite_select`.
- [x] Move SELECT SQL construction into `mylite_select`.
- [ ] Move table SELECT materialization into `mylite_select`.
- [x] Move table SELECT row copying, result ownership, rowset allocation, and
  LIMIT trimming helpers into `mylite_select_rowset`.
- [ ] Move table SELECT result sorting, DISTINCT checks, and LIMIT application
  into `mylite_select`.
- [ ] Move outer join materialization into `mylite_select`.
- [ ] Move table SELECT join rowset loading and join condition caches into
  `mylite_select`.
- [ ] Move aggregate state and count-distinct state into `mylite_select`.
- [ ] Move scalar SELECT planning/execution into `mylite_select` or a small
  scalar-select module after metadata inference is split.
- [ ] Move scalar aggregate evaluation into the scalar-select or aggregate
  module chosen above.
- [x] Move reusable prepared-statement AST clone/remap helpers out of
  `mylite.c`.
- [ ] Move UNION operand collection and preparation into `mylite_select`.
- [ ] Move UNION materialization/dedup/order logic into `mylite_select`.
- [ ] Move subquery preparation/scanning/evaluation into `mylite_select` after
  scalar/table SELECT entry points are module-owned.
- [ ] Move row-value comparison helpers into `mylite_select` or expression
  helpers according to final users.
- [ ] Move reusable field descriptor and metadata inference code into
  `src/runtime/mylite_metadata`.
- [x] Move shared expression descriptor utility helpers into
  `src/runtime/mylite_expression_descriptor`.
- [x] Move UNION field descriptor merge rules into
  `src/runtime/mylite_expression_descriptor`.
- [x] Move expression-value descriptor and operator nullability leaf helpers
  into `src/runtime/mylite_expression_descriptor`.
- [x] Move shared expression charset validation helpers out of `mylite.c`.
- [x] Move catalog column descriptor source helpers out of `mylite_runtime.h`.
- [ ] Move reusable catalog-to-field-descriptor inference into `mylite_metadata`
  after SELECT owns its table-column loading boundary.
- [ ] Move field descriptor inference for literals into `mylite_metadata`.
- [ ] Move field descriptor inference for identifiers into `mylite_metadata`.
- [ ] Move field descriptor inference for unary/binary/ternary expressions into
  `mylite_metadata`.
- [ ] Move field descriptor inference for built-in functions into
  `mylite_metadata` or function-family helpers.
- [x] Move pure SQL function-name classifiers into
  `src/runtime/mylite_function_names`.
- [ ] Move aggregate metadata inference into `mylite_metadata`.
- [ ] Move result metadata attachment for SELECT/UNION into `mylite_metadata`.
- [x] Move reusable result metadata label lookup into `mylite_metadata`.
- [ ] Move column type descriptor to SQLite affinity mapping into metadata or
  DDL according to final ownership.
- [x] Move temporal statement timestamp helpers out of `mylite.c`.
- [x] Move core session-function evaluation out of `mylite.c` without pulling
  string/collation inference into the session module.
- [ ] Move string/number conversion helpers used only by INSERT into
  `mylite_dml`.
- [x] Move parse/translate status mapping into `mylite_statement`.
- [ ] Move custom statement execution dispatch into `mylite_statement` after
  every statement family exposes an execution entry point.
- [ ] Move parsed statement dispatch into `mylite_statement` after every
  statement family exposes a prepare entry point.
- [ ] Delete stale prototype blocks from `mylite.c` as each region moves.
- [ ] Keep `mylite.c` as a thin integration file only while extraction is in
  progress; remove it when all statement families have homes.

## Rules

- No compatibility behavior changes in modularization commits.
- One moved concern per commit.
- Keep public ABI stable.
- Keep private dependencies acyclic: connection and diagnostics at the bottom,
  catalog above connection, statement dispatch above statement-family modules.
- Prefer narrow private functions over exposing mutable fields.
- If a moved module needs broad access to `mylite_stmt`, stop and split the
  relevant plan/result structures before moving more behavior.
