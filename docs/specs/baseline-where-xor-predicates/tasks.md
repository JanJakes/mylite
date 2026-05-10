# Baseline WHERE XOR Predicates Tasks

- [x] Read current compatibility, predicate, parser, runtime, catalog, result,
  storage, and test context.
- [x] Research official MySQL 8.4 logical operator and operator-precedence
  documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for truth tables, `NULL` propagation,
  precedence, repeated `XOR`, source-filter reuse, DML side effects,
  diagnostics, and warnings.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, physical SQLite handling, diagnostics, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Commit and push the start-feature artifacts.
- [x] Add parser grammar, AST operator support, parser helpers, and token
  mapping for descriptor-backed `XOR` predicates.
- [x] Extend descriptor-driven predicate planning, cleanup, and SQLite SQL
  generation for logical `XOR`.
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
