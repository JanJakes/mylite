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

- [x] Extend `integer_type` grammar to accept `(<unsigned decimal width>)`
      before optional `SIGNED` or `UNSIGNED`.
- [x] Capture display-width source spans in the AST.
- [x] Preserve deterministic rejection for signed, empty, decimal, string, hex,
      bit, parameter, expression, and post-signedness width forms.
- [x] Preserve deterministic rejection for `ZEROFILL`.
- [x] Add parser tests for `CREATE TABLE`, `ALTER TABLE ADD`, `MODIFY`, and
      `CHANGE` display-width forms.

## Runtime And Catalog

- [x] Validate width values in `0..255` and emit MySQL-compatible error 1439
      for out-of-range width.
- [x] Append one warning 1681 per accepted display width clause.
- [x] Store signed `TINYINT(1)` / `INT1(1)` as durable `TINYINT(1)`
      descriptor text.
- [x] Normalize all other admitted integer display widths to existing no-width
      descriptors.
- [x] Treat `TINYINT(1)` as the same integer range and physical SQLite
      `INTEGER` storage as `TINYINT`.
- [x] Keep display-width-only `ALTER TABLE ... MODIFY` / `CHANGE` changes
      metadata-only when name, nullability, physical type, and integer range
      are unchanged.

## Introspection

- [x] Render `SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` with
      `tinyint(1)` only for persisted `TINYINT(1)` descriptors.
- [x] Render `SHOW CREATE TABLE` with `tinyint(1)` only for persisted
      `TINYINT(1)` descriptors.
- [x] Preserve unknown future descriptor diagnostics.

## Tests

- [x] Add focused runtime coverage for create/alter display-width descriptors,
      warnings, physical storage, introspection, DML, predicates, persistence,
      and preamble preservation.
- [x] Cover width `0`, width `1`, width `255`, and width `256` diagnostics.
- [x] Cover `TINYINT(1)`, `TINYINT(1) SIGNED`, `TINYINT(1) UNSIGNED`, and
      `INT1(1)` normalization differences.
- [x] Cover deferred `ZEROFILL` as unsupported in MyLite even though MySQL
      accepts it.
- [x] Register any new test binary in `packages/libmylite/CMakeLists.txt` if a
      new binary is needed.

## Compatibility Docs

- [x] Update `COMPATIBILITY.md`.
- [x] Update `docs/compatibility/sql-table-ddl.md`.
- [x] Update `docs/compatibility/type-system-literals-conversion.md`.
- [x] Avoid claiming `ZEROFILL`, padding, protocol metadata, casts, expression
      type syntax, non-integer widths, compact storage, or changed ranges.

## Verification

- [x] `cmake --build --preset dev`
- [x] Focused parser/runtime CTest entries touched by this feature.
- [x] `./packages/libmylite/tests/mysql_baseline_integer_display_width_expectations.sh`
- [x] `cmake --workflow --preset check`
- [x] Final architecture/self-review for warning semantics, descriptor
      authority, metadata-only alter behavior, docs accuracy, and test
      relevance.
