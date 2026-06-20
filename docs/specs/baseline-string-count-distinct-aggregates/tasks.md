# Baseline String COUNT DISTINCT Aggregates Tasks

- [x] Record MySQL 8.4.9 expectations for nonbinary string
  `COUNT(DISTINCT column)` in ungrouped, mixed, and grouped aggregate contexts.
- [x] Extend aggregate planning so descriptor validation accepts integer and
  nonbinary string arguments.
- [x] Lower string distinct arguments with MyLite's registered string-key
  collation.
- [x] Add runtime tests for ungrouped, mixed, grouped, selected `HAVING`, and
  hidden grouped `ORDER BY` cases.
- [x] Update compatibility documentation and related count-distinct specs.
- [x] Run release-gate checks before commit.
