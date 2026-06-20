# Baseline Constant ORDER BY Keys Tasks

- [x] Verify official MySQL 8.4 documentation scope and MySQL 8.4.9 runtime behavior.
- [x] Write an independently authored feature spec with MyLite grammar notes.
- [x] Add parser support for `NULL` and string-literal `SELECT ORDER BY` keys.
- [x] Add planner support that treats admitted constant order keys as no-op keys.
- [x] Move parser-corpus constant `ORDER BY` cases from rejection to execution.
- [x] Add focused runtime coverage for mixed constant and descriptor order keys.
- [x] Keep broad expression, user-variable, parameter, and non-string constant keys outside this slice.
- [x] Confirm no catalog, file-format, VFS, SQLite fork, or public ABI changes are needed.
- [x] Update compatibility docs.
- [x] Run focused MySQL expectation, build, CTest, and full check workflow.
- [x] Review, commit atomically, push, and continue to the next baseline slice.
