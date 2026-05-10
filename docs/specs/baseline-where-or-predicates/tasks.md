# Baseline WHERE OR Predicates Tasks

- [x] Read current compatibility, predicate, parser, runtime, catalog, result,
  storage, and test context.
- [x] Research official MySQL 8.4 logical operator, operator precedence,
  `SELECT`, `DELETE`, and `UPDATE` documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for `OR`, `||`, precedence,
  parentheses, diagnostics, warnings, grouping, DML affected rows, ordering,
  and limiting.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, physical SQLite handling, diagnostics, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Add parser grammar, AST kind/operator support, parser helpers, and token
  mapping for `OR` / `||`.
- [x] Extend descriptor-driven predicate planning to preserve boolean
  expression trees, cleanup, SQL generation, and parameter binding.
- [x] Add parser and runtime lifecycle tests, including warning diagnostics,
  precedence, source reuse, DML behavior, persistence, preamble preservation,
  and independent handles.
- [x] Confirm no new test binary is needed in `packages/libmylite/CMakeLists.txt`.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, descriptor authority,
  performance, cleanup, compatibility wording, and test relevance.
- [x] Commit, push `main`, and continue to the next baseline slice.
