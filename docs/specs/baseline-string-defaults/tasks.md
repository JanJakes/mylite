# Baseline String Defaults Tasks

## Design

- [x] Read project architecture, compatibility, parser, runtime, storage, and
  SQLite fork guidance relevant to string defaults.
- [x] Verify MySQL 8.4.9 behavior for `CHAR`/`VARCHAR` literal defaults,
  metadata, DML materialization, `ALTER` default changes, cloning/copying,
  overlength diagnostics, and deferred `TEXT` literal/expression defaults.
- [x] Specify the independently authored MyLite grammar, semantics, storage
  mapping, diagnostics, compatibility gaps, performance boundary, and test
  plan.

## Implementation

- [x] Allow explicit literal defaults for `CHAR` and `VARCHAR` descriptors
  while preserving `TEXT` family literal-default rejection.
- [x] Reuse MyLite-owned string literal decoding, UTF-8/NUL validation,
  `CHAR` canonicalization, `VARCHAR` length validation, and descriptor text
  ownership for default conversion.
- [x] Keep generated SQLite `CREATE TABLE` descriptor-driven without physical
  default clauses, and ensure generated SQLite `ALTER TABLE ... ADD COLUMN`
  default literals are quoted safely.
- [x] Preserve descriptor-driven DML default materialization for omitted-column
  `INSERT`, explicit `DEFAULT`, `REPLACE`, and single-assignment `UPDATE`.
- [x] Update `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`,
  `SHOW CREATE TABLE`, and limited `INFORMATION_SCHEMA.COLUMNS` rendering for
  explicit `CHAR`/`VARCHAR` defaults.
- [x] Preserve `CREATE TABLE ... LIKE`, compatible `CREATE TABLE ... SELECT`,
  reopen persistence, file-format preamble invariants, and independent handle
  behavior.

## Tests and Docs

- [x] Add the MySQL 8.4.9 expectation script for the feature surface.
- [x] Add focused C runtime tests and parser regressions for supported and
  rejected behavior.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [x] Run focused build/tests, the new MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, catalog authority,
  string default conversion, generated SQL quoting, compatibility claims,
  performance, and file-format safety.
