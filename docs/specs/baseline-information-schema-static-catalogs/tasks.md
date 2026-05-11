# Baseline INFORMATION_SCHEMA Static Catalogs Tasks

## Design

- [x] Read project architecture, compatibility, information-schema, SHOW
  charset/collation, and InnoDB engine guidance.
- [x] Verify MySQL 8.4.9 behavior for `ENGINES`, `CHARACTER_SETS`, and
  `COLLATIONS` rows, system-view metadata, name predicate collation,
  successful warning/row-count behavior, and full-catalog counts.
- [x] Specify static ownership, row shape, query-surface reuse, diagnostics,
  ordering, compatibility gaps, and test expectations.

## Implementation

- [x] Add `ENGINES`, `CHARACTER_SETS`, and `COLLATIONS` table definitions to
  the limited information-schema registry.
- [x] Add static system row builders for the three new information-schema
  tables.
- [x] Add system `TABLES` and `COLUMNS` rows for the new views through the
  existing system-view metadata path.
- [x] Preserve descriptor authority, physical-name privacy, file-format safety,
  independent handles, reopen behavior, and zero-init cleanup.

## Tests and Docs

- [x] Add the MySQL 8.4.9 expectation script for the feature surface.
- [x] Add focused C runtime tests for supported rows, metadata, query reuse,
  diagnostics, and handle/reopen behavior.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented limited subset.
- [x] Run focused build/tests, the new MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, metadata claims, performance,
  file-format safety, and scope control.
