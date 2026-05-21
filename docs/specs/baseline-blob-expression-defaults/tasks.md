# Baseline BLOB Expression Defaults Tasks

## Design

- [x] Read project architecture, compatibility, parser, runtime, catalog,
      storage, and SQLite fork guidance relevant to BLOB expression defaults.
- [x] Verify MySQL 8.4.9 behavior for bare BLOB defaults, generated hex and
      `NULL` defaults, metadata display, DML materialization, ALTER variants,
      overlength behavior, and deferred broader expression defaults.
- [x] Specify the independently authored MyLite semantics, catalog mapping,
      diagnostics, compatibility gaps, performance boundary, and test plan.

## Implementation

- [x] Admit parenthesized hexadecimal and `NULL` generated defaults for
      BLOB-family descriptors in full column-definition DDL.
- [x] Store admitted generated BLOB bytes through MyLite's descriptor-owned
      byte-safe binary default payload.
- [x] Materialize omitted-column and explicit DML `DEFAULT` values as BLOB or
      `NULL` planned values.
- [x] Render generated BLOB defaults through `SHOW COLUMNS`, `DESCRIBE`,
      `SHOW CREATE TABLE`, and limited `INFORMATION_SCHEMA.COLUMNS`.
- [x] Preserve add-column backfill, descriptor cloning/copying, reopen
      persistence, table rename/drop behavior, independent handles, and
      file-format preamble invariants.

## Tests and Docs

- [x] Add the MySQL 8.4.9 expectation script for the feature surface.
- [x] Add focused C runtime tests for supported and rejected behavior.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
      implemented subset.
- [x] Run focused build/tests, the new MySQL expectation script, and
      `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, catalog authority,
      binary conversion, metadata generated-extra behavior, compatibility
      claims, performance, and file-format safety.
