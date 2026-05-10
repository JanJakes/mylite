# Baseline Scalar Logical Projection Tasks

- [x] Read current scalar expression, arithmetic, modulo, `DIV`, comparison,
  and descriptor-backed logical predicate specs, compatibility docs,
  parser/runtime code, and tests.
- [x] Review official MySQL 8.4 logical-operator, expression, and precedence
  documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for keyword logical truth tables,
  nonzero truthiness, `NULL`, child warning short-circuiting, precedence,
  labels, row count, warning count, boundary operands, syntax errors, and
  accepted broader forms.
- [x] Write independently authored feature spec with ownership boundaries,
  grammar snippet, runtime semantics, diagnostics, unsupported forms,
  performance/storage notes, and test plan.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the specified but not yet
  implemented subset.
- [x] Commit and push the start-feature artifacts.
- [x] Add parser support for keyword logical scalar expressions without
  changing descriptor-backed predicate planning.
- [x] Admit no-source and `FROM DUAL` scalar logical projection without
  widening table-backed expression projection.
- [x] Add MyLite-owned three-valued logical evaluation with MySQL 8.4.9
  short-circuit behavior.
- [x] Preserve evaluated child arithmetic warnings, in-statement diagnostics
  count behavior, and row-count ordering for mixed scalar projections.
- [x] Add runtime/parser tests for truth tables, precedence, labels, aliases,
  warning counts, diagnostics snapshot behavior, boundary operands, file
  safety, independent handles, and deterministic rejection of unsupported
  forms.
- [x] Register any new test binary in `packages/libmylite/CMakeLists.txt`, or
  document that an existing test binary is reused.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, expression-scope
  control, diagnostics sequencing, three-valued truth correctness, short-
  circuit warning behavior, signed-64 conversion safety, performance, cleanup,
  compatibility wording, and test relevance.
