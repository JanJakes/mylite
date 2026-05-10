# Baseline Replace Values Lifecycle Tasks

- [x] Read project architecture, compatibility, parser, runtime, storage, and
  prior DML lifecycle context.
- [x] Verify MySQL 8.4.9 `REPLACE` syntax, no-key behavior, key-conflict
  affected rows, diagnostics, warnings, and wider deferred forms.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, physical SQLite handling, diagnostics, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [ ] Add parser token mapping, grammar, AST kind, AST name, and parser helper
  for `REPLACE [INTO] ... VALUES`.
- [ ] Reuse the descriptor-driven insert planner/executor for no-key baseline
  `REPLACE` while keeping diagnostics and future duplicate-key semantics clear.
- [ ] Add C parser and runtime lifecycle tests, including persistence,
  diagnostics, reserved names, generation stability, preamble preservation, and
  independent file-backed handles.
- [ ] Register any new tests in `packages/libmylite/CMakeLists.txt`.
- [ ] Run focused build/tests, MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, descriptor authority,
  physical SQL generation, cleanup, compatibility wording, and test relevance.
