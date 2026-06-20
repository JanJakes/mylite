# Baseline Comparison Result IS Predicates Tasks

- [x] Verify MySQL 8.4.9 comparison-result `IS` behavior.
- [x] Specify supported grammar and runtime scope.
- [x] Add parser grammar for comparison-result `IS NULL`, `IS NOT NULL`,
  `IS UNKNOWN`, and `IS NOT UNKNOWN`.
- [x] Reuse comparison planning and render a parenthesized comparison followed
  by a postfix `IS NULL` or `IS NOT NULL` check.
- [x] Move parser-corpus placeholders into parse/runtime coverage.
- [x] Update compatibility docs.
- [x] Run focused and full verification.
