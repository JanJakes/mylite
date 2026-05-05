# `mylite.c` Modularization

`packages/libmylite/src/mylite.c` has been fully removed. It used to be the
runtime integration point for connection state, statement plans, catalog SQL,
DDL, DML, SELECT execution, metadata, diagnostics, and utility code. The first
modularization phase succeeded because it moved behavior behind private
interfaces while preserving the public ABI and keeping the runtime test suite
green after each slice.

This document is now the post-removal architecture contract. It should keep the
new runtime modules from becoming replacement monoliths while the remaining
large statement-family modules are split by ownership.

## Architecture Assessment

The direction remains sound: the recent commits extracted runtime ownership by
concern, preserved the public ABI, and kept behavior stable. That remains the
right migration path for a compatibility layer because broad rewrites make MySQL
behavior regressions hard to isolate.

The main monolith is gone. The remaining risk is second-order monolith growth:

- `mylite_statement_prepare.c` should stay a prepare switchboard, not a home for
  statement-family validation, cloning, or metadata inference.
- `mylite_statement_execute.c` should stay execution dispatch, not statement
  execution logic.
- `mylite_select_context.c` should stay static callback composition and tiny
  runtime adapters, not SELECT planning or execution logic.
- `mylite_runtime.h` should stay a private object-layout header, not a shared
  type dumping ground.
- The largest remaining modules need follow-up ownership splits:
  `mylite_table_ddl.c`, `mylite_select_materialize.c`,
  `mylite_dml_insert_value_resolve.c`, `mylite_table_ddl_alter.c`, and
  `mylite_select_subquery_eval.c`.
- The broadest type headers also need follow-up splits once ownership is clear:
  `mylite_select_types.h`, `mylite_dml.h`, `mylite_dml_types.h`, and
  `mylite_table_ddl_types.h`.

The next architecture phase should keep this layering:

1. Diagnostics and connection state stay at the bottom.
2. Catalog owns persisted `__mylite_*` metadata access and physical object
   naming.
3. Statement owns public statement lifetime, row/result access, prepare routing,
   and execution routing.
4. Statement-family modules own their plan copy, validation, execution,
   cleanup, and diagnostics.
5. SELECT and expression-heavy code stay split by role: planning, descriptor
   inference, materialization, scalar evaluation, UNION, subquery evaluation,
   rowsets, grouping, and diagnostics.

Good next extraction order:

1. Split `mylite_table_ddl.c` into statement preparation, create/drop/rename,
   truncate, shared catalog model, SQL builders, and validation modules.
2. Split `mylite_select_materialize.c` into base-table scanning, joined-row
   assembly, outer-join orchestration, and materialization driver modules.
3. Split `mylite_dml_insert_value_resolve.c` into column-list resolution,
   positional/default resolution, `INSERT ... SET` resolution, and SQLite bind
   helpers.
4. Split `mylite_select_subquery_eval.c` into scalar subquery, quantified
   subquery, row-value comparison, and diagnostics modules.
5. Split broad type headers after the implementation owners are narrow enough
   that the new headers have one clear purpose.
6. Convert guardrails from this document into build checks whenever they can be
   expressed mechanically.

## Runtime Header Guardrails

`mylite_runtime.h` is a private object-layout header. It is intentionally small
after the first extraction phase and should stay that way. Moving feature-owned
types back into it would recreate the monolith as an object model instead of an
implementation file.

Keep `mylite_runtime.h` limited to:

- `struct mylite_db`
- `struct mylite_stmt`
- `struct mylite_statement_timestamp`
- tiny cross-runtime primitives needed by those objects

`enum mylite_stmt_kind` lives in `mylite_statement_types.h`. Do not move it back
unless statement dispatch and object layout are deliberately redesigned together.

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
- If a module starts mutating another module's plan internals, stop and move that
  plan type to the owning module before moving more behavior.
- Headers should form a one-way dependency graph: diagnostics and connection
  stay low, catalog sits above them, statement dispatch depends on
  statement-family modules, and statement-family modules depend on catalog,
  metadata, diagnostics, and span helpers as needed.
