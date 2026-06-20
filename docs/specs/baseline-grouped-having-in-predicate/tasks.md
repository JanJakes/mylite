# Baseline Grouped HAVING IN Predicate Tasks

- [x] Verify official MySQL 8.4 documentation scope and MySQL 8.4.9 runtime behavior.
- [x] Write an independently authored feature spec with MyLite grammar notes.
- [x] Add MySQL 8.4.9 expectation coverage for grouped `HAVING IN`.
- [x] Add runtime C coverage for grouped `HAVING IN`.
- [x] Move the parser-corpus grouped `HAVING IN` case from rejection to execution.
- [x] Keep `NOT IN`, aggregate-result membership, subqueries, and broad expression lists outside this slice.
- [x] Confirm no catalog, file-format, VFS, SQLite fork, or public ABI changes are needed.
- [x] Update compatibility docs.
- [x] Run focused MySQL expectation, build, CTest, and full check workflow.
- [x] Review, commit atomically, push, and continue to the next baseline slice.
