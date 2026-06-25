# Baseline UUID Function Tasks

- [x] Verify MySQL 8.4.9 behavior for `UUID()` result shape, charset,
      collation, coercibility, `DO`, identifier handling, and arity errors.
- [x] Write independently authored feature spec and grammar snippets.
- [x] Add MySQL expectation script for the supported and intentionally deferred
      behavior.
- [x] Extend lexer/parser/AST support for `UUID()`.
- [x] Implement MyLite-owned UUID generation and SQLite row-scalar UDF.
- [x] Add parser and runtime C tests.
- [x] Update compatibility docs.
- [x] Run focused build/tests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review, commit, amend if needed, and push `origin main`.
- [x] Verify MySQL 8.4.9 behavior for `UUID_SHORT()` values, metadata, scalar
      metadata functions, `DO`, identifier handling, and arity errors.
- [x] Extend the spec, MySQL expectation script, parser coverage, runtime tests,
      compatibility docs, and runtime implementation for the `UUID_SHORT()`
      baseline.
- [x] Run focused build/tests, MySQL expectation script, and check workflow for
      the `UUID_SHORT()` baseline.
