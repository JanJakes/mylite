# Baseline DO Statement Tasks

- [x] Read current scalar expression, arithmetic, comparison, logical, scalar
  `IS`, `CASE`, control-flow, diagnostics, parser, runtime, and compatibility
  docs.
- [x] Review official MySQL 8.4 `DO`, expression, function/operator,
  flow-control, and precedence documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for expression lists, row-count
  state, warning counts, no-result behavior, short-circuit warnings,
  overflows, wrong arities, accepted broader forms, and syntax errors.
- [x] Write independently authored feature spec with ownership boundaries,
  grammar snippet, runtime semantics, diagnostics, unsupported forms,
  performance/storage notes, and test plan.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the specified but not yet
  implemented subset.
- [x] Commit and push the start-feature artifacts.
- [x] Add parser/AST support for limited `DO` statements.
- [x] Add MyLite-owned `DO` execution that evaluates admitted scalar
  expressions without returning rows.
- [x] Preserve evaluated child warnings, skipped-child warning suppression,
  diagnostics counts, and row-count ordering.
- [x] Add runtime/parser tests for accepted expressions, warning behavior,
  diagnostics, row-count state, file safety, independent handles, and
  deterministic rejection of unsupported forms.
- [x] Register the new runtime test binary in `packages/libmylite/CMakeLists.txt`.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, expression-scope
  control, diagnostics sequencing, no-result conventions, signed-64 conversion
  safety, performance, cleanup, compatibility wording, and test relevance.
