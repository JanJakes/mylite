# Baseline INFORMATION_SCHEMA VIEW_TABLE_USAGE Tasks

- [x] Read current information-schema implementation and compatibility context.
- [x] Verify MySQL 8.4.9 `VIEW_TABLE_USAGE` table shape, metadata, empty-row
  behavior, and real-view dependency behavior.
- [x] Write the independent feature spec with ownership boundaries, runtime
  behavior, diagnostics, performance notes, and test plan.
- [x] Add a MySQL-runtime expectation script for this feature.
- [x] Register `VIEW_TABLE_USAGE` in the synthetic information-schema table
  registry with exact column metadata.
- [x] Update compatibility documentation for the exact supported empty-system
  view subset.
- [x] Add focused runtime tests and CMake registration if a new test binary is
  clearer than extending an existing information-schema test.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for metadata fidelity, scope control, allocation
  cleanup, file-format safety, docs accuracy, and test relevance.
- [x] Commit, review with a subagent, amend if needed, and push `main`.
