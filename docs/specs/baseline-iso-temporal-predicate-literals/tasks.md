# Baseline ISO Temporal Predicate Literals Tasks

- [x] Read the MyLite architecture, temporal type, predicate, SQL mode, and
  time-zone specs relevant to descriptor-backed temporal predicates.
- [x] Verify MySQL 8.4.9 behavior for `T` separators, numeric offsets, offset
  range limits, session time-zone effects, invalid offsets, and trailing `Z`
  behavior.
- [x] Specify the independently authored feature scope and grammar/semantic
  snippets before implementation.
- [x] Add a MySQL-runtime expectation script for the admitted and explicitly
  deferred behavior.
- [x] Implement MyLite-owned predicate conversion for `DATETIME` `T` separator
  and numeric-offset string literals.
- [x] Implement MyLite-owned predicate conversion for `TIMESTAMP` `T` separator
  and numeric-offset string literals using the current fixed-UTC timestamp
  storage baseline.
- [x] Add fast runtime tests for SELECT, UPDATE, DELETE, `BETWEEN`, `IN`,
  session-offset, invalid-offset, and trailing-`Z` cases.
- [x] Update compatibility docs without overclaiming general temporal
  conversion or trailing-`Z` behavior.
- [x] Run the MySQL expectation script, focused CTest entries, and full check
  workflow.
- [x] Review the final diff for descriptor authority, warning/diagnostic scope,
  no SQLite temporal parser reliance, no row materialization, file-format
  safety, cleanup safety, and compatibility-matrix accuracy.
