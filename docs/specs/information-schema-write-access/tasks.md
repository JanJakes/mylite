# INFORMATION_SCHEMA Write Access Baseline Tasks

- [x] Verify MySQL 8.4.9 diagnostics for `information_schema` schema, table,
  index, DML, rename, and selected-schema write targets.
- [x] Specify the MyLite ownership boundary, resolution precedence, diagnostics,
  and no-storage-impact behavior.
- [x] Update compatibility docs for the limited read-only selected
  `information_schema` and write-access-denied baseline.
- [x] Add planner/runtime write-target guards before schema lookup.
- [x] Add `USE information_schema` session selection support without catalog
  descriptor storage.
- [x] Add unqualified `INFORMATION_SCHEMA` metadata `SELECT` resolution when
  `information_schema` is selected.
- [x] Add MySQL expectation script for this slice.
- [x] Add focused C runtime coverage and CTest registration.
- [x] Run focused tests and MySQL expectation verification.
- [x] Run `cmake --workflow --preset check`.
- [x] Review, fix findings, commit, and push `main`.
