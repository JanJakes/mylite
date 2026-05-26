# Baseline UPDATE IGNORE Modifier Tasks

- [x] Verify MySQL 8.4.9 behavior for modifier order, `LOW_PRIORITY`,
      strict-mode `IGNORE` adjustment, warning counts, changed-row affected
      counts, `ORDER BY` / `LIMIT`, `LIMIT 0`, no-default `DEFAULT`, multiple
      assignments, and duplicate-key behavior to defer.
- [x] Write the independently authored feature spec with MyLite grammar
      snippets, ownership boundaries, physical SQLite handling, diagnostics,
      performance notes, and out-of-scope behavior.
- [x] Add the MySQL-runtime expectation artifact for the user-visible behavior
      introduced by this phase.
- [x] Extend the parser/AST for `UPDATE [LOW_PRIORITY] [IGNORE]` single-table
      statements without admitting `UPDATE IGNORE LOW_PRIORITY` or joined-update
      modifiers.
- [x] Add planner state for accepted `LOW_PRIORITY` and `IGNORE`, reject
      unsupported `UPDATE IGNORE` key/auto-increment assignments before
      execution, and keep descriptor resolution authoritative.
- [x] Route supported update assignment conversion through `ignore_errors =
      true` when `UPDATE IGNORE` is present, including warning multiplication
      for matched rows and `LIMIT 0` / no-match suppression.
- [x] Add focused parser/runtime tests for accepted/rejected syntax,
      strict-mode adjustment, warning records, affected rows, ordering/limits,
      deferred key assignment diagnostics, and persistence.
- [x] Update `COMPATIBILITY.md` and compatibility detail docs for the exact
      implemented subset without claiming duplicate-key skip behavior.
- [x] Run the MySQL expectation script, focused CTest entries, and
      `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL evidence, scope control, descriptor
      authority, physical SQL safety, warning-count correctness, zero-init
      cleanup, and compatibility docs.
