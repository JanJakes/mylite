# Baseline String Function Predicates Tasks

- [x] Specify the narrow predicate grammar, semantics, ownership boundaries,
      diagnostics, and performance model.
- [x] Verify MySQL 8.4.9 behavior for the admitted length and substring
      predicate forms.
- [x] Refactor parser productions so length and substring expressions can be
      reused by predicate grammar.
- [x] Add descriptor-driven predicate planner support for string length and
      substring function expressions.
- [x] Preserve SQLite `WHERE` pushdown and add string collation to
      string-valued row-scalar comparisons.
- [x] Add parser, runtime, and MySQL expectation coverage.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused tests, MySQL expectations, `cmake --build --preset dev`, and
      `cmake --workflow --preset check`.
- [x] Review the feature and fix release-gate findings before committing.
