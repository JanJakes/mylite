# Baseline INFORMATION_SCHEMA Constraints Tasks

## Design

- [x] Read project architecture, compatibility, information-schema, catalog,
  primary-key, secondary-index, and unique-index guidance.
- [x] Verify MySQL 8.4.9 behavior for `TABLE_CONSTRAINTS` and
  `KEY_COLUMN_USAGE` columns, system-view metadata, primary-key rows,
  unique-constraint rows, and nonunique-index omission.
- [x] Specify descriptor ownership, row shape, query-surface reuse,
  diagnostics, ordering, compatibility gaps, and test expectations.

## Implementation

- [ ] Add `TABLE_CONSTRAINTS` and `KEY_COLUMN_USAGE` table definitions to the
  limited information-schema registry.
- [ ] Add descriptor-backed row builders for primary and supported unique-index
  descriptors, excluding nonunique secondary indexes.
- [ ] Add system `TABLES` and `COLUMNS` rows for the new information-schema
  views through the existing system-view metadata path.
- [ ] Preserve descriptor authority, physical-name privacy, reopen persistence,
  independent handles, rename/drop/truncate behavior, and zero-init cleanup.

## Tests and Docs

- [x] Add the MySQL 8.4.9 expectation script for the feature surface.
- [ ] Add focused C runtime tests for supported rows, metadata, lifecycle, and
  diagnostics.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [ ] Run focused build/tests, the new MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review the final diff for MySQL behavior, descriptor authority,
  metadata claims, performance, file-format safety, and scope control.
