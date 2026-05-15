# Baseline Time Zone System Variable Tasks

- [x] Verify MySQL 8.4.9 `time_zone` and `system_time_zone` behavior for the
  admitted subset.
- [x] Write an independently authored feature spec with grammar snippets,
  ownership boundaries, semantics, diagnostics, and non-goals.
- [x] Add a MySQL expectation script for the runtime-observed behavior.
- [x] Update compatibility documentation for the designed slice.
- [ ] Extend parser/AST support for unquoted `SYSTEM` / `UTC` `SET` values.
- [ ] Add session-state defaults and parsed time-zone offset storage.
- [ ] Add scalar and `SHOW VARIABLES` readback for `time_zone` and
  `system_time_zone`.
- [ ] Add `SET time_zone` validation, normalization, diagnostics, and
  session-local mutation.
- [ ] Apply session time-zone offset to current date/time/timestamp
  materialization without changing TIMESTAMP row read conversion.
- [ ] Add focused runtime and parser tests.
- [ ] Run MySQL expectation script, focused CTest entries, and full check
  workflow.
- [ ] Review the implementation, amend gaps, commit, and push.
