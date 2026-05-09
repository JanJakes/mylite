# Baseline Integer SIGNED Attribute Tasks

## Design And Evidence

- [x] Verify `packages/libmylite/tests/mysql_baseline_integer_signed_attribute_expectations.sh`
      against MySQL 8.4.9.
- [x] Keep the feature scope limited to one explicit `SIGNED` attribute after
      supported integer-family column types.
- [x] Confirm no public API, SQLite fork patch, compact storage, or file-format
      change is required.

## Parser And AST

- [x] Map the `SIGNED` keyword to a parser token only where the feature grammar
      admits it.
- [x] Extend `integer_type` grammar for `TINYINT`, `SMALLINT`, `MEDIUMINT`,
      `INT`, `INTEGER`, and `BIGINT` followed by one `SIGNED`.
- [x] Preserve the existing signed descriptor AST payload (`is_unsigned == 0`)
      for admitted `SIGNED` forms.
- [x] Preserve deterministic rejection for display width, `ZEROFILL`, combined
      `SIGNED`/`UNSIGNED`, repeated attributes, aliases, and unsupported
      numeric types.
- [x] Add parser tests for `CREATE TABLE`, `ALTER TABLE ADD`, `MODIFY`, and
      `CHANGE` forms using explicit `SIGNED`.

## Runtime And Catalog

- [x] Ensure descriptor mapping for explicit `SIGNED` matches the corresponding
      bare signed integer family.
- [x] Ensure `CREATE TABLE`, `ALTER TABLE ADD`, `MODIFY`, and `CHANGE` store
      normalized signed descriptors and physical `INTEGER` storage.
- [x] Ensure `INSERT ... VALUES`, `INSERT ... SET`, and `UPDATE` assignment
      conversion uses the existing signed descriptor ranges.
- [x] Ensure supported `SELECT`, `DELETE`, and `UPDATE` predicates work over
      columns declared with explicit `SIGNED`.
- [x] Ensure `ALTER TABLE ... MODIFY` / `CHANGE` with explicit `SIGNED`
      validates existing rows and treats same-definition changes as no-ops
      under the current descriptor model.

## Introspection

- [x] Render `SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` lower-case type
      text without the word `signed`.
- [x] Render `SHOW CREATE TABLE` lower-case type text without the word
      `signed`.
- [x] Preserve unknown future descriptor diagnostics.

## Tests

- [x] Add focused runtime coverage for explicit `SIGNED` descriptors, physical
      storage, introspection, DML, predicates, alters, persistence, and
      preamble preservation.
- [x] Cover successful lower-bound signed values and out-of-range diagnostics
      for all supported signed integer families.
- [x] Cover deferred MySQL-accepted syntax combinations as unsupported in
      MyLite: display width, `ZEROFILL`, repeated attributes,
      `SIGNED UNSIGNED`, `UNSIGNED SIGNED`, aliases, and unsupported numeric
      types.
- [x] No new test binary was needed; focused runtime coverage extends the
      existing integer-family runtime test.

## Compatibility Docs

- [x] Update `COMPATIBILITY.md`.
- [x] Update `docs/compatibility/sql-table-ddl.md`.
- [x] Update `docs/compatibility/type-system-literals-conversion.md` if needed
      for integer-family wording.
- [x] Avoid claiming display width, `ZEROFILL`, combined/repeated attributes,
      aliases, compact storage, protocol metadata, casts, or expression
      semantics.

## Verification

- [x] `cmake --build --preset dev`
- [x] Focused parser/runtime CTest entries touched by this feature.
- [x] `./packages/libmylite/tests/mysql_baseline_integer_signed_attribute_expectations.sh`
- [x] `cmake --workflow --preset check`
- [x] Final architecture/self-review for descriptor authority, range
      correctness, introspection normalization, docs accuracy, and test
      relevance.
