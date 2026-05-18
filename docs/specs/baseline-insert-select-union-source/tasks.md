# Baseline Insert Select Union Source Tasks

- [x] Read the current `INSERT ... SELECT`, row-scalar source, keyed-target,
  and top-level `UNION` specs, compatibility docs, parser grammar, runtime
  insert-select planner, result builder, and SQLite materialization path.
- [x] Research official MySQL 8.4 `INSERT ... SELECT` and set-operation
  documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for compound insert sources,
  duplicate handling, zero-row sources, row count, diagnostics, same-table
  materialization, and unsupported-ordering boundaries.
- [x] Write the independently authored feature spec with grammar snippets,
  ownership boundaries, SQLite temporary-table handling, diagnostics,
  performance notes, and test plan.
- [x] Add MySQL-runtime expectation script for user-visible behavior.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Extend parser support so `INSERT ... SELECT` can carry a compound source
  without admitting unrelated query-expression grammar.
- [x] Add analyzer/planner support for compound source classification, branch
  planning, branch column-count validation, descriptor compatibility checks,
  and cleanup.
- [x] Implement SQLite temporary-table materialization for compound sources
  using descriptor-built SQL, unique numbered parameters, and existing
  validation/insertion paths.
- [x] Add focused parser/runtime C tests for success paths, diagnostics,
  persistence, same-table source/target, file-format safety, independent
  handles, and unsupported forms.
- [x] Register any new test binary in `packages/libmylite/CMakeLists.txt`.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review final diff for MySQL behavior, descriptor authority, performance,
  cleanup, scope control, and compatibility accuracy.
- [x] Commit and push the implementation slice.
