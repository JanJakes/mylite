# Baseline Temporary AUTO_INCREMENT Tasks

- [x] Create an independently authored feature spec with official MySQL 8.4
  documentation references and MySQL 8.4.9 runtime observations.
- [x] Add a MySQL 8.4.9 expectation script for direct temporary
  auto-increment, `LIKE` cloning, SQL-mode behavior, metadata omission, and
  transaction rollback observations.
- [x] Preserve auto-increment column flags and table counters in temporary
  descriptors.
- [x] Add a session-local temporary-catalog counter update API.
- [x] Route insert/update counter advancement to the durable catalog for
  persistent tables and the temporary catalog for temporary tables.
- [x] Remove the temporary `AUTO_INCREMENT` rejection while keeping temporary
  `FULLTEXT`, `FOREIGN KEY`, `CHECK`, and active-transaction DDL restrictions.
- [x] Flip temporary `LIKE` auto-increment coverage from rejected to supported.
- [x] Add focused C runtime tests under `packages/libmylite/tests/`.
- [x] Update `COMPATIBILITY.md` and compatibility detail docs for the exact
  supported subset.
- [x] Run MySQL expectations, focused CTests, `cmake --build --preset dev`,
  and `cmake --workflow --preset check`.
- [x] Review, commit, push, and continue with the next baseline slice.
