# Baseline TABLE Statement Tasks

- [x] Read project architecture, query-expression compatibility docs, parser
      and runtime SELECT architecture, result builder APIs, and SQLite fork
      policy.
- [x] Research official MySQL 8.4 `TABLE`, `SELECT`, and limit/tie-order
      documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for visible columns, temporary-table
      shadowing, ordering, limiting, `@@sql_select_limit`, diagnostics, and
      result state.
- [x] Write the independently authored feature spec with MyLite grammar
      snippet, ownership boundaries, unsupported surfaces, diagnostics,
      performance notes, and test plan.
- [x] Add MySQL-runtime expectation script for user-visible behavior.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Extend parser support for top-level `TABLE` by constructing a
      descriptor-backed wildcard `SELECT` AST.
- [x] Add parser tests for accepted and rejected `TABLE` forms.
- [x] Add focused runtime C tests for success paths, diagnostics, result
      metadata, temporary-table shadowing, persistence, file safety,
      independent handles, and unsupported forms.
- [x] Register any new test binary in `packages/libmylite/CMakeLists.txt`.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review final diff for MySQL behavior, descriptor authority, performance,
      cleanup, scope control, and compatibility accuracy.
- [x] Commit and push the implementation slice.
