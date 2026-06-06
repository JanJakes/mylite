# Baseline Joined SELECT SQL_CALC_FOUND_ROWS Tasks

- [x] Verify MySQL 8.4.9 runtime behavior for joined `SQL_CALC_FOUND_ROWS`,
  warnings, found-row state, `LIMIT 0`, cartesian joined rows, left-join
  unmatched rows, and ordinary joined limit-envelope accounting.
- [x] Write the independently authored feature specification.
- [x] Extend the MySQL expectation artifact with joined
  `SQL_CALC_FOUND_ROWS` cases.
- [x] Extend runtime C coverage for inner join, comma join, left join,
  cartesian join, `LIMIT 0`, ordinary joined envelope accounting, and warnings.
- [x] Remove the joined-select planner rejection while preserving existing
  unsupported distinct/grouped/aggregate/source-select diagnostics.
- [x] Update compatibility docs for the exact supported joined subset.
- [x] Run focused build and CTest entries.
- [x] Run the MySQL 8.4.9 expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, descriptor authority,
  no MyLite-side materialization, warning behavior, scope control, and docs.
- [x] Extend joined `DISTINCT SQL_CALC_FOUND_ROWS` to count distinct projected
  rows before `LIMIT`.
