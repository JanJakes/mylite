# Baseline YEAR Type Tasks

## Design

- [x] Read project architecture, compatibility, parser, runtime, storage,
  descriptor, temporal type, string type, BIT type, index, metadata, and
  SQLite integration guidance.
- [x] Verify MySQL 8.4.9 behavior for `YEAR`, `YEAR(4)`, defaults, conversion,
  nullability, `INSERT IGNORE`, predicates, ordering, `ALTER ADD`, secondary
  indexes, metadata, and deferred wider forms.
- [x] Specify the independently authored MyLite grammar, descriptor model,
  conversion rules, physical storage strategy, diagnostics, metadata,
  compatibility gaps, and test plan.

## Implementation

- [ ] Extend parser and AST support for `YEAR` and `YEAR(4)` column type nodes.
- [ ] Map `YEAR` descriptors to logical `YEAR` and stable physical row storage.
- [ ] Add MyLite-owned `YEAR` conversion for defaults, inserted values,
  replaced values, one-assignment updates, compatible descriptor copies, and
  limited predicates.
- [ ] Preserve MySQL-shaped warnings/errors for display-width deprecation,
  invalid display widths, invalid defaults, out-of-range values, bad strings,
  `NULL` into `NOT NULL`, and omitted no-default `NOT NULL`.
- [ ] Support `INSERT IGNORE` adjustment to `0000` for the verified warning
  cases.
- [ ] Render `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`,
  and limited `INFORMATION_SCHEMA.COLUMNS` metadata from descriptors.
- [ ] Admit supported unique and nonunique secondary-index declarations on
  `YEAR` descriptors while keeping primary keys and deferred key forms
  rejected.
- [ ] Preserve `CREATE TABLE ... LIKE`, `CREATE TABLE ... SELECT`,
  `INSERT ... SELECT`, `REPLACE ... SELECT`, table rename/drop, reopen
  persistence, independent handles, and `.mylite` preamble invariants.

## Tests and Docs

- [x] Add the MySQL 8.4.9 expectation script for supported behavior and
  deferred upstream forms.
- [ ] Add focused C parser/runtime tests for supported and rejected behavior.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [ ] Run focused parser/runtime tests and the new MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for MySQL behavior, descriptor authority,
  conversion correctness, metadata accuracy, file-format safety, performance,
  scope control, and test relevance.
