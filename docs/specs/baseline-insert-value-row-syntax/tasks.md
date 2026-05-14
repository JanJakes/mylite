# Baseline INSERT VALUE And ROW Syntax Tasks

## Design And Evidence

- [x] Verify MySQL 8.4.9 behavior for `VALUE`, `VALUES ROW(...)`, empty
      `ROW()`, multi-row forms, row-shape mismatches, mixed-form syntax errors,
      duplicate-key updates, `INSERT IGNORE`, and no-key `REPLACE`.
- [x] Write the independently authored feature specification.
- [x] Add MySQL-runtime expectation script for supported and intentionally
      unsupported syntax.
- [x] Update compatibility docs for the exact syntax aliases admitted.

## Implementation

- [x] Extend parser grammar so `VALUE` ordinary row lists and
      `VALUES ROW(...)` constructor lists normalize to existing insert-row AST
      nodes.
- [x] Preserve parser rejection for `VALUE ROW(...)` and mixed ordinary/row
      constructor lists.
- [x] Reuse existing descriptor-driven values planners without runtime storage
      changes.
- [x] Add focused parser and runtime C coverage.

## Verification

- [x] Run `packages/libmylite/tests/mysql_baseline_insert_value_row_syntax_expectations.sh`.
- [x] Run `cmake --build --preset dev`.
- [x] Run focused CTest entries for parser, default values, row values,
      duplicate-key update, replace values, insert-ignore, and file-backed
      behavior.
- [x] Run `cmake --workflow --preset check`.
- [x] Review with a subagent, amend findings, commit, and push to remote main.
