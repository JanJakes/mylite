# Baseline INSERT SELECT ON DUPLICATE KEY UPDATE Tasks

- [x] Read current architecture, compatibility docs, insert-select specs, ODKU
      specs, parser grammar, runtime insert-select planning, runtime duplicate
      update execution, and tests.
- [x] Research official MySQL 8.4 `INSERT ... SELECT` and
      `INSERT ... ON DUPLICATE KEY UPDATE` documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for table-backed, row-scalar,
      compound, no-key, no-op, changed duplicate, warnings, diagnostics,
      same-table materialization, `IGNORE`, and auto-increment target behavior.
- [x] Write the independently authored feature spec with grammar snippets,
      ownership boundaries, runtime semantics, diagnostics, performance notes,
      unsupported boundaries, and test plan.
- [x] Add a MySQL-runtime expectation script for the newly admitted behavior
      and documented unsupported boundaries.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Extend parser/AST support for an optional duplicate-key tail on existing
      insert-select source forms.
- [x] Reuse existing duplicate-update planning for insert-select target plans,
      preserving descriptor-driven assignment and `VALUES()` resolution.
- [x] Reject insert-select ODKU with `IGNORE` and auto-increment targets for
      this phase.
- [x] Route table-backed, row-scalar, and compound selected rows through the
      existing duplicate-update executor without buffering selected rows in C
      memory.
- [x] Add focused parser/runtime C tests and register any new test binary.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [ ] Review with a subagent, amend if needed, commit, and push `main`.
