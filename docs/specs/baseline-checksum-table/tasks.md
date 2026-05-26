# Baseline CHECKSUM TABLE Tasks

- [x] Review existing parser, descriptor resolution, result-building,
      diagnostics, transaction, savepoint, and file-backed storage paths.
- [x] Research official MySQL 8.4 `CHECKSUM TABLE` documentation and implicit
      commit rules.
- [x] Verify MySQL 8.4.9 behavior for result rows, options, unavailable targets,
      duplicate names, row counts, warning counts, and syntax boundaries.
- [x] Write independently authored feature spec and scope boundaries.
- [x] Add MySQL-runtime expectation artifact for this feature surface.
- [x] Add parser/AST support for the admitted `CHECKSUM TABLE` grammar.
- [x] Add runtime result synthesis and target-warning diagnostics.
- [x] Preserve MySQL implicit-commit and savepoint-clear behavior.
- [x] Add fast parser/runtime C tests for success paths, options, diagnostics,
      transaction behavior, persistence, and unsupported forms.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run the feature MySQL expectation script.
- [x] Run targeted parser/runtime/diagnostics CTest entries.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, commit atomically, push `main`, and run a
      subagent release-gate review.