- Track progress by ownership clarity, not only by reducing `mylite.c` line
  count.
- After every extraction, check whether declarations in `mylite_runtime.h` or a
  broad family header can move into a narrower type header.

## Completed `mylite.c` Removal Map

`packages/libmylite/src/mylite.c` no longer exists and should not be
reintroduced. Its former responsibilities now have these owners:

- Public `mylite_prepare()` API:
  `src/runtime/mylite_statement_prepare.c`.
- Parsed statement routing, family prepare calls, parser status mapping, and
  SQLite fallback translation: `src/runtime/mylite_statement_prepare.c`.
- Custom statement execution dispatch:
  `src/runtime/mylite_statement_execute.c`.
- Public statement stepping, finalization, row accessors, and lifecycle cleanup:
  `src/runtime/mylite_statement.c`.
- SELECT callback graph and subquery runtime adapters:
  `src/runtime/mylite_select_context.c`.
- Expression descriptor dispatch across scalar, aggregate, collation, and
  SELECT metadata inference:
  `src/runtime/mylite_expression_descriptor_dispatch.c`.
- Table SELECT row stepping:
  `src/runtime/mylite_select_statement.c`.
- SELECT predicate error mapping:
  `src/runtime/mylite_select_diagnostics.c`.
- Scalar SELECT planning and execution:
  `src/runtime/mylite_select_scalar.c`.
- UNION operand preparation and runtime:
  `src/runtime/mylite_select_union_prepare.c` and
  `src/runtime/mylite_select_union.c`.
- Subquery preparation, scanning, evaluation, and row-value comparison:
  `src/runtime/mylite_select_subquery*.{h,c}`.

## Target Layout

- `src/runtime/mylite_runtime.h`
  Private runtime object layout: `mylite_db`, `mylite_stmt`, statement timestamp
  state, and only the tiny primitives needed by those objects.
- `src/runtime/mylite_diagnostics.{h,c}`
  Error message ownership, warnings, notes, MySQL condition promotion, and
  public diagnostic accessors.
- `src/runtime/mylite_connection.{h,c}`
  Connection lifecycle, selected schema, charset/collation session state,
  transaction release state, and public connection accessors.
- `src/runtime/mylite_statement.{h,c}`,
  `src/runtime/mylite_statement_prepare.{h,c}`, and
  `src/runtime/mylite_statement_execute.{h,c}`
  Public statement lifecycle, `mylite_prepare()`, `mylite_finalize()`,
  `mylite_step()`, public result accessors, prepare routing, and execution
  routing.
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
- `src/runtime/mylite_select_group.{h,c}`
  Table SELECT group state ownership, aggregate-state updates, group-key
  matching, finalized row construction, and group cleanup.
- `src/runtime/mylite_select_materialize.{h,c}`
  Table SELECT row materialization orchestration, join and outer-join scanning,
  row filtering, DISTINCT checks, ORDER/LIMIT application, and result-row
  appends behind SELECT eval callbacks.
- `src/runtime/mylite_select_union.{h,c}`
  UNION execution, operand scanning, de-duplication, global ordering,
  warning aggregation, and row stepping behind operand/eval callbacks.
- `src/runtime/mylite_expression_descriptor*.{h,c}`,
  `src/runtime/mylite_select_metadata.{h,c}`, and
  `src/runtime/mylite_metadata.{h,c}`
  Field descriptors, expression descriptor inference, SELECT/UNION result
  metadata attachment, and public column accessor helpers.
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

`src/runtime/mylite_runtime.h` should keep only the core object model:

- `struct mylite_db`
  Core connection object layout. Keep here until a later private
  `mylite_runtime_objects.h` split is justified.
- `struct mylite_statement_timestamp`
  Core statement execution timestamp state. Keep with `struct mylite_stmt`.
- `struct mylite_stmt`
  Core statement object layout. Keep here until feature-family execution plans
  are no longer embedded directly.

The current 240-line build guard is a ceiling, not a target. New runtime work
should keep the header close to its current size and move complete type families
into focused `*_types.h` headers.

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
- [x] Move immutable runtime constants from `mylite.c` into focused private
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
- [x] Extract public statement lifecycle, result accessors, prepare routing,
  and execution routing into focused statement modules.
