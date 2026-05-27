# Built-in Schema Write Protection Baseline Tasks

- [x] Verify official MySQL 8.4 references for `INFORMATION_SCHEMA`, `mysql`,
  Performance Schema, and `sys` built-in schemas.
- [x] Probe MySQL 8.4.9 runtime diagnostics and write behavior for safe
  built-in schema cases.
- [x] Specify MyLite's stricter embedded write-protection decision for
  metadata-only built-in schemas.
- [x] Add a MySQL 8.4.9 expectation script for safe built-in schema access
  observations.
- [x] Add focused C runtime coverage for qualified and selected-schema
  built-in write targets.
- [x] Register the runtime test in CMake.
- [x] Update compatibility documentation for built-in schema access semantics.
- [x] Run focused tests and MySQL expectation verification.
- [x] Run `git diff --check`.
- [x] Run `cmake --workflow --preset check`.
- [ ] Review, fix findings, commit, and push `main`.
