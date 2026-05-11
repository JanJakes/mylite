# Baseline DATETIME Type Tasks

## Design

- [x] Read project architecture, compatibility, parser, runtime, catalog,
  storage, and SQLite fork guidance relevant to temporal row values.
- [x] Verify MySQL 8.4.9 behavior for datetime declaration, canonical values,
  invalid and zero datetimes under default SQL mode, defaults,
  `INSERT IGNORE` adjustment, predicates, ordering, metadata, and deliberately
  deferred forms.
- [x] Specify the independently authored MyLite grammar, semantics, storage
  mapping, diagnostics, compatibility gaps, performance boundary, and test
  plan.

## Implementation

- [ ] Extend parser and AST support for admitted bare `DATETIME` column types.
- [ ] Map admitted datetime descriptors to logical `DATETIME` text and physical
  SQLite `TEXT`.
- [ ] Add MyLite-owned canonical datetime conversion, validation, strict
  errors, `INSERT IGNORE` zero-datetime adjustment, null/default handling, and
  text binding.
- [ ] Add DATETIME support for `CREATE TABLE`, `ALTER ADD COLUMN`,
  `ALTER COLUMN SET/DROP DEFAULT`, `INSERT`, `REPLACE`, `UPDATE`, compatible
  `INSERT ... SELECT`, compatible `REPLACE ... SELECT`, compatible
  `CREATE TABLE ... SELECT`, `CREATE TABLE ... LIKE`, plain `SELECT` readback,
  comparison/range/membership/null predicates, and one-column ordering.
- [ ] Add DATETIME support for supported nonunique and unique secondary-index
  declaration, metadata, duplicate enforcement, update checks, and descriptor
  copying.
- [ ] Reject unsupported DATETIME fractional precision, relaxed/numeric/T
  separator conversion, expression values, temporal functions, temporal literal
  introducers, partial-zero direct values, primary keys, auto-increment,
  unsupported ordering expressions, and `ALTER MODIFY` / `CHANGE` replacement
  deterministically.
- [ ] Update `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`,
  `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS` rendering, including
  `DATETIME_PRECISION = 0`.

## Tests and Docs

- [ ] Add the MySQL 8.4.9 expectation script for the feature surface.
- [ ] Add focused C runtime tests and parser tests for supported and rejected
  behavior.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [ ] Run focused build/tests, the new MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, catalog authority,
  canonical datetime conversion, descriptor-driven SQLite SQL, compatibility
  claims, performance, and file-format safety.
