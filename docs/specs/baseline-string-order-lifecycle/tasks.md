# Baseline String Order Lifecycle Tasks

- [x] Choose feature scope and slug.
- [x] Read project architecture, compatibility, existing string, ordering, and
  DML specs.
- [x] Verify official MySQL 8.4 documentation for string sorting and
  single-table ordered `UPDATE` / `DELETE`.
- [x] Probe MySQL 8.4.9 runtime behavior for admitted ASCII string ordering,
  `NULL` placement, directions, trailing spaces, ordered limited `UPDATE`, and
  ordered limited `DELETE`.
- [x] Add MySQL-runtime expectation script for the selected feature surface.
- [x] Write independently authored feature specification with MyLite grammar
  snippets, ownership boundaries, diagnostics, and known exclusions.
- [ ] Extend the shared descriptor order planner for admitted nonbinary string
  descriptors.
- [ ] Add or extend runtime tests under `packages/libmylite/tests/`.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs with limited
  support wording.
- [ ] Run focused runtime/MySQL expectation verification.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review with a subagent, amend findings, commit, and push to remote
  `main`.
