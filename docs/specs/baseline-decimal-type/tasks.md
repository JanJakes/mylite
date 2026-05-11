# Baseline DECIMAL Type Tasks

## Design

- [x] Read project architecture, compatibility, parser, runtime, catalog,
  storage, and SQLite fork guidance relevant to exact fixed-point row values.
- [x] Verify MySQL 8.4.9 behavior for decimal declaration forms, precision and
  scale bounds, aliases, unsigned deprecation warnings, defaults, DML
  conversion, rounding, range errors, `INSERT IGNORE` adjustment, metadata, and
  deliberately deferred forms.
- [x] Specify the independently authored MyLite grammar, semantics, storage
  mapping, catalog default extension, diagnostics, compatibility gaps, and test
  plan.

## Implementation

- [x] Extend parser and AST support for admitted decimal column types and
  decimal literal DML/default values.
- [x] Add durable catalog support for decimal default text without regressing
  existing integer defaults or older catalog migrations.
- [x] Map admitted decimal descriptors to logical `DECIMAL(M,D)` text and
  physical SQLite `TEXT`.
- [x] Add MyLite-owned exact decimal conversion, canonicalization, rounding,
  range checking, `INSERT IGNORE` clipping, null/default handling, and text
  binding.
- [x] Add decimal support for `CREATE TABLE`, `ALTER ADD COLUMN`,
  `ALTER COLUMN SET/DROP DEFAULT`, `INSERT`, `REPLACE`, `UPDATE`, compatible
  `INSERT ... SELECT`, compatible `REPLACE ... SELECT`, compatible
  `CREATE TABLE ... SELECT`, `CREATE TABLE ... LIKE`, plain `SELECT` readback,
  and null-test predicates.
- [x] Reject unsupported decimal comparisons, ordering, distinct, grouping,
  aggregates, primary keys, auto-increment, `ZEROFILL`, expression values,
  string/float/hex/bit conversions, and `ALTER MODIFY` / `CHANGE` replacement
  deterministically.
- [x] Update `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`,
  `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS` rendering.

## Tests and Docs

- [x] Add the MySQL 8.4.9 expectation script for the feature surface.
- [x] Add focused C runtime tests and parser tests for supported and rejected
  behavior.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [x] Run focused build/tests, the new MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, catalog authority,
  exact decimal conversion, descriptor-driven SQLite SQL, compatibility claims,
  performance, and file-format safety.
