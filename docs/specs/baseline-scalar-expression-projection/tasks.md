# Baseline Scalar Expression Projection Tasks

- [x] Read current compatibility, scalar select, literal projection,
  control-flow/comparison scalar functions, parser, runtime, result, storage,
  and test context.
- [x] Research official MySQL 8.4 `SELECT`, expression, literal, flow-control,
  and comparison-function documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for mixed scalar value select lists,
  `FROM DUAL`, explicit `ALL`, labels, aliases, parentheses, nested functions,
  warning count, row count, wrong arity, and broader deferred forms.
- [x] Write the independent feature spec with MyLite analyzer grammar snippets,
  ownership boundaries, scalar evaluation semantics, diagnostics, performance
  constraints, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the specified but not yet
  implemented subset.
- [ ] Commit and push the start-feature artifacts.
- [ ] Replace homogeneous literal/function projection classifiers with a shared
  scalar value projection classifier while preserving the separate supported
  session-scalar path.
- [ ] Admit mixed no-source and `FROM DUAL` scalar value projection over the
  current literal/control-flow/comparison-function subset.
- [ ] Admit parenthesized scalar value expressions at top level and inside the
  supported scalar functions.
- [ ] Add runtime lifecycle tests for values, labels, aliases, nested calls,
  warnings, row count, file safety, independent handles, and deterministic
  rejection of unsupported broader forms.
- [ ] Register any new test binary in `packages/libmylite/CMakeLists.txt`.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, expression-scope
  control, performance, cleanup, compatibility wording, and test relevance.
- [ ] Commit, push `main`, and continue to the next baseline slice.
