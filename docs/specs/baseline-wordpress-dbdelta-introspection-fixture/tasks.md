# Baseline WordPress dbDelta Introspection Fixture Tasks

- [x] Create the feature spec under
  `docs/specs/baseline-wordpress-dbdelta-introspection-fixture/`.
- [x] Verify MySQL 8.4.9 behavior for the representative metadata statements,
  result shapes, warning counts, and row-count state.
- [x] Add the MySQL-runtime expectation script for the fixture.
- [x] Add fast C runtime coverage for the descriptor-backed introspection
  workflow.
- [x] Register any new test binary in `packages/libmylite/CMakeLists.txt`.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs for the exact
  fixture wording.
- [x] Run focused runtime tests and the MySQL expectation script.
- [x] Run `cmake --build --preset dev` and `cmake --workflow --preset check`.
- [x] Review the final diff, commit atomically, run a subagent review, amend if
  needed, and push `main`.
