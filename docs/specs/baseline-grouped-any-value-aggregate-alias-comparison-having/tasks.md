# Baseline Grouped ANY_VALUE Aggregate Alias Comparison HAVING Tasks

- [x] Verify official MySQL 8.4 documentation scope and MySQL 8.4.9 runtime behavior.
- [x] Write an independently authored feature spec with MyLite grammar notes.
- [x] Add MySQL 8.4.9 expectation coverage for selected alias comparison `HAVING`.
- [x] Add runtime C coverage for selected alias comparison `HAVING`.
- [x] Keep direct `HAVING ANY_VALUE(column)` expression operands rejected with MySQL-compatible diagnostics.
- [x] Add the narrow parser grammar needed for string literal comparison predicates.
- [x] Confirm no catalog, file-format, VFS, SQLite fork, or public ABI changes are needed.
- [x] Update compatibility docs.
- [x] Run focused MySQL expectation, build, CTest, and full check workflow.
- [x] Review, commit atomically, push, and continue to the next baseline slice.
