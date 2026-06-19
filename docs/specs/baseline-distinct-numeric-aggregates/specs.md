# Baseline DISTINCT Aggregate Arguments

## Status

This feature extends the current descriptor-backed aggregate envelope with
executable `DISTINCT` arguments for `SUM()`, `AVG()`, `MIN()`, and `MAX()`.
It builds on the mixed ungrouped aggregate, grouped aggregate, shared
row-scalar aggregate-argument, and result-formatting paths already used by the
non-`DISTINCT` forms.

This is not full aggregate expression support. It supports `DISTINCT` only for
the same descriptor-column and supported row-scalar argument shapes that the
current non-`DISTINCT` aggregate paths already execute.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline aggregate specs:
  `docs/specs/baseline-sum-aggregate/specs.md`,
  `docs/specs/baseline-avg-aggregate/specs.md`, and
  `docs/specs/baseline-min-max-aggregate/specs.md`
- Baseline grouped aggregate specs:
  `docs/specs/baseline-group-by-single-column-aggregate/specs.md`
  and `docs/specs/baseline-group-by-multiple-aggregates/specs.md`
- Shared row-scalar aggregate arguments:
  `docs/specs/baseline-universal-row-scalar-expression-contexts/specs.md`
- MySQL 8.4 Reference Manual, aggregate functions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- SQLite aggregate function documentation:
  https://www.sqlite.org/lang_aggfunc.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation scripts
`packages/libmylite/tests/mysql_baseline_sum_aggregate_expectations.sh`,
`packages/libmylite/tests/mysql_baseline_avg_aggregate_expectations.sh`,
`packages/libmylite/tests/mysql_baseline_min_max_aggregate_expectations.sh`,
and
`packages/libmylite/tests/mysql_baseline_group_by_single_column_aggregate_expectations.sh`
record runtime probes for this feature.

Observed behavior that shapes this slice:

- `SUM(DISTINCT expr)` sums the distinct non-`NULL` values of the aggregate
  argument expression.
- `AVG(DISTINCT expr)` averages the distinct non-`NULL` values of the aggregate
  argument expression.
- `MIN(DISTINCT expr)` and `MAX(DISTINCT expr)` are accepted. `DISTINCT` does
  not change the minimum or maximum value, but it is part of the legal MySQL
  syntax surface.
- Empty matched sets, and matched sets where the distinct value set is empty
  after ignoring `NULL`, return `NULL`.
- In grouped queries, the distinct value set is per group.
- Default result labels preserve the source expression text, for example
  `SUM(DISTINCT n)` and `AVG(DISTINCT n + 1)`.
- Aggregate-window `OVER` forms remain syntax-admitted but not executable in
  this slice.

## Scope

The implementation must add:

- parser and AST support for `DISTINCT` modifiers on `MIN()`, `MAX()`,
  no-space-sensitive `SUM()`, and `AVG()`;
- runtime planning for `DISTINCT` over supported descriptor-column and
  row-scalar aggregate arguments in the existing mixed ungrouped aggregate
  envelope;
- runtime planning for the same supported forms in the existing grouped
  aggregate envelope;
- generated SQLite aggregate SQL with `DISTINCT` applied to the aggregate
  argument;
- `AVG(DISTINCT expr)` result formatting through the existing MyLite
  `SUM(expr), COUNT(expr)` decomposition, using `SUM(DISTINCT expr)` and
  `COUNT(DISTINCT expr)` for the same argument expression;
- result labels from the original MySQL expression source span unless an
  explicit select-item alias is present;
- MySQL-runtime-verified expectation updates and focused runtime regression
  tests for duplicate-sensitive `SUM(DISTINCT ...)` and `AVG(DISTINCT ...)`
  behavior.

## Non-Goals

This feature must not implement:

- broad aggregate argument expressions outside the current supported
  row-scalar aggregate-argument envelope;
- `DISTINCT` for bitwise, statistical, JSON, or other aggregate families;
- `COUNT(DISTINCT expr[, expr...])` beyond the already documented descriptor
  column subset;
- exact decimal widening beyond the current signed-64 aggregate result
  envelope;
- binary string comparison semantics for `MIN()` / `MAX()`;
- aggregate-local ordering, aggregate filters, aggregate windows, full grouping,
  joins beyond already supported grouped source shapes, CTE-specific aggregate
  behavior, or optimizer parity.

## Ownership Boundary

- Lexer/parser/AST own syntax admission and the lightweight distinct marker.
- The aggregate planners own deciding whether the distinct marker applies in
  the supported ungrouped and grouped execution envelopes.
- SQL builders own emitting descriptor-safe SQLite SQL. They must build SQL
  from catalog descriptors, quoted physical identifiers, and bound parameters
  for row-scalar constants; user SQL text is not spliced into executable SQL.
