# Baseline Mutable Foreign Key Checks Tasks

- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime behavior.
- [x] Write independently authored feature specification.
- [x] Add MySQL-runtime expectation artifact for mutable session values and FK-off side effects.
- [x] Add MyLite runtime tests for session value mutation, diagnostics, DML gates, DDL rejections, persistence, and independent handles.
- [x] Implement handle-local `foreign_key_checks` session state and scalar/SHOW reads.
- [x] Implement supported `SET foreign_key_checks` value conversion and diagnostics.
- [x] Gate descriptor-owned foreign-key checks, referential actions, and `INSERT IGNORE` warnings.
- [x] Update compatibility docs for the exact limited mutable subset.
- [x] Run focused MySQL expectation and CTest coverage.
- [x] Run `cmake --workflow --preset check`.
- [x] Review final diff, amend if needed, commit, push, and run subagent review.
