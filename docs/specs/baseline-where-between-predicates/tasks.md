# Baseline WHERE BETWEEN Predicates Tasks

- [x] Read current compatibility, predicate, parser, runtime, catalog, result,
  storage, and test context.
- [x] Research official MySQL 8.4 comparison, expression, operator precedence,
  `SELECT`, `DELETE`, and `UPDATE` documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for `BETWEEN`, `NOT BETWEEN`,
  precedence, parentheses, nullable tested values, DML affected rows, ordering,
  limiting, and warnings.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, physical SQLite handling, diagnostics, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Commit the start-feature artifacts.
- [x] Add parser grammar, AST kind/operator support, parser helpers, and token
  mapping for `BETWEEN`.
- [x] Extend descriptor-driven predicate planning for range leaves, cleanup,
  SQL generation, and parameter binding.
- [x] Add parser and runtime lifecycle tests, including precedence, source
  reuse, DML behavior, persistence, preamble preservation, independent handles,
  and deterministic rejection of unsupported broader forms.
- [x] Confirm whether a new test binary is needed in
  `packages/libmylite/CMakeLists.txt`.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, descriptor authority,
  performance, cleanup, compatibility wording, and test relevance.
- [x] Commit, push `main`, and continue to the next baseline slice.
