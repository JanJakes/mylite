# Baseline STRCMP Function Tasks

- [x] Research official MySQL 8.4 string comparison documentation.
- [x] Verify MySQL 8.4.9 runtime behavior for scalar values, table-backed
  values, `DO`, wrong arity, warnings, row count, ASCII case folding, and
  trailing-space behavior.
- [x] Write the independently authored feature specification.
- [x] Add MySQL-runtime expectation script.
- [x] Extend lexer/parser/AST for `STRCMP()` and wrong-arity forms.
- [x] Add parser coverage.
- [x] Register a MyLite SQLite scalar helper for ASCII case-insensitive
  normalized string comparison.
- [x] Add scalar and row-scalar projection runtime support.
- [x] Extend direct row-backed `STRCMP()` predicate and non-grouped order-key
  contexts.
- [x] Add focused runtime tests.
- [x] Update `COMPATIBILITY.md` and string/literal/query compatibility docs.
- [x] Run focused parser/runtime tests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review, commit, push, and run the feature review gate.
