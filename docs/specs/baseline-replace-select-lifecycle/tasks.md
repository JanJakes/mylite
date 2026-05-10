# Baseline Replace Select Lifecycle Tasks

- [x] Read project architecture, compatibility, parser, runtime, storage, and
  prior `INSERT ... SELECT` / `REPLACE` lifecycle context.
- [x] Research official MySQL 8.4 `REPLACE` and `INSERT ... SELECT`
  documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for supported syntax, result shape,
  no-key/keyed behavior, diagnostics, zero-row sources, omitted columns,
  invisible columns, and target/source resolution precedence.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, physical SQLite handling, diagnostics, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [ ] Add parser grammar, AST kind, AST name, and parser helper for
  `REPLACE [INTO] ... SELECT`.
- [ ] Reuse the descriptor-driven `INSERT ... SELECT` planner/executor for the
  no-key baseline `REPLACE ... SELECT` path while keeping future duplicate-key
  semantics clear.
- [ ] Add C parser and runtime lifecycle tests, including persistence,
  diagnostics, reserved names, generation stability, preamble preservation, and
  independent file-backed handles.
- [ ] Register any new tests in `packages/libmylite/CMakeLists.txt`.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, descriptor authority,
  performance, cleanup, compatibility wording, and test relevance.
- [ ] Commit the implementation slice.
