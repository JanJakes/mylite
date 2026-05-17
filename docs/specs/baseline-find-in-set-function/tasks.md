# Baseline FIND_IN_SET Function Tasks

- [x] Research official MySQL 8.4 string-function documentation.
- [x] Verify MySQL 8.4.9 runtime behavior for scalar values, table-backed
  values, predicates, `DO`, wrong arity, warnings, and row count.
- [x] Write the independently authored feature specification.
- [x] Add MySQL-runtime expectation script.
- [x] Extend lexer/parser/AST for `FIND_IN_SET()` and narrow predicate forms.
- [x] Add parser coverage.
- [x] Register a MyLite SQLite scalar helper for ASCII case-insensitive
  comma-list matching.
- [x] Add scalar, row-scalar projection, predicate, `UPDATE`, and `DELETE`
  runtime support.
- [x] Add focused runtime tests.
- [x] Update `COMPATIBILITY.md` and string/literal/query/DML compatibility docs.
- [x] Run focused parser/runtime tests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review, commit, and run the feature review gate.
