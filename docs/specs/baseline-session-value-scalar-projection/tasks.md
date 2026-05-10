# Baseline Session Value Scalar Projection Tasks

- [x] Read current compatibility, session scalar, scalar value projection,
  diagnostics, system-variable, parser, runtime, result, storage, and test
  context.
- [x] Research official MySQL 8.4 `SELECT`, expression, information-function,
  system-variable, flow-control, and comparison-function documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for mixed session/value scalar select
  lists, `FROM DUAL`, explicit `ALL`, labels, aliases, parentheses, warning
  count, error count, row count, and broader deferred forms.
- [x] Write the independent feature spec with MyLite analyzer grammar snippets,
  ownership boundaries, scalar evaluation semantics, diagnostics, performance
  constraints, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the specified but not yet
  implemented subset.
- [ ] Commit and push the start-feature artifacts.
- [ ] Replace the split session-scalar and scalar-value projection admission
  checks with one mixed scalar projection classifier while keeping one shared
  executor.
- [ ] Admit mixed no-source and `FROM DUAL` scalar projection over the current
  session-scalar and scalar-value subsets.
- [ ] Preserve scalar value function operand restrictions, especially rejection
  of session/system variables inside `IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/
  `ISNULL()`.
- [ ] Preserve warning-count and row-count ordering for mixed projections,
  including warning-producing `@@sql_slave_skip_counter` reads.
- [ ] Add runtime lifecycle tests for values, labels, aliases, warnings, row
  count, error count, file safety, independent handles, and deterministic
  rejection of unsupported broader forms.
- [ ] Register any new test binary in `packages/libmylite/CMakeLists.txt`.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, expression-scope
  control, diagnostics sequencing, performance, cleanup, compatibility wording,
  and test relevance.
- [ ] Commit, push `main`, and continue to the next baseline slice.
