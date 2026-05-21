# Baseline INFORMATION_SCHEMA PARTITIONS Tasks

- [x] Review existing MyLite information-schema query, system-view metadata,
      table-status, descriptor catalog, and compatibility docs.
- [x] Research official MySQL 8.4 documentation and verify MySQL 8.4.9 runtime
      behavior for nonpartitioned table rows, system-view rows, columns
      metadata, diagnostics, warnings, and row-count behavior.
- [x] Specify the metadata-only scope, ownership boundaries, row values,
      diagnostics, performance expectations, and tests.
- [x] Add the MySQL 8.4.9 expectation script for the feature surface.
- [x] Register `INFORMATION_SCHEMA.PARTITIONS` and its MySQL-shaped column
      definitions in the information-schema table registry.
- [x] Add system-view and descriptor-derived nonpartitioned base-table rows.
- [x] Preserve descriptor authority, physical-name privacy, file-format safety,
      independent handles, reopen behavior, and zero-init cleanup.
- [x] Add focused C runtime tests for metadata, query reuse, diagnostics,
      persistence, independent handles, and file-format safety.
- [x] Update `COMPATIBILITY.md` and detailed information-schema compatibility
      docs with exact limited wording.
- [x] Run the MySQL expectation script, focused CTests, and
      `cmake --workflow --preset check`.
- [x] Review with a subagent, amend if needed, commit atomically, and push
      `main`.
