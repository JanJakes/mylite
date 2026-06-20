# Baseline Grouped AVG Aggregate Alias Order Tasks

- [x] Record MySQL 8.4.9 expectations for selected `AVG()` aggregate aliases
  ordering by exact aggregate value, including `NULL` placement and descending
  `LIMIT` behavior.
- [x] Cover single-key, multiple-key, and exact-rational grouped `AVG()` alias
  ordering in runtime tests.
- [x] Document the compatibility slice and surface it as a narrow green row.
- [x] Run focused MySQL expectations, focused CTest, format/check gates, and
  release-gate review before commit.
