# Baseline Empty INSERT Values Tasks

## Design And Evidence

- [x] Verify MySQL 8.4.9 behavior for explicit and omitted empty `VALUES` rows,
      mixed row-shape diagnostics, strict no-default errors, `INSERT IGNORE`
      warnings, auto-increment, no-key `REPLACE`, duplicate-key updates, and
      `INSERT ... SELECT` mismatch behavior.
- [x] Write the independently authored feature specification.
- [x] Add MySQL-runtime expectation script for supported and intentionally
      deferred behavior.
- [x] Update compatibility docs for the exact empty-row insert surface.

## Implementation

- [x] Preserve parser distinction between omitted column lists and explicit
      empty `()` column lists.
- [x] Admit empty insert rows in `INSERT ... VALUES` and `REPLACE ... VALUES`.
- [x] Extend the insert planner so omitted-column empty rows and explicit empty
      column lists plan zero explicit targets and materialize descriptor
      defaults.
- [x] Keep ordinary omitted-column nonempty rows mapped to visible descriptor
      columns.
- [x] Preserve existing descriptor default, auto-increment, duplicate-key,
      foreign-key, `INSERT IGNORE`, `REPLACE`, result, and transaction behavior.
- [x] Add focused parser and runtime C coverage.

## Verification

- [x] Run `packages/libmylite/tests/mysql_baseline_empty_insert_values_expectations.sh`.
- [x] Run `cmake --build --preset dev`.
- [x] Run the new CTest entry plus parser, insert, insert-ignore,
      duplicate-key, replace, defaults, auto-increment, and file-backed entries.
- [x] Run `cmake --workflow --preset check`.
- [x] Review with a subagent, amend findings, commit, and push to remote main.
