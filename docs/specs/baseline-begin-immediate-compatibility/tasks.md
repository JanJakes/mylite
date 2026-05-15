# Baseline BEGIN IMMEDIATE Compatibility Tasks

- [x] Confirm the current MyLite transaction path and parser grammar.
- [x] Review official MySQL 8.4 transaction syntax and SQLite `BEGIN
      IMMEDIATE` documentation.
- [x] Write independently authored feature spec and scope boundaries.
- [x] Extend parser support for exactly `BEGIN IMMEDIATE`.
- [x] Add focused parser tests for accepted and rejected forms.
- [x] Reuse the existing runtime start-transaction execution path.
- [x] Add focused runtime transaction lifecycle coverage.
- [x] Update compatibility docs for the extension spelling.
- [x] Run targeted parser/runtime transaction CTest entries.
- [x] Run `cmake --workflow --preset check`.
- [ ] Review the final diff, commit atomically, push `main`, and run a
      subagent release-gate review.
