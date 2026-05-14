# Baseline Table Maintenance Statements Tasks

- [x] Review existing parser, descriptor resolution, result-building,
      transaction, savepoint, and file-backed storage paths.
- [x] Research official MySQL 8.4 maintenance statement docs and implicit
      commit rules.
- [x] Verify MySQL 8.4.9 behavior for admitted result rows, options, unknown
      names, duplicate names, row counts, warning counts, and implicit commit.
- [x] Write independently authored feature spec and scope boundaries.
- [x] Add MySQL-runtime expectation artifact for this feature surface.
- [ ] Add parser/AST support for the admitted maintenance grammar.
- [ ] Add runtime result synthesis for supported base-table targets and
      unavailable-target result rows.
- [ ] Preserve MySQL implicit-commit and savepoint-clear behavior.
- [ ] Add fast parser/runtime C tests for success paths, options,
      diagnostics/result rows, transaction behavior, persistence, and
      unsupported forms.
- [ ] Update compatibility docs for the exact supported subset.
- [ ] Run the feature MySQL expectation script.
- [ ] Run targeted parser/runtime CTest entries.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff, commit atomically, push `main`, and run a
      subagent release-gate review.
