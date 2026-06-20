# Baseline Grouped String Comparison HAVING Tasks

- [x] Verify official MySQL 8.4 documentation scope and MySQL 8.4.9 runtime behavior.
- [x] Write an independently authored feature spec with MyLite grammar notes.
- [x] Add MySQL 8.4.9 expectation coverage for grouped string comparison `HAVING`.
- [x] Add runtime C coverage for grouped string comparison `HAVING`.
- [x] Move the parser-corpus string RHS grouped `HAVING` runtime case from rejection to execution.
- [x] Keep numeric RHS string group-column comparisons outside this slice.
- [x] Confirm no catalog, file-format, VFS, SQLite fork, or public ABI changes are needed.
- [x] Update compatibility docs.
- [x] Run focused MySQL expectation, build, CTest, and full check workflow.
- [x] Review, commit atomically, push, and continue to the next baseline slice.
