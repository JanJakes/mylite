# Baseline Boolean Literals Tasks

## Design And Evidence

- [x] Verify
      `packages/libmylite/tests/mysql_baseline_boolean_literals_expectations.sh`
      against MySQL 8.4.9.
- [x] Keep the feature scope limited to `TRUE` / `FALSE` in existing
      descriptor-backed integer DML value and predicate positions.
- [x] Confirm no public API, SQLite fork patch, compact storage, or file-format
      change is required.

## Parser And AST

- [ ] Add grammar support for `TRUE` and `FALSE` in `insert_value`,
      `update_value`, and comparison predicate right operands.
- [ ] Keep `limit_integer` restricted to unsigned decimal integer literals.
- [ ] Preserve deterministic rejection for unary boolean literals, expression
      assignments, `WHERE TRUE`, `IS TRUE`, and broader boolean expressions.
- [ ] Add parser tests for accepted and rejected boolean-literal forms.

## Runtime

- [ ] Convert `TRUE` to planned integer `1` and `FALSE` to planned integer `0`
      before descriptor range validation and SQLite binding.
- [ ] Reuse existing integer descriptor conversion, nullability behavior,
      predicate conversion, ordering, affected-row semantics, and warning
      behavior.
- [ ] Preserve descriptor authority, quoted generated identifiers, bound
      SQLite parameters, rowid-limited DML shapes, and file-format invariants.
- [ ] Keep catalog descriptors, catalog generation, and SQLite schema
      generation unchanged for DML statements.

## Tests

- [ ] Cover `INSERT ... VALUES` and `INSERT ... SET` with `TRUE` and `FALSE`
      across integer and `BOOL` / `BOOLEAN` descriptors.
- [ ] Cover `UPDATE` assignments to `TRUE` and `FALSE`, no-op changed-row
      reporting, and `FALSE` into `NOT NULL` columns.
- [ ] Cover `SELECT`, `DELETE`, and `UPDATE` predicates using `TRUE` and
      `FALSE` for the existing comparison operators.
- [ ] Cover case-insensitive keyword spelling through runtime tests.
- [ ] Cover unsupported shapes including unary boolean literals, expression
      assignments, `WHERE TRUE`, `IS TRUE`, and boolean literal limits.
- [ ] Cover persistence, independent handles, preamble preservation, and
      unchanged catalog/schema generations around supported DML.
- [ ] Register any new test binary in `packages/libmylite/CMakeLists.txt` if a
      new binary is needed.

## Compatibility Docs

- [ ] Update `COMPATIBILITY.md`.
- [ ] Update `docs/compatibility/type-system-literals-conversion.md`.
- [ ] Update `docs/compatibility/sql-table-dml.md`.
- [ ] Update `docs/compatibility/sql-query-expressions.md`.
- [ ] Update `docs/compatibility/operators.md`.
- [ ] Avoid claiming full boolean expression semantics, `IS TRUE`,
      boolean operators, boolean literal limits, protocol metadata, or broader
      type conversion.

## Verification

- [ ] `cmake --build --preset dev`
- [ ] Focused parser/runtime CTest entries touched by this feature.
- [ ] `./packages/libmylite/tests/mysql_baseline_boolean_literals_expectations.sh`
- [ ] `cmake --workflow --preset check`
- [ ] Final architecture/self-review for scope control, MySQL evidence,
      descriptor authority, conversion correctness, docs accuracy, and test
      relevance.
