# Baseline Integer SIGNED Attribute Tasks

## Design And Evidence

- [x] Verify `packages/libmylite/tests/mysql_baseline_integer_signed_attribute_expectations.sh`
      against MySQL 8.4.9.
- [x] Keep the feature scope limited to one explicit `SIGNED` attribute after
      supported integer-family column types.
- [x] Confirm no public API, SQLite fork patch, compact storage, or file-format
      change is required.

## Parser And AST

- [ ] Map the `SIGNED` keyword to a parser token only where the feature grammar
      admits it.
- [ ] Extend `integer_type` grammar for `TINYINT`, `SMALLINT`, `MEDIUMINT`,
      `INT`, `INTEGER`, and `BIGINT` followed by one `SIGNED`.
- [ ] Preserve the existing signed descriptor AST payload (`is_unsigned == 0`)
      for admitted `SIGNED` forms.
- [ ] Preserve deterministic rejection for display width, `ZEROFILL`, combined
      `SIGNED`/`UNSIGNED`, repeated attributes, aliases, and unsupported
      numeric types.
- [ ] Add parser tests for `CREATE TABLE`, `ALTER TABLE ADD`, `MODIFY`, and
      `CHANGE` forms using explicit `SIGNED`.

## Runtime And Catalog

- [ ] Ensure descriptor mapping for explicit `SIGNED` matches the corresponding
      bare signed integer family.
- [ ] Ensure `CREATE TABLE`, `ALTER TABLE ADD`, `MODIFY`, and `CHANGE` store
      normalized signed descriptors and physical `INTEGER` storage.
- [ ] Ensure `INSERT ... VALUES`, `INSERT ... SET`, and `UPDATE` assignment
      conversion uses the existing signed descriptor ranges.
- [ ] Ensure supported `SELECT`, `DELETE`, and `UPDATE` predicates work over
      columns declared with explicit `SIGNED`.
- [ ] Ensure `ALTER TABLE ... MODIFY` / `CHANGE` with explicit `SIGNED`
      validates existing rows and treats same-definition changes as no-ops
      under the current descriptor model.

## Introspection

- [ ] Render `SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` lower-case type
      text without the word `signed`.
- [ ] Render `SHOW CREATE TABLE` lower-case type text without the word
      `signed`.
- [ ] Preserve unknown future descriptor diagnostics.

## Tests

- [ ] Add focused runtime coverage for explicit `SIGNED` descriptors, physical
      storage, introspection, DML, predicates, alters, persistence, and
      preamble preservation.
- [ ] Cover successful lower-bound signed values and out-of-range diagnostics
      for all supported signed integer families.
- [ ] Cover deferred MySQL-accepted syntax combinations as unsupported in
      MyLite: display width, `ZEROFILL`, repeated attributes,
      `SIGNED UNSIGNED`, `UNSIGNED SIGNED`, aliases, and unsupported numeric
      types.
- [ ] Register any new test binary in `packages/libmylite/CMakeLists.txt`.

## Compatibility Docs

- [ ] Update `COMPATIBILITY.md`.
- [ ] Update `docs/compatibility/sql-table-ddl.md`.
- [ ] Update `docs/compatibility/type-system-literals-conversion.md` if needed
      for integer-family wording.
- [ ] Avoid claiming display width, `ZEROFILL`, combined/repeated attributes,
      aliases, compact storage, protocol metadata, casts, or expression
      semantics.

## Verification

- [ ] `cmake --build --preset dev`
- [ ] Focused parser/runtime CTest entries touched by this feature.
- [ ] `./packages/libmylite/tests/mysql_baseline_integer_signed_attribute_expectations.sh`
- [ ] `cmake --workflow --preset check`
- [ ] Final architecture/self-review for descriptor authority, range
      correctness, introspection normalization, docs accuracy, and test
      relevance.
