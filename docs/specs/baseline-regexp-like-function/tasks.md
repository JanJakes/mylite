# Baseline REGEXP_LIKE Function Tasks

- [x] Audit existing regex predicate, row-scalar function, parser, AST, and
  SQLite function-registration paths.
- [x] Verify MySQL 8.4.9 behavior for scalar values, row predicates,
  `match_type`, `DO`, wrong arity, diagnostics, warnings, and row count.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL 8.4.9 expectation script.
- [x] Extend lexer/parser/AST for `REGEXP_LIKE()` and narrow predicate forms.
- [x] Add parser coverage.
- [x] Register a MyLite SQLite scalar helper for ASCII `REGEXP_LIKE()`.
- [x] Add scalar, row-scalar projection, predicate, `UPDATE`, and `DELETE`
  runtime support.
- [x] Add focused runtime tests.
- [x] Update `COMPATIBILITY.md` and string/query compatibility docs.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review, commit, push to `origin/main`, and run the feature review gate.