- [x] Start `src/runtime/mylite_statement.{h,c}` with affected-row access and
  row-count bookkeeping.
- [x] Move table-select row/current-result cleanup into
  `src/runtime/mylite_statement`.
- [x] Move scalar SELECT result and select-constant cleanup into
  `src/runtime/mylite_statement`.
- [x] Move UNION plan cleanup into `src/runtime/mylite_statement`.
- [x] Move statement finalization and public row-value accessors into
  `src/runtime/mylite_statement`.
- [x] Move public statement stepping into `src/runtime/mylite_statement`.
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
- [ ] Continue splitting schema lifecycle and table/index DDL plans and
  execution; `mylite_table_ddl.c` remains the largest runtime module.
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
- [x] Split create-table AST plan-copy helpers into a focused table DDL create
  copy module.
- [x] Split index, drop, rename, truncate, and ALTER AST plan-copy helpers into
  a focused table DDL copy module.
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
- [x] Move table DDL plan/model cleanup into a focused table DDL plan module.
- [x] Move ALTER TABLE prepared execution, rebuild transactions, shadow-table
  SQL, and diagnostics into a focused table DDL alter execution module.
- [x] Move ALTER TABLE catalog rewrite helpers into a focused table DDL catalog
  companion module.
- [x] Move ALTER TABLE warning generation into a focused table DDL warnings
  companion module.
- [x] Move ALTER TABLE catalog model loading into a focused table DDL model
  companion module.
- [x] Move ALTER TABLE index-action application and key diagnostics into a
  focused table DDL index-action companion module.
- [ ] Continue splitting DML plans and execution; insert value resolution and
  broad DML headers remain the next ownership risks.
- [x] Start `src/runtime/mylite_dml.{h,c}` with DML plan and row cleanup.
- [x] Move insert/replacement AST copy helpers into `mylite_dml`.
- [x] Move update/delete AST copy helpers into `mylite_dml`.
- [x] Move insert target validation into `mylite_dml`.
- [x] Move insert column-list, row-alias, and `INSERT ... SET` target
  validation into `mylite_dml`.
- [x] Move insert-table metadata loading behind catalog/metadata helper APIs.
- [x] Move insert bound-value conversion into `mylite_dml`.
- [x] Split insert bound-value binding, copying, and numeric parsing into a
  focused DML companion module.
- [x] Split insert column-reference qualifier and table-column lookup helpers
  into a focused DML companion module.
- [x] Split insert default, text, current-timestamp, and auto-increment value
  resolution into a focused DML companion module.
- [x] Split `INSERT ... SET` expression evaluation into a focused DML companion
  module.
- [x] Split `INSERT ... SET` row resolution into a focused DML companion
  module.
- [x] Split insert diagnostics and warning de-duplication into a focused DML
  companion module.
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
- [x] Split `ON DUPLICATE KEY UPDATE` assignment expression evaluation into a
  focused DML insert update expression module.
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
- [x] Split `REPLACE` row execution and conflict deletion into a focused DML
  insert companion module.
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
- [ ] Continue splitting SELECT, UNION, aggregate, and subquery
  planning/execution; the old `mylite.c` code is module-owned, but several
  SELECT modules are still broad.
- [x] Start `src/runtime/mylite_select.{h,c}` with SELECT plan cleanup.
- [x] Move SELECT plan accessors and plan container mutation helpers into
  `mylite_select`.
- [x] Move shared table-target and column-reference resolution helpers into
  `mylite_select`.
- [x] Move plan-level wildcard and column-reference resolution into
  `mylite_select_resolve`.
- [ ] Move SELECT AST copy/bind helpers into `mylite_select`.
- [x] Move table-column catalog loading behind a focused `mylite_select_catalog`
  helper.
- [x] Split SELECT catalog column descriptor reconstruction into a focused
  catalog descriptor module.
- [ ] Move table target resolution and remaining column loading behind catalog/metadata
  helper APIs.
