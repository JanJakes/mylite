# Baseline CHAR Type Tasks

## Design

- [x] Read project architecture, compatibility, parser, runtime, storage, and
  SQLite fork guidance relevant to fixed-length character strings.
- [x] Verify MySQL 8.4.9 behavior for `CHAR` length defaults and bounds,
  metadata, row values, trailing-space handling, strict overlength failures,
  `IGNORE` adjustments, `NULL`, omitted defaults, and string literal forms.
- [x] Specify the independently authored MyLite grammar, semantics, storage
  mapping, diagnostics, compatibility gaps, and test plan.

## Implementation

- [x] Extend parser and AST support for `CHAR` and `CHAR(length)` column types.
- [x] Map admitted `CHAR(0..255)` descriptors to logical `CHAR(n)` and
  physical SQLite `TEXT`, with bare `CHAR` normalized to `CHAR(1)`.
- [x] Add MyLite-owned `CHAR` string conversion, UTF-8/NUL/length validation,
  default-mode trailing-space canonicalization, cleanup, and SQLite text
  binding.
- [x] Add `CHAR` support for `INSERT`, `REPLACE`, `UPDATE`, compatible
  `INSERT ... SELECT`, compatible `CREATE TABLE ... SELECT`, plain `SELECT`
  readback, and null-test predicates.
- [x] Reject unsupported `CHAR` conversions, defaults, ordering, distinct,
  grouped, aggregate, and collation-sensitive predicate forms deterministically.
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
  text ownership, descriptor-driven SQLite SQL, compatibility claims,
  performance, and file-format safety.
