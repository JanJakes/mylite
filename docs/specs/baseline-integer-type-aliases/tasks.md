# Baseline Integer Type Aliases Tasks

## Design And Evidence

- [x] Verify `packages/libmylite/tests/mysql_baseline_integer_type_aliases_expectations.sh`
      against MySQL 8.4.9.
- [x] Keep the feature scope limited to `INT1`, `INT2`, `INT3`, `INT4`, and
      `INT8`, each optionally followed by one `SIGNED` or `UNSIGNED`.
- [x] Confirm no public API, SQLite fork patch, compact storage, or file-format
      change is required.

## Parser And AST

- [ ] Map `INT1`, `INT2`, `INT3`, `INT4`, and `INT8` keywords to parser tokens.
- [ ] Extend `integer_type` grammar for bare, `SIGNED`, and `UNSIGNED` alias
      forms.
- [ ] Normalize aliases to existing integer-family AST payloads.
- [ ] Preserve deterministic rejection for display width, `ZEROFILL`, combined
      `SIGNED`/`UNSIGNED`, repeated attributes, `BOOL`, `BOOLEAN`, `SERIAL`,
      and unsupported aliases.
- [ ] Add parser tests for `CREATE TABLE`, `ALTER TABLE ADD`, `MODIFY`, and
      `CHANGE` forms using aliases.

## Runtime And Catalog

- [ ] Ensure descriptor mapping for aliases matches the corresponding
      normalized integer family.
- [ ] Ensure `CREATE TABLE`, `ALTER TABLE ADD`, `MODIFY`, and `CHANGE` store
      normalized descriptors and physical `INTEGER` storage.
- [ ] Ensure `INSERT ... VALUES`, `INSERT ... SET`, and `UPDATE` assignment
      conversion uses normalized descriptor ranges.
- [ ] Ensure supported `SELECT`, `DELETE`, and `UPDATE` predicates work over
      alias-declared columns.
- [ ] Ensure `ALTER TABLE ... MODIFY` / `CHANGE` with aliases validates
      existing rows and treats same-definition changes as no-ops under the
      current descriptor model.

## Introspection

- [ ] Render `SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` lower-case
      normalized type text.
- [ ] Render `SHOW CREATE TABLE` lower-case normalized type text.
- [ ] Preserve unknown future descriptor diagnostics.

## Tests

- [ ] Add focused runtime coverage for alias descriptors, physical storage,
      introspection, DML, predicates, alters, persistence, and preamble
      preservation.
- [ ] Cover successful normalized range values and out-of-range diagnostics for
      all alias families.
- [ ] Cover deferred MySQL-accepted syntax combinations as unsupported in
      MyLite: display width, `ZEROFILL`, `BOOL`, `BOOLEAN`, `SERIAL`, repeated
      attributes, and combined `SIGNED`/`UNSIGNED`.
- [ ] Register any new test binary in `packages/libmylite/CMakeLists.txt` if a
      new binary is needed.

## Compatibility Docs

- [ ] Update `COMPATIBILITY.md`.
- [ ] Update `docs/compatibility/sql-table-ddl.md`.
- [ ] Update `docs/compatibility/type-system-literals-conversion.md`.
- [ ] Avoid claiming `BOOL`, `BOOLEAN`, `SERIAL`, display width, `ZEROFILL`,
      combined/repeated attributes, compact storage, protocol metadata, casts,
      or expression semantics.

## Verification

- [ ] `cmake --build --preset dev`
- [ ] Focused parser/runtime CTest entries touched by this feature.
- [ ] `./packages/libmylite/tests/mysql_baseline_integer_type_aliases_expectations.sh`
- [ ] `cmake --workflow --preset check`
- [ ] Final architecture/self-review for descriptor authority, range
      correctness, introspection normalization, docs accuracy, and test
      relevance.
