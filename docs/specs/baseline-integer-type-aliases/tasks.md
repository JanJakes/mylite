# Baseline Integer Type Aliases Tasks

## Design And Evidence

- [x] Verify `packages/libmylite/tests/mysql_baseline_integer_type_aliases_expectations.sh`
      against MySQL 8.4.9.
- [x] Keep the feature scope limited to `INT1`, `INT2`, `INT3`, `INT4`, and
      `INT8`, each optionally followed by one `SIGNED` or `UNSIGNED`.
- [x] Confirm no public API, SQLite fork patch, compact storage, or file-format
      change is required.

## Parser And AST

- [x] Map `INT1`, `INT2`, `INT3`, `INT4`, and `INT8` keywords to parser tokens.
- [x] Extend `integer_type` grammar for bare, `SIGNED`, and `UNSIGNED` alias
      forms.
- [x] Normalize aliases to existing integer-family AST payloads.
- [x] Preserve deterministic rejection for display width, `ZEROFILL`, combined
      `SIGNED`/`UNSIGNED`, repeated attributes, `BOOL`, `BOOLEAN`, `SERIAL`,
      and unsupported aliases.
- [x] Add parser tests for `CREATE TABLE`, `ALTER TABLE ADD`, `MODIFY`, and
      `CHANGE` forms using aliases.

## Runtime And Catalog

- [x] Ensure descriptor mapping for aliases matches the corresponding
      normalized integer family.
- [x] Ensure `CREATE TABLE`, `ALTER TABLE ADD`, `MODIFY`, and `CHANGE` store
      normalized descriptors and physical `INTEGER` storage.
- [x] Ensure `INSERT ... VALUES`, `INSERT ... SET`, and `UPDATE` assignment
      conversion uses normalized descriptor ranges.
- [x] Ensure supported `SELECT`, `DELETE`, and `UPDATE` predicates work over
      alias-declared columns.
- [x] Ensure `ALTER TABLE ... MODIFY` / `CHANGE` with aliases validates
      existing rows and treats same-definition changes as no-ops under the
      current descriptor model.

## Introspection

- [x] Render `SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` lower-case
      normalized type text.
- [x] Render `SHOW CREATE TABLE` lower-case normalized type text.
- [x] Preserve unknown future descriptor diagnostics.

## Tests

- [x] Add focused runtime coverage for alias descriptors, physical storage,
      introspection, DML, predicates, alters, persistence, and preamble
      preservation.
- [x] Cover successful normalized range values and out-of-range diagnostics for
      all alias families.
- [x] Cover deferred MySQL-accepted syntax combinations as unsupported in
      MyLite: display width, `ZEROFILL`, `BOOL`, `BOOLEAN`, `SERIAL`, repeated
      attributes, and combined `SIGNED`/`UNSIGNED`.
- [x] No new test binary was needed; coverage extends existing parser and
      small-integer runtime tests.

## Compatibility Docs

- [x] Update `COMPATIBILITY.md`.
- [x] Update `docs/compatibility/sql-table-ddl.md`.
- [x] Update `docs/compatibility/type-system-literals-conversion.md`.
- [x] Avoid claiming `BOOL`, `BOOLEAN`, `SERIAL`, display width, `ZEROFILL`,
      combined/repeated attributes, compact storage, protocol metadata, casts,
      or expression semantics.

## Verification

- [x] `cmake --build --preset dev`
- [x] Focused parser/runtime CTest entries touched by this feature.
- [x] `./packages/libmylite/tests/mysql_baseline_integer_type_aliases_expectations.sh`
- [x] `cmake --workflow --preset check`
- [x] Final architecture/self-review for descriptor authority, range
      correctness, introspection normalization, docs accuracy, and test
      relevance.
