# Baseline WHERE NOT Predicates Tasks

- [x] Read current compatibility, predicate, parser, runtime, catalog, result,
  storage, and test context.
- [x] Research official MySQL 8.4 logical operator, expression, operator
  precedence, `SELECT`, `DELETE`, and `UPDATE` documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for keyword `NOT`, symbolic `!`,
  precedence, parentheses, diagnostics, warnings, grouping, DML affected rows,
  ordering, and limiting.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, physical SQLite handling, diagnostics, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [ ] Add parser grammar, AST kind/operator support, parser helpers, and token
  mapping for keyword `NOT`.
- [ ] Extend descriptor-driven predicate planning to preserve unary negation,
  cleanup, SQL generation, and parameter binding.
- [ ] Add parser and runtime lifecycle tests, including precedence, source
  reuse, DML behavior, persistence, preamble preservation, independent handles,
  and deterministic rejection of symbolic `!`.
- [ ] Confirm whether a new test binary is needed in
  `packages/libmylite/CMakeLists.txt`.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, descriptor authority,
  performance, cleanup, compatibility wording, and test relevance.
- [ ] Commit, push `main`, and continue to the next baseline slice.
