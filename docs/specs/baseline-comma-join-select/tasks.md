# Baseline Comma Join SELECT Tasks

- [x] Read project architecture, compatibility docs, existing explicit join
      specs, parser grammar, planner/runtime join code, tests, and SQLite fork
      policy.
- [x] Research official MySQL 8.4 `SELECT` and `JOIN` documentation for comma
      table references.
- [x] Probe MySQL 8.4.9 runtime behavior for two-source comma joins, column
      order, aliasing, ambiguity, diagnostics, warnings, and `ROW_COUNT()`.
- [x] Write the independently authored feature spec with grammar snippets,
      ownership boundaries, unsupported surfaces, diagnostics, performance
      notes, and test plan.
- [x] Add MySQL-runtime expectation script for user-visible behavior.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Extend parser support for `FROM table_source, table_source`.
- [x] Add parser tests for accepted and rejected comma join forms.
- [x] Extend focused runtime C tests for success paths, diagnostics,
      persistence, temporary-table shadowing, file safety, independent handles,
      and unsupported forms.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review final diff for MySQL behavior, descriptor authority, performance,
      cleanup, scope control, and compatibility accuracy.
- [ ] Commit and push the implementation slice.
