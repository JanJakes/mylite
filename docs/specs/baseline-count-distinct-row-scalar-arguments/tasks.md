# Baseline COUNT DISTINCT Row-Scalar Arguments Tasks

- [x] Record MySQL 8.4.9 expectations for supported single-expression
  `COUNT(DISTINCT row_scalar_expression)` and literal arguments.
- [x] Add parser support that represents single-expression distinct COUNT
  arguments with existing count expression/literal AST nodes plus the aggregate
  distinct marker.
- [x] Extend count-expression aggregate planning and SQL generation to preserve
  the distinct marker and apply string collation when needed.
- [x] Add runtime tests for literal, integer arithmetic, string expression,
  filtered, empty, all-`NULL`, joined, and mixed aggregate cases.
- [x] Update compatibility documentation and related count-distinct specs.
- [x] Run focused MySQL expectations, focused CTest, format/check gates, and
  release-gate review before commit.
