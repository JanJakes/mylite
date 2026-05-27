# Baseline UPDATE DATE Interval Assignment Tasks

- [x] Read existing update, DATE interval, temporal type, compatibility, parser,
  runtime, storage, and test context.
- [x] Verify MySQL 8.4.9 runtime behavior for admitted success, no-op,
  no-match, `LIMIT 0`, `NULL`, invalid-source, and overflow cases.
- [x] Write the independently authored feature spec and grammar snippets.
- [x] Add MySQL expectation artifact for the admitted user-visible behavior.
- [x] Extend parser support for the narrow DATE interval update-value shape.
- [x] Add planner/runtime support using descriptor-driven SQLite SQL and bound
  UDF arguments.
- [x] Add the strict update DATE interval SQLite UDF through public SQLite APIs.
- [x] Add C runtime tests and CMake registration.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused tests, MySQL expectation comparison, and full check workflow.
- [x] Review the diff, commit atomically, run subagent review, amend if needed,
  and push `main`.
