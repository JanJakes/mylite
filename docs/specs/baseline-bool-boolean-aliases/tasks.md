# Baseline BOOL And BOOLEAN Aliases Tasks

## Design And Evidence

- [x] Verify
      `packages/libmylite/tests/mysql_baseline_bool_boolean_aliases_expectations.sh`
      against MySQL 8.4.9.
- [x] Keep the feature scope limited to column-type aliases that normalize to
      the current signed `TINYINT(1)` descriptor.
- [x] Confirm no public API, SQLite fork patch, compact storage, or file-format
      change is required.

## Parser And AST

- [ ] Add parser tokens/rules for bare `BOOL` and `BOOLEAN` column types.
- [ ] Preserve deterministic rejection for explicit alias widths, signedness,
      `ZEROFILL`, repeated attributes, and combined attributes.
- [ ] Carry enough AST information to map aliases to `TINYINT(1)` without
      emitting display-width warnings.
- [ ] Add parser tests for `CREATE TABLE`, `ALTER TABLE ADD`, `MODIFY`, and
      `CHANGE` alias forms.

## Runtime And Catalog

- [ ] Map `BOOL` and `BOOLEAN` to durable `TINYINT(1)` descriptors.
- [ ] Reuse current signed `TINYINT(1)` value range, nullability, assignment,
      predicate, ordering, delete, and update behavior.
- [ ] Emit zero warnings for accepted alias definitions.
- [ ] Keep alias-only `ALTER TABLE ... MODIFY` / `CHANGE` changes
      metadata-only when name, nullability, physical type, and integer range
      are unchanged.

## Introspection

- [ ] Render `SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` with
      `tinyint(1)` for alias-backed descriptors.
- [ ] Render `SHOW CREATE TABLE` with `tinyint(1)` for alias-backed
      descriptors.
- [ ] Preserve unknown future descriptor diagnostics.

## Tests

- [ ] Add focused runtime coverage for create/alter alias descriptors,
      warnings, physical storage, introspection, DML, predicates, persistence,
      and preamble preservation.
- [ ] Cover signed `TINYINT(1)` range boundaries and out-of-range diagnostics.
- [ ] Cover `NULL` into nullable alias columns and deterministic diagnostics
      for `NULL` into `NOT NULL` alias columns.
- [ ] Cover deferred truth-expression semantics as intentionally unsupported in
      row-value and update-assignment inputs.
- [ ] Register any new test binary in `packages/libmylite/CMakeLists.txt` if a
      new binary is needed.

## Compatibility Docs

- [ ] Update `COMPATIBILITY.md`.
- [ ] Update `docs/compatibility/type-system-literals-conversion.md`.
- [ ] Avoid claiming expression truth semantics, `TRUE` / `FALSE` row-value
      assignments, `BOOL(1)`, signedness attributes, `ZEROFILL`, `SERIAL`,
      protocol metadata, compact storage, or changed ranges.

## Verification

- [ ] `cmake --build --preset dev`
- [ ] Focused parser/runtime CTest entries touched by this feature.
- [x] `./packages/libmylite/tests/mysql_baseline_bool_boolean_aliases_expectations.sh`
- [ ] `cmake --workflow --preset check`
- [ ] Final architecture/self-review for descriptor authority, no-warning
      alias semantics, metadata-only alter behavior, docs accuracy, and test
      relevance.
