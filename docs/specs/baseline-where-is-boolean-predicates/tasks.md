# Baseline WHERE IS Boolean Predicates Tasks

- [x] Read current compatibility, predicate, parser, runtime, catalog, result,
  storage, and test context.
- [x] Research official MySQL 8.4 comparison, expression, and operator
  precedence documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for `IS TRUE`, `IS FALSE`,
  `IS UNKNOWN`, their `IS NOT` forms, prefix `NOT`, DML composition,
  diagnostics, warnings, and unsupported broader forms.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, physical SQLite handling, diagnostics, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Commit the start-feature artifacts.
- [ ] Add parser grammar, AST operator support, parser helpers, and token
  mapping for descriptor-backed `IS` boolean predicates.
- [ ] Extend descriptor-driven predicate planning for truth-test leaves,
  cleanup, and SQLite SQL generation.
- [ ] Add parser and runtime lifecycle tests, including precedence, source
  reuse, DML behavior, persistence, preamble preservation, independent handles,
  and deterministic rejection of unsupported broader forms.
- [ ] Confirm whether a new test binary is needed in
  `packages/libmylite/CMakeLists.txt`.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, descriptor authority,
  performance, cleanup, compatibility wording, and test relevance.
- [ ] Commit, push `main`, and continue to the next baseline slice.
