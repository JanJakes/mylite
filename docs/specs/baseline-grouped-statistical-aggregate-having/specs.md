# Baseline Grouped Statistical Aggregate HAVING

## Status

This slice admits grouped `HAVING` predicates over selected statistical
aggregate results:

```sql
SELECT group_column, STDDEV_POP(integer_expression) AS spread
FROM source
GROUP BY group_column
HAVING spread > 0
```

It covers selected `STD()`, `STDDEV()`, `STDDEV_POP()`, `STDDEV_SAMP()`,
`VAR_POP()`, `VAR_SAMP()`, and `VARIANCE()` aliases and repeated selected
aggregate expressions in the current grouped aggregate envelopes.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline statistical aggregates:
  `docs/specs/baseline-statistical-aggregates/specs.md`
- Baseline grouped statistical aggregate alias order:
  `docs/specs/baseline-grouped-statistical-aggregate-alias-order/specs.md`
- Baseline grouped selected statistical aggregate expression order:
  `docs/specs/baseline-grouped-selected-statistical-aggregate-expression-order/specs.md`
- Baseline grouped `HAVING`:
  `docs/specs/baseline-having-grouped-aggregate/specs.md`
- MySQL 8.4 Reference Manual, `SELECT` syntax:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, aggregate functions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

`packages/libmylite/tests/mysql_baseline_statistical_aggregates_expectations.sh`
records MySQL 8.4.9 behavior for this slice. Observed behavior:

- A selected statistical aggregate alias may be used as a grouped `HAVING`
  operand.
- A selected statistical aggregate expression may be repeated in grouped
  `HAVING` when it matches the selected aggregate result.
- Population aggregate results compare as double values; sample aggregate
  results are `NULL` for groups with fewer than two non-`NULL` inputs.
- `IS NULL` filters sample aggregate groups with insufficient non-`NULL`
  arguments.
- `STD()` and `STDDEV()` behave as `STDDEV_POP()`.
- `VARIANCE()` behaves as `VAR_POP()`.

## Supported Surface

Supported shape:

```sql
SELECT group_projection [, ...],
       statistical_aggregate(supported_integer_argument) [AS alias] [, ...]
FROM source
[WHERE predicate]
GROUP BY group_key [, ...]
HAVING statistical_having_operand comparison_operator integer_literal
   | statistical_having_operand IS [NOT] NULL
[ORDER BY supported_grouped_order_key]
[LIMIT supported_limit]
```

`statistical_aggregate` is one of `STD`, `STDDEV`, `STDDEV_POP`,
`STDDEV_SAMP`, `VAR_POP`, `VAR_SAMP`, or `VARIANCE`.
`statistical_having_operand` is either:

- an unqualified alias that uniquely identifies a selected statistical
  aggregate;
- a selected descriptor-column statistical aggregate expression repeated
  exactly in `HAVING`;
- a selected supported row-scalar statistical aggregate expression repeated
  exactly in `HAVING`.

The comparison operator and integer literal domain are the current grouped
`HAVING` baseline domain:

```sql
=  <=>  <>  !=  <  <=  >  >=
```

MyLite Lemon-syntax grammar snippets are unchanged from the grouped `HAVING`
and statistical aggregate slices:

```lemon
having_predicate_atom ::= having_operand comparison_operator having_integer_value.
having_predicate_atom ::= having_operand IS NULL.
having_predicate_atom ::= having_operand IS NOT NULL.
having_operand ::= qualified_identifier.
having_operand ::= selected_grouped_aggregate_expression.
selected_grouped_aggregate_expression ::= IDENTIFIER LPAREN sum_aggregate_argument RPAREN.
/* Builder specialization:
   If IDENTIFIER is STD, STDDEV, STDDEV_POP, STDDEV_SAMP, VAR_POP, VAR_SAMP,
   or VARIANCE, return the matching statistical aggregate node; otherwise
   reject the production as a syntax error. */
```

## Runtime Semantics

MyLite resolves the `HAVING` operand to one selected grouped aggregate result.
For statistical aggregate result predicates, generated SQLite SQL reuses the
same registered aggregate UDF used for projection and ordering:

```sql
HAVING _mylite_stddev_pop(argument) > ?
```

The comparison value is bound as the existing grouped `HAVING` signed integer
literal. SQLite compares the aggregate UDF's double or `NULL` result against
the bound value. Public result values remain the selected aggregate's formatted
double or `NULL` text.

SQLite still performs scanning, grouping, aggregate stepping, `HAVING`
filtering, ordering, and limiting. MyLite owns MySQL-visible descriptor
resolution, selected-expression matching, statistical function-name
specialization, result formatting, diagnostics, and parameter binding.

No SQLite fork hook, public ABI change, file-format change, catalog mutation,
or new dependency is required.

## Non-Goals

This slice does not add:

- noninteger comparison literals or MySQL decimal/double literal coercion
  parity in grouped `HAVING`;
- statistical aggregate predicates where the aggregate is not selected;
- arbitrary expressions around the statistical aggregate result, such as
  `STDDEV_POP(col) + 1 > 10`;
- `DISTINCT` statistical aggregate arguments;
- string-to-double coercion warnings or broader noninteger argument domains;
- broader `HAVING` boolean composition, parameters, subqueries, function
  literals, or general MySQL expression evaluation;
- executable aggregate windows or full grouping support beyond the existing
  grouped aggregate envelopes.

Unsupported forms continue to return deterministic MyLite diagnostics.

## Validation

Required verification for this slice:

```sh
sh -n packages/libmylite/tests/mysql_baseline_statistical_aggregates_expectations.sh
packages/libmylite/tests/mysql_baseline_statistical_aggregates_expectations.sh
cmake --build --preset dev --target mylite_runtime_statistical_aggregates_test
ctest --preset dev -R '^libmylite\.runtime\.statistical_aggregates$' --output-on-failure
git diff --check
cmake --workflow --preset check
```
