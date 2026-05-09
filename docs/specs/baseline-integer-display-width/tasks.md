# Baseline Integer Display Width Tasks

## Design And Evidence

- [x] Verify
      `packages/libmylite/tests/mysql_baseline_integer_display_width_expectations.sh`
      against MySQL 8.4.9.
- [x] Keep the feature scope limited to integer display width on already
      supported integer families and aliases.
- [x] Confirm no public API, SQLite fork patch, compact storage, or file-format
      change is required.

## Parser And AST

- [ ] Extend `integer_type` grammar to accept `(<unsigned decimal width>)`
      before optional `SIGNED` or `UNSIGNED`.
- [ ] Capture display-width source spans in the AST.
- [ ] Preserve deterministic rejection for signed, empty, decimal, string, hex,
      bit, parameter, expression, and post-signedness width forms.
- [ ] Preserve deterministic rejection for `ZEROFILL`.
- [ ] Add parser tests for `CREATE TABLE`, `ALTER TABLE ADD`, `MODIFY`, and
      `CHANGE` display-width forms.

## Runtime And Catalog

- [ ] Validate width values in `0..255` and emit MySQL-compatible error 1439
      for out-of-range width.
- [ ] Append one warning 1681 per accepted display width clause.
- [ ] Store signed `TINYINT(1)` / `INT1(1)` as durable `TINYINT(1)`
      descriptor text.
- [ ] Normalize all other admitted integer display widths to existing no-width
      descriptors.
- [ ] Treat `TINYINT(1)` as the same integer range and physical SQLite
      `INTEGER` storage as `TINYINT`.
- [ ] Keep display-width-only `ALTER TABLE ... MODIFY` / `CHANGE` changes
      metadata-only when name, nullability, physical type, and integer range
      are unchanged.

## Introspection

- [ ] Render `SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` with
      `tinyint(1)` only for persisted `TINYINT(1)` descriptors.
- [ ] Render `SHOW CREATE TABLE` with `tinyint(1)` only for persisted
      `TINYINT(1)` descriptors.
- [ ] Preserve unknown future descriptor diagnostics.

## Tests

- [ ] Add focused runtime coverage for create/alter display-width descriptors,
      warnings, physical storage, introspection, DML, predicates, persistence,
      and preamble preservation.
- [ ] Cover width `0`, width `1`, width `255`, and width `256` diagnostics.
- [ ] Cover `TINYINT(1)`, `TINYINT(1) SIGNED`, `TINYINT(1) UNSIGNED`, and
      `INT1(1)` normalization differences.
- [ ] Cover deferred `ZEROFILL` as unsupported in MyLite even though MySQL
      accepts it.
- [ ] Register any new test binary in `packages/libmylite/CMakeLists.txt` if a
      new binary is needed.

## Compatibility Docs

- [ ] Update `COMPATIBILITY.md`.
- [ ] Update `docs/compatibility/sql-table-ddl.md`.
- [ ] Update `docs/compatibility/type-system-literals-conversion.md`.
- [ ] Avoid claiming `ZEROFILL`, padding, protocol metadata, casts, expression
      type syntax, non-integer widths, compact storage, or changed ranges.

## Verification

- [ ] `cmake --build --preset dev`
- [ ] Focused parser/runtime CTest entries touched by this feature.
- [ ] `./packages/libmylite/tests/mysql_baseline_integer_display_width_expectations.sh`
- [ ] `cmake --workflow --preset check`
- [ ] Final architecture/self-review for warning semantics, descriptor
      authority, metadata-only alter behavior, docs accuracy, and test
      relevance.
