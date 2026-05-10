# Baseline Fixed SET System Variables Tasks

- [x] Read current system-variable, diagnostics, parser, runtime, connection
  character set `SET`, statement-context, storage/file-format, and
  compatibility docs.
- [x] Review official MySQL 8.4 `SET` variable-assignment, system-variable,
  and SQL-mode documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for accepted scope spellings,
  fixed-value assignments, default SQL mode, row-count state, warning counts,
  unknown/read-only variables, global scope, mutable values, assignment lists,
  and broader forms deferred by this slice.
- [x] Write independently authored feature spec with ownership boundaries,
  grammar snippet, target resolution, admitted values, diagnostics,
  unsupported forms, performance/storage notes, and test plan.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the specified but not yet
  implemented subset.
- [x] Commit and push the start-feature artifacts.
- [ ] Add parser/AST support for limited fixed `SET` system-variable
  statements.
- [ ] Add MyLite-owned runtime validation/execution for admitted no-op
  assignments.
- [ ] Preserve result shape, affected rows, warning count, row-count state,
  diagnostics clearing, catalog generations, SQLite schema generation, and
  file preamble invariants.
- [ ] Add parser/runtime tests for accepted scope/value forms, diagnostics,
  file safety, independent handles, and deterministic rejection of unsupported
  forms.
- [ ] Register any new runtime test binary in `packages/libmylite/CMakeLists.txt`.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, scope control,
  diagnostics, row-count state, fixed-value semantics, performance, cleanup,
  compatibility wording, and test relevance.
