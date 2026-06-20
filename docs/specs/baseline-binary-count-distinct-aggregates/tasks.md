# Baseline Binary COUNT DISTINCT Aggregates Tasks

- [x] Record MySQL 8.4.9 expectations for binary string
  `COUNT(DISTINCT column)` in ungrouped, mixed, and grouped aggregate contexts.
- [x] Extend aggregate planning so descriptor validation accepts integer,
  nonbinary string, and binary string arguments.
- [x] Keep binary distinct arguments uncollated while preserving string-key
  collation for nonbinary string arguments.
- [x] Add runtime tests for ungrouped, mixed, grouped, multiple-key grouped,
  joined grouped, selected `HAVING`, filtered, fixed `BINARY`, `VARBINARY`,
  `BLOB`, and hidden grouped `ORDER BY` cases.
- [x] Update compatibility documentation and related count-distinct specs.
- [x] Run release-gate checks before commit.
