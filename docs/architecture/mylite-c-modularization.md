# `mylite.c` Modularization

`packages/libmylite/src/mylite.c` has become the runtime integration point for
connection state, statement plans, catalog SQL, DDL, DML, SELECT execution,
metadata, diagnostics, and utility code. The direction is correct for MySQL
compatibility, but the implementation needs smaller private modules before more
runtime surface is added.

This refactor must preserve behavior. Move code behind private interfaces first,
then split statement families one at a time with the existing test suite green
after every step.

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
- `src/runtime/mylite_table_ddl.{h,c}`
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

## Task List

- [x] Document the intended runtime module layout and migration order.
- [x] Move shared runtime structs and enums from `mylite.c` into
  `src/runtime/mylite_runtime.h`.
- [ ] Move immutable runtime constants from `mylite.c` into focused private
  modules without creating unused-header warnings.
- [x] Extract diagnostics into `src/runtime/mylite_diagnostics.{h,c}`.
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
- [ ] Extract public statement lifecycle and result accessors into
  `src/runtime/mylite_statement.{h,c}` while keeping statement-family execution
  in `mylite.c`.
- [x] Start `src/runtime/mylite_statement.{h,c}` with affected-row access and
  row-count bookkeeping.
- [x] Move table-select row/current-result cleanup into
  `src/runtime/mylite_statement`.
- [x] Move scalar SELECT result and select-constant cleanup into
  `src/runtime/mylite_statement`.
- [x] Extract public result metadata accessors into
  `src/runtime/mylite_metadata.{h,c}`.
- [ ] Split catalog lookup helpers into `src/runtime/mylite_catalog`.
- [ ] Split `SHOW` and `information_schema` dynamic result builders.
- [ ] Split schema lifecycle and table/index DDL plans and execution.
- [ ] Split DML plans and execution.
- [ ] Split SELECT, UNION, aggregate, and subquery planning/execution.
- [ ] Move reusable field descriptor and metadata inference code into
  `src/runtime/mylite_metadata`.
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
