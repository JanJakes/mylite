# Baseline WHERE AND Predicates Tasks

- [x] Read current compatibility, predicate, parser, runtime, catalog, result,
  storage, and test context.
- [x] Research official MySQL 8.4 `SELECT`, `DELETE`, `UPDATE`, logical
  operator, and operator precedence documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for `AND`, `&&`, parentheses,
  diagnostics, warnings, grouping, DML affected rows, ordering, and limiting.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, physical SQLite handling, diagnostics, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Add parser grammar, AST kind/operator support, parser helpers, and token
  mapping for `AND` / `&&`.
- [x] Extend descriptor-driven predicate planning to support conjunctions,
  cleanup, SQL generation, and parameter binding.
- [x] Add parser and runtime lifecycle tests, including warning diagnostics,
  source reuse, DML behavior, persistence, preamble preservation, and
  independent handles.
- [x] Register any new tests in `packages/libmylite/CMakeLists.txt`.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, descriptor authority,
  performance, cleanup, compatibility wording, and test relevance.
- [x] Commit, push `main`, and continue to the next baseline slice.
