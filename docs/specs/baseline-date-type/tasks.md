# Baseline DATE Type Tasks

## Design

- [x] Read project architecture, compatibility, parser, runtime, catalog,
  storage, and SQLite fork guidance relevant to temporal row values.
- [x] Verify MySQL 8.4.9 behavior for date declaration, canonical values,
  invalid and zero dates under default SQL mode, defaults, `INSERT IGNORE`
  adjustment, predicates, ordering, metadata, and deliberately deferred forms.
- [x] Specify the independently authored MyLite grammar, semantics, storage
  mapping, catalog default extension, diagnostics, compatibility gaps, and
  test plan.

## Implementation

- [ ] Extend parser and AST support for admitted `DATE` column types and
  string default/predicate forms.
- [ ] Add durable catalog support for a generic text default kind without
  regressing existing integer and decimal defaults or catalog migrations.
- [ ] Map admitted date descriptors to logical `DATE` text and physical SQLite
  `TEXT`.
- [ ] Add MyLite-owned canonical date conversion, validation, strict errors,
  `INSERT IGNORE` zero-date adjustment, null/default handling, and text
  binding.
- [ ] Add DATE support for `CREATE TABLE`, `ALTER ADD COLUMN`,
  `ALTER COLUMN SET/DROP DEFAULT`, `INSERT`, `REPLACE`, `UPDATE`, compatible
  `INSERT ... SELECT`, compatible `REPLACE ... SELECT`, compatible
  `CREATE TABLE ... SELECT`, `CREATE TABLE ... LIKE`, plain `SELECT` readback,
  comparison/range/membership/null predicates, and one-column ordering.
- [ ] Reject unsupported DATE relaxed/numeric/date-time conversion,
  expression values, temporal functions, standard/ODBC temporal literals,
  partial-zero direct values, primary keys, auto-increment, unsupported
  ordering expressions, and `ALTER MODIFY` / `CHANGE` replacement
  deterministically.
- [ ] Update `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`,
  `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS` rendering.

## Tests and Docs

- [ ] Add the MySQL 8.4.9 expectation script for the feature surface.
- [ ] Add focused C runtime tests and parser tests for supported and rejected
  behavior.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [ ] Run focused build/tests, the new MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, catalog authority,
  canonical date conversion, descriptor-driven SQLite SQL, compatibility
  claims, performance, and file-format safety.