- [x] Move wildcard expansion and wildcard output-column planning into a
  focused SELECT projection module.
- [ ] Move SELECT predicate binding into `mylite_select`.
- [x] Move join planning and `USING` resolution into `mylite_select`.
- [x] Move shared SELECT `USING` column resolution helpers into
  `mylite_select`.
- [x] Move shared SELECT plan predicate helpers into `mylite_select`.
- [ ] Move `ORDER BY`, `GROUP BY`, `HAVING`, and `LIMIT` binding into
  `mylite_select`.
- [x] Split recursive `ORDER BY` expression binding into a focused SELECT
  order companion module.
- [x] Move SELECT LIMIT binding and row-keeping helpers into `mylite_select`.
- [x] Move SELECT duplicate-mode and custom-runtime policy helpers into
  `mylite_select`.
- [ ] Move DISTINCT validation into `mylite_select`.
- [ ] Move grouping validation into `mylite_select`.
- [x] Move SELECT SQL construction into `mylite_select`.
- [x] Move table SELECT materialization into a focused SELECT materialize
  module.
- [x] Move table SELECT row copying, result ownership, rowset allocation, and
  LIMIT trimming helpers into `mylite_select_rowset`.
- [x] Move shared SQLite column-to-expression value copying into a focused
  runtime helper used by SELECT and subquery row copying.
- [x] Move table SELECT result sorting, DISTINCT checks, and LIMIT application
  into focused SELECT rowset helpers.
- [x] Move table SELECT expression evaluation callbacks, output
  materialization, current-row projection, and order-value evaluation into a
  focused SELECT eval module.
- [x] Move outer join materialization into the focused SELECT materialize
  module.
- [x] Move table SELECT join condition cache range calculation, lookups, stores,
  and cleanup into a focused SELECT join-cache module.
- [x] Move table SELECT current SQLite row copying, eval column-copy callback,
  and join rowset loading into a focused SELECT row-loader module.
- [x] Move table SELECT group state allocation, lookup, aggregate updates,
  finalized-row construction, and cleanup into a focused SELECT group module.
- [x] Move aggregate state and count-distinct state into a focused
  `mylite_select_aggregate` module.
- [x] Move scalar SELECT planning/execution into `mylite_select` or a small
  scalar-select module after metadata inference is split.
- [x] Move scalar aggregate evaluation into the scalar-select or aggregate
  module chosen above.
- [x] Move reusable prepared-statement AST clone/remap helpers out of
  `mylite.c`.
- [x] Move UNION operand collection and preparation into `mylite_select`.
- [x] Move UNION execution, materialization, deduplication, global ordering,
  operand row copying, and warning aggregation into a focused SELECT UNION
  module.
- [x] Move subquery preparation/scanning/evaluation into `mylite_select` after
  scalar/table SELECT entry points are module-owned.
- [x] Move row-value comparison helpers into the focused SELECT subquery
  evaluation module.
- [x] Move reusable field descriptor and metadata inference code into
  focused expression descriptor and metadata modules.
- [x] Move shared expression descriptor utility helpers into
  `src/runtime/mylite_expression_descriptor`.
- [x] Move UNION field descriptor merge rules into
  `src/runtime/mylite_expression_descriptor`.
- [x] Move expression-value descriptor and operator nullability leaf helpers
  into `src/runtime/mylite_expression_descriptor`.
- [x] Move shared expression charset validation helpers out of `mylite.c`.
- [x] Move catalog column descriptor source helpers out of `mylite_runtime.h`.
- [x] Move reusable catalog-to-field-descriptor inference into `mylite_metadata`
  after SELECT owns its table-column loading boundary.
- [x] Move field descriptor inference for literals into focused expression
  descriptor modules.
- [x] Move field descriptor inference for identifiers into focused expression
  descriptor modules.
- [x] Move field descriptor inference for unary/binary/ternary expressions into
  focused expression descriptor modules.
- [x] Move field descriptor inference for built-in functions into focused
  expression descriptor function-family helpers.
- [x] Move pure SQL function-name classifiers into
  `src/runtime/mylite_function_names`.
