# Baseline INFORMATION_SCHEMA RESOURCE_GROUPS Tasks

- [x] Verify MySQL 8.4.9 `RESOURCE_GROUPS` table shape, metadata, default
      rows, row ordering, and status behavior.
- [x] Register `RESOURCE_GROUPS` in the synthetic information-schema table
      registry.
- [x] Add MySQL-shaped `INFORMATION_SCHEMA.COLUMNS` metadata for all five
      columns.
- [x] Emit default `USR_default` and `SYS_default` metadata rows with
      system-dependent all-online-CPU `VCPU_IDS` text.
- [x] Add a focused C runtime test for row shape, metadata, query behavior,
      and file-backed handle safety.
- [x] Add a MySQL 8.4.9 expectation script documenting observed behavior.
- [x] Update `COMPATIBILITY.md` and
      `docs/compatibility/metadata-information-schema.md`.
- [x] Run the MySQL expectation script, focused C tests, `git diff --check`,
      and the full CMake check workflow.
- [x] Review, commit, and push the focused slice.
