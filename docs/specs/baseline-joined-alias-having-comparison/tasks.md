# Baseline Joined Alias HAVING Comparison Tasks

- [x] Verify official MySQL 8.4 documentation scope and MySQL 8.4.9 runtime behavior.
- [x] Write an independently authored feature spec with MyLite grammar notes.
- [x] Add parser support for HAVING operand-to-operand equality and inequality comparisons.
- [x] Plan joined selected-alias HAVING comparisons by resolving aliases to descriptor columns.
- [x] Keep unsupported HAVING forms on deterministic diagnostics.
- [x] Move the parser-corpus joined alias HAVING residual from placeholder to execution.
- [x] Add focused runtime coverage for joined alias HAVING filtering.
- [x] Confirm no catalog, file-format, VFS, SQLite fork, or public ABI changes are needed.
- [x] Update compatibility docs.
- [x] Run focused MySQL expectation, build, CTest, and full check workflow.
- [x] Review, commit atomically, push, and continue to the next baseline slice.