- [x] Split SQL function-name classifiers into focused family modules while
  keeping one internal caller-facing header.
- [x] Move aggregate metadata inference into focused expression descriptor
  modules.
- [x] Move result metadata attachment for SELECT/UNION into focused SELECT
  metadata modules.
- [x] Move reusable result metadata label lookup into `mylite_metadata`.
- [ ] Move column type descriptor to SQLite affinity mapping into metadata or
  DDL according to final ownership.
- [x] Move temporal statement timestamp helpers out of `mylite.c`.
- [x] Move core session-function evaluation out of `mylite.c` without pulling
  string/collation inference into the session module.
- [ ] Move string/number conversion helpers used only by INSERT into
  `mylite_dml`.
- [x] Move parse/translate status mapping into `mylite_statement`.
- [x] Move custom statement execution dispatch into `mylite_statement` after
  every statement family exposes an execution entry point.
- [x] Move parsed statement dispatch into `mylite_statement` after every
  statement family exposes a prepare entry point.
- [x] Delete stale prototype blocks from `mylite.c` as each region moves.
- [x] Remove `mylite.c` when all statement families have homes.
- [x] Add a build guard that fails if `packages/libmylite/src/mylite.c` is
  reintroduced or added back to the libmylite source list.
- [ ] Split `mylite_table_ddl.c` into narrower create/drop/rename/truncate,
  catalog-model, SQL-builder, validation, and statement-preparation modules.
- [x] Split DROP TABLE execution, validation, catalog cleanup, and physical
  table removal into a focused table DDL module.
- [x] Split CREATE TABLE physical SQL generation and backing-table creation into
  a focused table DDL module.
- [x] Split TRUNCATE TABLE validation, row deletion, and auto-increment reset
  into a focused table DDL module.
- [x] Split RENAME TABLE validation, physical table rename, and catalog rewrite
  into a focused table DDL module.
- [x] Move ALTER TABLE final-model validation into the focused ALTER validation
  module.
- [x] Move ALTER TABLE index-action application and key diagnostics into a
  focused table DDL index-action companion module.
- [x] Split ALTER TABLE rebuild, shadow-table copy, and physical swap into a
  focused table DDL alter rebuild module.
- [x] Split SELECT row and join-condition matching into a focused SELECT
  companion module.
- [x] Split SELECT joined-row allocation, copying, and null-extension helpers
  into a focused SELECT companion module.
- [x] Split SELECT outer-join range rowset assembly into a focused SELECT
  companion module.
- [x] Split SELECT JOIN `USING` request capture and resolution into a focused
  FROM-clause companion module.
- [x] Split SELECT GROUP/HAVING reference resolution and grouped-column
  membership checks into a focused group resolver module.
- [x] Split insert column-reference qualifier and table-column lookup helpers
  into a focused DML companion module.
- [x] Split INSERT value and column-reference AST copy helpers into a focused
  DML companion module.
- [x] Split SELECT row-constructor comparison and null detection into a focused
  SELECT companion module.
- [x] Split scalar subquery and `EXISTS` evaluation into a focused SELECT
  subquery companion module.
- [x] Split quantified subquery evaluation into a focused SELECT subquery
  companion module.
- [x] Split row subquery evaluation into a focused SELECT subquery companion
  module.
- [x] Split insert default, text, current-timestamp, and auto-increment value
  resolution into a focused DML companion module.
- [x] Split `INSERT ... SET` expression evaluation into a focused DML companion
  module.
- [x] Split `INSERT ... SET` row resolution into a focused DML companion
  module.
- [x] Split `ON DUPLICATE KEY UPDATE` assignment column-reference resolution
  into a focused DML companion module.
- [x] Split CREATE TABLE physical SQL generation and backing-table creation into
  a focused table DDL module.
- [x] Split CREATE TABLE catalog insertion into a focused table DDL module.
- [x] Split CREATE TABLE option normalization into a focused table DDL module.
- [x] Split CREATE TABLE validation into a focused table DDL module.
- [x] Split CREATE TABLE column-definition AST copying into a focused table DDL
  create column copy module.
