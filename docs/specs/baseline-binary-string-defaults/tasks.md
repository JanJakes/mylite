# Baseline Binary String Defaults Tasks

## Design

- [x] Read project architecture, compatibility, parser, runtime, storage, and
      SQLite fork guidance relevant to binary defaults.
- [x] Verify MySQL 8.4.9 behavior for `BINARY`/`VARBINARY` literal defaults,
      metadata display, DML materialization, overlength diagnostics, and
      deferred BLOB-family defaults.
- [x] Specify the independently authored MyLite semantics, storage mapping,
      diagnostics, compatibility gaps, performance boundary, and test plan.

## Implementation

- [ ] Add a catalog default kind for descriptor-owned binary default bytes.
- [ ] Convert admitted `BINARY` and `VARBINARY` literal defaults through the
      existing MyLite binary literal conversion path.
- [ ] Store binary defaults as byte-safe internal hex text in catalog
      descriptors.
- [ ] Materialize omitted-column and explicit DML `DEFAULT` values as BLOB
      planned values.
- [ ] Render binary defaults through `SHOW COLUMNS`, `DESCRIBE`,
      `SHOW CREATE TABLE`, and limited `INFORMATION_SCHEMA.COLUMNS`.
- [ ] Preserve add-column backfill, descriptor cloning/copying, reopen
      persistence, and file-format preamble invariants.

## Tests and Docs

- [ ] Add the MySQL 8.4.9 expectation script for the feature surface.
- [ ] Add focused C runtime tests for supported and rejected behavior.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
      implemented subset.
- [ ] Run focused build/tests, the new MySQL expectation script, and
      `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, catalog authority,
      binary default conversion, generated SQL quoting, compatibility claims,
      performance, and file-format safety.
