# Baseline Result Column Metadata Tasks

- [x] Choose feature scope and slug.
- [x] Read project architecture, compatibility, result metadata, statement
  context, descriptor-backed SELECT, type, index, and public API docs.
- [x] Verify official MySQL 8.4 documentation for `MYSQL_FIELD`, field type
  IDs, charset/collation metadata, display lengths, decimals, and flag bits.
- [x] Probe MySQL 8.4.9 runtime behavior for aliases, table aliases, collation
  IDs, representative descriptor families, display lengths, decimals, key
  flags, and no-warning metadata selects.
- [x] Add MySQL-runtime expectation script for the selected metadata surface.
- [x] Write independently authored feature specification with ownership
  boundaries, public API shape, descriptor mapping, diagnostics, performance,
  and known exclusions.
- [x] Add public result metadata constants and accessors.
- [x] Make `mylite_result` own column metadata for its lifetime.
- [x] Populate descriptor-backed single-table `SELECT` metadata from catalog
  descriptors and loaded key metadata.
- [x] Add runtime tests under `packages/libmylite/tests/`.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs with limited
  support wording.
- [x] Register any new test binary in `packages/libmylite/CMakeLists.txt`.
- [x] Run focused runtime/MySQL expectation verification.
- [x] Run `cmake --workflow --preset check`.
- [x] Review with a subagent, amend findings, commit, and push to remote
  `main`.