- SQLite owns scanning, grouping, distinct aggregate state, pager behavior, and
  ordinary aggregate evaluation through public aggregate syntax. No SQLite fork
  hook is required for this slice.
- MyLite owns MySQL-facing result labels, signed-64 envelope diagnostics, AVG
  text formatting, and compatibility documentation.

## Grammar

The Lemon grammar extension is:

```lemon
expression ::= MIN LPAREN DISTINCT expression RPAREN aggregate_window_opt.
expression ::= MAX LPAREN DISTINCT expression RPAREN aggregate_window_opt.
expression ::= SUM LPAREN DISTINCT expression RPAREN aggregate_window_opt.
expression ::= AVG LPAREN DISTINCT expression RPAREN aggregate_window_opt.

selected_grouped_aggregate_expression ::=
    MIN LPAREN DISTINCT sum_aggregate_argument RPAREN.
selected_grouped_aggregate_expression ::=
    MAX LPAREN DISTINCT sum_aggregate_argument RPAREN.
selected_grouped_aggregate_expression ::=
    SUM LPAREN DISTINCT sum_aggregate_argument RPAREN.
selected_grouped_aggregate_expression ::=
    AVG LPAREN DISTINCT avg_aggregate_argument RPAREN.
```

The parser must attach an aggregate distinct marker as a child after the
aggregate argument. The marker must not replace the argument node, because
existing planning code expects the argument at child index `0`.

## Runtime Semantics

For supported descriptor-column and row-scalar aggregate arguments:

- `SUM(DISTINCT expr)` emits `SUM(DISTINCT <planned expr>)`.
- `AVG(DISTINCT expr)` emits `SUM(DISTINCT <planned expr>),
  COUNT(DISTINCT <planned expr>)`; MyLite formats the final value using the
  same four-fractional-digit AVG formatter used by non-`DISTINCT` AVG.
- `MIN(DISTINCT expr)` emits `MIN(DISTINCT <planned expr>)`.
- `MAX(DISTINCT expr)` emits `MAX(DISTINCT <planned expr>)`.

The implementation must preserve existing diagnostics for unsupported
argument shapes, unknown columns, unsupported source shapes, signed-64 overflow,
and executable aggregate-window forms.

## Tests

Required coverage:

- parser AST tests for the generic aggregate distinct marker on
  `MIN(DISTINCT ...)`, `SUM(DISTINCT ...)`, and `AVG(DISTINCT ...)`;
- ungrouped runtime tests where duplicate values prove that
  `SUM(DISTINCT n)` and `AVG(DISTINCT n)` differ from their non-`DISTINCT`
  counterparts;
- row-scalar duplicate-sensitive runtime tests for
  `SUM(DISTINCT n + 1)` and `AVG(DISTINCT n + 1)`;
- descriptor and row-scalar string runtime tests for
  `MIN(DISTINCT expr)` and `MAX(DISTINCT expr)`;
- grouped runtime tests for `MIN(DISTINCT n)`, `MAX(DISTINCT n)`,
  `SUM(DISTINCT n)`, and `AVG(DISTINCT n)`;
- MySQL 8.4.9 expectation scripts for each aggregate family and grouped
  aggregate use.

Verification commands:

```sh
sh -n packages/libmylite/tests/mysql_baseline_sum_aggregate_expectations.sh
sh -n packages/libmylite/tests/mysql_baseline_avg_aggregate_expectations.sh
sh -n packages/libmylite/tests/mysql_baseline_min_max_aggregate_expectations.sh
sh -n packages/libmylite/tests/mysql_baseline_group_by_single_column_aggregate_expectations.sh
cmake --build --preset dev --target \
  mylite_parser_expression_aggregate_test \
  mylite_runtime_sum_aggregate_test \
  mylite_runtime_avg_aggregate_test \
  mylite_runtime_min_max_aggregate_test \
  mylite_runtime_group_by_single_column_aggregate_test
ctest --preset dev -R 'libmylite\.(parser\.expression_aggregate|runtime\.(sum_aggregate|avg_aggregate|min_max_aggregate|group_by_single_column_aggregate))$' --output-on-failure
```

Release-gate verification additionally runs `cmake --workflow --preset check`.

## Compatibility Documentation

Update `COMPATIBILITY.md` and
`docs/compatibility/functions-aggregate.md` to state that
`SUM(DISTINCT expr)`, `AVG(DISTINCT expr)`, `MIN(DISTINCT expr)`, and
`MAX(DISTINCT expr)` execute only inside the documented descriptor-backed
ungrouped and grouped aggregate envelopes. Keep remaining gaps explicit:
windows, full grouping, aggregate ordering, binary string comparison, and exact
decimal widening are still limited.
