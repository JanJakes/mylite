# Baseline TIMESTAMP Type Tasks

## Design

- [x] Read project architecture, compatibility, parser, runtime, catalog,
  storage, and SQLite fork guidance relevant to temporal row values.
- [x] Verify MySQL 8.4.9 behavior for timestamp declaration, fixed-UTC
  canonical values, invalid/zero/out-of-range timestamps under default SQL
  mode, defaults, `INSERT IGNORE` adjustment, predicates, ordering, metadata,
  and deliberately deferred wider timestamp forms.
- [x] Specify the independently authored MyLite grammar, semantics, storage
  mapping, diagnostics, compatibility gaps, performance boundary, and test
  plan.

## Implementation

- [x] Extend parser and AST support for admitted bare `TIMESTAMP` column types.
- [x] Map admitted timestamp descriptors to logical `TIMESTAMP` text and
  physical SQLite `TEXT`.
- [x] Add MyLite-owned canonical timestamp conversion, validation, strict
  errors, `INSERT IGNORE` zero-timestamp adjustment, `ALTER ADD TIMESTAMP NOT
  NULL` zero backfill, null/default handling, and text binding.
- [x] Add TIMESTAMP support for `CREATE TABLE`, `ALTER ADD COLUMN`,
  `ALTER COLUMN SET/DROP DEFAULT`, `INSERT`, `REPLACE`, `UPDATE`, compatible
  `INSERT ... SELECT`, compatible `REPLACE ... SELECT`, compatible
  `CREATE TABLE ... SELECT`, `CREATE TABLE ... LIKE`, plain `SELECT` readback,
  comparison/range/membership/null predicates, and one-column ordering.
- [x] Add TIMESTAMP support for supported nonunique and unique secondary-index
  declaration, metadata, duplicate enforcement, update checks, and descriptor
  copying.
- [x] Reject unsupported fractional precision, relaxed/numeric/T-separator
  conversion, time-zone-offset inputs, expression values, temporal functions,
  temporal literal introducers, direct zero values outside admitted adjustment
  paths, primary keys, auto-increment, unsupported ordering expressions, and
  unsupported `ALTER MODIFY` / `CHANGE` replacement deterministically.
- [x] Update `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`,
  `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS` rendering, including
  `DATETIME_PRECISION = 0`.

## Tests and Docs

- [x] Add the MySQL 8.4.9 expectation script for the feature surface.
- [x] Add focused C runtime tests and parser tests for supported and rejected
  behavior.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [x] Run focused build/tests, the new MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, catalog authority,
  canonical timestamp conversion, descriptor-driven SQLite SQL, compatibility
  claims, performance, and file-format safety.
