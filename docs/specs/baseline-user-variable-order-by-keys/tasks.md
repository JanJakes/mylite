# Baseline User-Variable ORDER BY Keys Tasks

- [x] Verify official MySQL 8.4 documentation scope and MySQL 8.4.9 runtime behavior.
- [x] Write an independently authored feature spec with MyLite grammar notes.
- [x] Add parser support for user-variable `SELECT ORDER BY` keys.
- [x] Add planner support that treats user-variable order keys as no-op keys.
- [x] Move the parser-corpus `ORDER BY @rank` case from rejection to execution.
- [x] Add focused runtime coverage for mixed user-variable and descriptor order keys.
- [x] Keep assignment, parameter, system-variable, function, and arbitrary expression order keys outside this slice.
- [x] Confirm no catalog, file-format, VFS, SQLite fork, or public ABI changes are needed.
- [x] Update compatibility docs.
- [x] Run focused MySQL expectation, build, CTest, and full check workflow.
- [x] Review, commit atomically, push, and continue to the next baseline slice.
