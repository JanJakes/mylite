# Baseline VARCHAR Type Tasks

## Design

- [x] Read project architecture, compatibility, parser, runtime, storage, and
  SQLite fork guidance relevant to the first string-storage slice.
- [x] Verify MySQL 8.4.9 behavior for `VARCHAR` bounds, metadata, row values,
  strict overlength failures, `IGNORE` adjustments, `NULL`, omitted defaults,
  and string literal forms.
- [x] Specify the independently authored MyLite grammar, semantics, storage
  mapping, diagnostics, compatibility gaps, and test plan.

## Implementation

- [ ] Extend parser and AST support for `VARCHAR(length)` column types and
  string DML values in admitted row-value positions.
- [ ] Map admitted `VARCHAR(0..255)` descriptors to logical `VARCHAR(n)` and
  physical SQLite `TEXT`.
- [ ] Add MyLite-owned string literal decoding, UTF-8/NUL/length validation,
  text value ownership, cleanup, and SQLite text binding.
- [ ] Update DDL generation and descriptor cloning/copying paths to use
  descriptor physical type text instead of hardcoded integer columns.
- [ ] Add string support for `INSERT`, `REPLACE`, `UPDATE`, compatible
  `INSERT ... SELECT`, compatible `CREATE TABLE ... SELECT`, plain `SELECT`
  readback, and null-test predicates.
- [ ] Reject unsupported string conversions, defaults, ordering, distinct,
  grouped, aggregate, and collation-sensitive predicate forms deterministically.
- [ ] Update `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, and
  `SHOW CREATE TABLE` rendering.

## Tests and Docs

- [x] Add the MySQL 8.4.9 expectation script for the feature surface.
- [ ] Add focused C runtime tests and parser tests for supported and rejected
  behavior.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [ ] Run focused build/tests, the new MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, catalog authority,
  text ownership, descriptor-driven SQLite SQL, compatibility claims,
  performance, and file-format safety.
