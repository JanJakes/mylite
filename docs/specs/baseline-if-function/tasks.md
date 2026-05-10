# Baseline IF Function Tasks

- [x] Read current compatibility, scalar select, literal projection, parser,
  runtime, result, storage, and test context.
- [x] Research official MySQL 8.4 `IF()` / flow-control documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for truthiness, `NULL` handling,
  booleans, integer values, labels, nested calls, aliases, `FROM DUAL`,
  warnings, row count, wrong arity, and broader deferred forms.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, scalar evaluation semantics, diagnostics, performance
  constraints, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [ ] Commit and push the start-feature artifacts.
- [ ] Add parser/AST support and parser tests for exact three-argument `IF()`.
- [ ] Extend the no-source/`FROM DUAL` scalar projection runtime to evaluate
  supported `IF()` expressions without SQLite SQL.
- [ ] Add runtime lifecycle tests for values, labels, aliases, nested calls,
  warnings, row count, file safety, independent handles, and deterministic
  rejection of unsupported broader forms.
- [ ] Register any new test binary in `packages/libmylite/CMakeLists.txt`.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, expression-scope
  control, performance, cleanup, compatibility wording, and test relevance.
- [ ] Commit, push `main`, and continue to the next baseline slice.

