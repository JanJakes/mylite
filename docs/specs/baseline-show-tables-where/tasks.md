# Baseline SHOW TABLES WHERE Tasks

## Design And Evidence

- [x] Create the feature spec under
  `docs/specs/baseline-show-tables-where/`.
- [x] Verify MySQL 8.4.9 behavior for ordinary and `FULL` `SHOW TABLES`
  `WHERE` filters, output headers, diagnostics, warning count, and row-count
  state.
- [x] Update the MySQL runtime expectation artifact.

## Implementation

- [x] Extend parser grammar and parser tests for `SHOW TABLES ... WHERE`.
- [x] Resolve `SHOW TABLES` filter nodes as either `LIKE` or `WHERE`.
- [x] Evaluate supported output-column predicates for ordinary and `FULL`
  `SHOW TABLES`.
- [x] Preserve existing schema resolution, reserved-name rejection, result
  labels, warning count, and row-count behavior.

## Tests And Docs

- [x] Extend fast runtime tests for successful filters, diagnostics,
  rename/drop/reopen behavior, and unsupported syntax.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs for the exact
  supported subset.
- [x] Run the updated MySQL expectation script.
- [x] Run targeted parser/runtime CTest entries.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, commit atomically, and push `main`.
