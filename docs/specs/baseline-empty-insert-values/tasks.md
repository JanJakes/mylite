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

- [ ] Preserve parser distinction between omitted column lists and explicit
      empty `()` column lists.
- [ ] Admit empty insert rows in `INSERT ... VALUES` and `REPLACE ... VALUES`.
- [ ] Extend the insert planner so omitted-column empty rows and explicit empty
      column lists plan zero explicit targets and materialize descriptor
      defaults.
- [ ] Keep ordinary omitted-column nonempty rows mapped to visible descriptor
      columns.
- [ ] Preserve existing descriptor default, auto-increment, duplicate-key,
      foreign-key, `INSERT IGNORE`, `REPLACE`, result, and transaction behavior.
- [ ] Add focused parser and runtime C coverage.

## Verification

- [ ] Run `packages/libmylite/tests/mysql_baseline_empty_insert_values_expectations.sh`.
- [ ] Run `cmake --build --preset dev`.
- [ ] Run the new CTest entry plus parser, insert, insert-ignore,
      duplicate-key, replace, defaults, auto-increment, and file-backed entries.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review with a subagent, amend findings, commit, and push to remote main.
