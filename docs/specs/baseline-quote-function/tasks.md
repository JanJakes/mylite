# Baseline QUOTE Function Tasks

- [x] Research official MySQL 8.4 string function documentation.
- [x] Verify MySQL 8.4.9 runtime behavior for scalar values, escaping,
  `QUOTE(NULL)`, SQL-mode parsing interaction, table-backed values, `DO`,
  wrong arity, warnings, and row count.
- [x] Write the independently authored feature specification.
- [x] Add MySQL-runtime expectation script.
- [x] Extend lexer/parser/AST for `QUOTE()` and wrong-arity forms.
- [x] Add parser coverage.
- [x] Register a MyLite SQLite scalar helper for SQL string quoting.
- [x] Add scalar and row-scalar projection runtime support.
- [x] Add focused runtime tests.
- [x] Update `COMPATIBILITY.md` and string/query compatibility docs.
- [x] Run focused parser/runtime tests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [ ] Review, commit, push, and run the feature review gate.