- [x] Split standalone CREATE/DROP INDEX catalog mutations into a focused table
  DDL index catalog module.
- [x] Split create-table plan column lookup into a focused table DDL helper.
- [x] Split SELECT materialized group finalization and DISTINCT probing into a
  focused materialization helper.
- [x] Split joined SELECT materialization into a focused SELECT companion
  module.
- [x] Split outer JOIN materialization range-rowset processing into a focused
  SELECT join companion.
- [x] Split single-table ordered and unordered SELECT materialization into a
  focused SELECT companion module.
- [x] Split `mylite_select_materialize.c` into single-table, joined-table,
  aggregate, and materialization-driver modules.
- [x] Split SELECT value comparison into a focused SELECT comparison module.
- [x] Split scalar SELECT `ORDER BY` validation into a focused SELECT scalar
  companion module.
- [x] Split scalar SELECT expression evaluation, scalar aggregate handling, and
  scalar expression callback adapters out of the statement-copy module.
- [x] Split SELECT `LIMIT` parsing and row-retention helpers out of the common
  SELECT utility module.
- [x] Split SELECT column-reference resolution, reference display-name copying,
  and alias copying out of the common SELECT utility module.
- [x] Split SELECT expression-evaluation context callbacks and output-value
  evaluation into a focused SELECT eval companion.
- [x] Split SELECT wildcard projection expansion and JOIN/USING output ordering
  into a focused SELECT projection wildcard companion.
- [x] Split SELECT FROM table resolution, alias validation, and column loading
  into a focused FROM resolve companion.
- [x] Split joined SELECT row output processing into a focused SELECT join
  output companion.
- [x] Split SELECT materialized row ORDER BY sorting into a focused rowset sort
  companion.
- [x] Split SELECT rowset DISTINCT probing and distinct-value comparison into a
  focused rowset distinct companion.
- [x] Split COUNT(DISTINCT ...) aggregate argument binding and descriptor
  inference into a focused SELECT aggregate companion.
- [x] Split SELECT aggregate-call registration and aggregate binding collection
  into a focused aggregate bind companion.
- [x] Split SELECT catalog descriptor row-source parsing into a focused
  descriptor source companion.
- [x] Split SELECT catalog descriptor type inference into a focused descriptor
  type companion.
- [x] Split SELECT plan lifetime and vector mutation into a focused plan
  companion.
- [x] Split SELECT table target resolution into a focused target companion.
- [x] Split SELECT JOIN/USING range lookup into a focused USING range
  companion.
- [x] Split SELECT ONLY_FULL_GROUP_BY invariance checks into a focused group
  invariant companion.
- [x] Split SELECT subquery MySQL diagnostics into a focused subquery
  diagnostics companion.
- [x] Split SELECT subquery outer-reference detection into a focused subquery
  validation companion.
- [x] Split `CHAR()` string descriptor inference into a focused expression
  descriptor module.
- [x] Split string encoding descriptor inference into a focused expression
  descriptor module.
- [x] Split `QUOTE()` string descriptor length inference into a focused
  expression descriptor module.
- [x] Split shared SHOW CREATE identifier quoting, string literal quoting, and
  SQLite bind lifetime helpers out of the mixed table/schema module.
- [x] Split SHOW CREATE TABLE target resolution, catalog reads, column/index
  rendering, and table options into a dedicated table renderer module.
- [x] Split SHOW CREATE TABLE target copying and validation into a focused
  target companion.
- [x] Split SHOW CREATE TABLE catalog metadata reads into a focused info
  companion.
- [x] Split SHOW CREATE TABLE table option rendering into a focused options
  companion.
- [x] Split SHOW CREATE TABLE column rendering into a focused columns
  companion.
- [x] Split SHOW CREATE TABLE index rendering into a focused indexes
  companion.
- [x] Split SHOW INDEX target copying and validation into a focused target
  companion.
- [x] Split SHOW CHARACTER SET and SHOW COLLATION SQL builders into a focused
  SHOW charset/collation companion.
- [x] Split SHOW STATUS SQL builders and uptime projection into a focused SHOW
  status companion.
