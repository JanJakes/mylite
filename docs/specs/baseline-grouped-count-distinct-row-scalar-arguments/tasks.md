# Baseline Grouped COUNT DISTINCT Row-Scalar Arguments Tasks

- [x] Record MySQL 8.4.9 expectations for grouped single-expression
  `COUNT(DISTINCT row_scalar_expression)` and literal arguments.
- [x] Extend grouped aggregate planning to treat count literals as grouped
  count row-scalar aggregate arguments.
- [x] Apply MyLite's string-key collation to grouped row-scalar aggregate SQL
  when the planned argument is string-valued.
- [x] Add runtime tests for grouped distinct literals, arithmetic expressions,
  control-flow expressions, string expressions, `HAVING`, and aggregate
  expression ordering.
- [x] Update compatibility documentation and related count-distinct specs.
- [x] Run focused MySQL expectations, focused CTest, format/check gates, and
  release-gate review before commit.
