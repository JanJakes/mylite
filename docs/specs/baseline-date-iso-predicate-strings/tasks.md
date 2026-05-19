# Baseline DATE ISO Predicate Strings Tasks

- [x] Probe MySQL 8.4.9 behavior for `DATE` predicates with datetime-shaped
  string literals, numeric offsets, trailing `Z` / `z`, DML predicates, warning
  counts, and invalid offsets.
- [x] Specify the exact supported MyLite subset and deferred MySQL surfaces.
- [x] Add a MySQL 8.4.9 expectation script for the user-visible behavior.
- [x] Extend descriptor predicate planning so datetime-shaped `DATE` operands
  compare stored dates as midnight datetime text while still executing in
  SQLite.
- [x] Add MyLite runtime coverage for comparison, range, membership, warning,
  DML, and invalid-literal behavior.
- [x] Update compatibility docs for the exact supported `DATE` predicate
  subset.
- [x] Run focused MySQL, build, CTest, and full workflow verification.
- [x] Review the final diff for MySQL evidence, architecture boundaries,
  predicate pushdown, diagnostics, and scope control.