- [x] Split SHOW VARIABLES SQL builders and session/global variable projection
  into a focused SHOW variables companion.
- [x] Split SHOW diagnostics SQL builders and warning/error condition rendering
  into a focused SHOW diagnostics companion.
- [x] Split dynamic INFORMATION_SCHEMA SQL builders into a focused companion
  module.
- [x] Split INFORMATION_SCHEMA SELECT target detection into a focused companion
  module.
- [x] Split schema catalog defaults, existence checks, mutations, and system
  schema seeding into a focused catalog schema module.
- [x] Split catalog physical table-name encoding into a focused catalog
  companion module.
- [x] Split generated CREATE TABLE index-name assignment into a focused table
  DDL index-name module.
- [x] Split standalone CREATE INDEX warning emission into a focused table DDL
  index warnings module.
- [x] Split ALTER TABLE refreshed index/column metadata into a focused table
  DDL alter metadata module.
- [x] Split ALTER TABLE MySQL-compatible diagnostics into a focused table DDL
  alter diagnostics module.
- [x] Split ALTER TABLE column action mutation into a focused table DDL alter
  column module.
- [x] Split ALTER TABLE column definition-to-model conversion into a focused
  table DDL alter column definition module.
- [x] Split ALTER TABLE AST-to-plan copying into a focused table DDL alter copy
  module.
- [x] Split ALTER TABLE added-index validation and primary-key diagnostics into
  a focused table DDL alter index validation module.
- [x] Split UNION global ORDER BY binding into a focused select union order
  module.
- [x] Split UNION global ORDER BY runtime value evaluation into a focused select
  union order evaluation module.
- [x] Split numeric variadic descriptor inference into a focused expression
  descriptor numeric variadic module.
- [x] Split `FORMAT()` descriptor length inference into a focused expression
  descriptor numeric format module.
- [x] Split `TIME()` descriptor inference and fractional-second deduction into
  a focused expression descriptor temporal time module.
- [x] Split UNION field descriptor merge rules into a focused expression
  descriptor union companion.
- [x] Split UPDATE expression binding into a focused DML update expression bind
  module.
- [x] Split INSERT duplicate-key update assignment validation into a focused
  DML insert update validation module.
- [x] Split INSERT duplicate-key update AST copying into a focused DML insert
  duplicate-update copy module.
- [x] Split INSERT duplicate-entry diagnostics out of unique-index conflict
  probing.
- [x] Split INSERT/REPLACE transaction finish bookkeeping into a focused DML
  insert transaction finish module.
- [x] Split expression function collation inference into a focused expression
  collation companion.
- [x] Split expression literal, identifier, and CAST collation inference into a
  focused expression collation leaf companion.
- [x] Split `STRCMP()` evaluation and collation-aware comparison into a focused
  statement function module.
- [x] Split transaction savepoint parsing, execution, stack management, and
  savepoint diagnostics out of the core transaction module.
- [x] Split transaction auto-increment catalog update and rollback bookkeeping
  into a focused transaction companion.
- [x] Split statement prepare custom-kind mapping into a focused prepare
  companion.
- [ ] Split `mylite_dml_insert_value_resolve.c` into insert column-list,
  positional/default, `INSERT ... SET`, and SQLite-bind modules.
- [ ] Split `mylite_select_subquery_eval.c` into scalar subquery, quantified
  subquery, row-value comparison, and diagnostics modules.
- [ ] Shrink broad type headers after the implementation modules above have
  narrower owners.
- [ ] Keep `mylite_select_context.c` composition-only; split callback groups if
  it starts accumulating SELECT behavior.
- [ ] Keep `mylite_statement_prepare.c` a switchboard; move any new family
  validation, cloning, or metadata inference into family modules.

## Rules

- No compatibility behavior changes in modularization commits.
- One moved concern per commit.
- Keep public ABI stable.
- Keep private dependencies acyclic: connection and diagnostics at the bottom,
  catalog above connection, statement dispatch above statement-family modules.
- Prefer narrow private functions over exposing mutable fields.
- If a moved module needs broad access to `mylite_stmt`, stop and split the
  relevant plan/result structures before moving more behavior.
