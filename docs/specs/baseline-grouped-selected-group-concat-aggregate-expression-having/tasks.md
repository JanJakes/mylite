# Baseline Grouped Selected GROUP_CONCAT Aggregate Expression HAVING Tasks

- [x] Verify official MySQL 8.4 documentation scope and MySQL 8.4.9 runtime behavior.
- [x] Write an independently authored feature spec with MyLite grammar notes.
- [x] Add MySQL 8.4.9 expectation coverage for repeated selected expression `HAVING`.
- [x] Add runtime C coverage for repeated selected expression `HAVING`.
- [x] Keep nonselected `GROUP_CONCAT()` expressions and comparisons unsupported for this slice.
- [x] Confirm no parser, catalog, file-format, VFS, SQLite fork, or public ABI changes are needed.
- [x] Update compatibility docs.
- [x] Run focused MySQL expectation, build, CTest, and full check workflow.
- [x] Review, commit atomically, push, and continue to the next baseline slice.
