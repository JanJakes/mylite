# Baseline Unique Index Lifecycle Tasks

## Design

- [x] Read project architecture, compatibility, parser, catalog, runtime,
  primary-key, secondary-index, DML, metadata, storage, and SQLite integration
  guidance.
- [x] Verify MySQL 8.4.9 behavior for table-level and inline unique indexes,
  nullable unique semantics, duplicate diagnostics, metadata rendering, index
  naming, and deferred forms.
- [x] Specify the independently authored MyLite grammar, semantics, descriptor
  model, physical SQLite strategy, diagnostics, compatibility gaps, and test
  plan.

## Implementation

- [x] Extend parser and AST support for admitted inline and table-level unique
  index definitions.
- [x] Extend create-table planning to preserve uniqueness on descriptor-owned
  secondary index plans and reject unsupported unique forms deterministically.
- [x] Generate physical SQLite unique indexes from descriptor-owned stable
  physical names while preserving catalog/physical atomicity.
- [x] Clone supported unique index descriptors for `CREATE TABLE ... LIKE` and
  keep `CREATE TABLE ... SELECT` key-free.
- [x] Enforce admitted unique indexes for `INSERT`, `INSERT IGNORE`, and
  single-assignment `UPDATE`, including duplicate `NULL` behavior and
  MySQL-compatible duplicate diagnostics.
- [x] Reject key-bearing targets for deferred `INSERT ... SELECT` and
  `REPLACE` paths unless the implementation explicitly supports them.
- [x] Render unique index metadata in `SHOW COLUMNS`, `SHOW CREATE TABLE`,
  `SHOW INDEX`, and limited `INFORMATION_SCHEMA.STATISTICS`.
- [x] Preserve file-format preamble, descriptor authority, reopen persistence,
  independent handles, table rename/drop/truncate behavior, rebuild-ALTER
  rejection, and cleanup-on-failure behavior.

## Tests and Docs

- [x] Add the MySQL 8.4.9 expectation script for the feature surface.
- [x] Add focused C parser/runtime tests for supported and rejected behavior.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [x] Run focused build/tests, the new MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, descriptor authority,
  duplicate-key semantics, metadata claims, performance, and file-format safety.
