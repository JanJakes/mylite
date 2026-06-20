# Baseline Grouped Bitwise Aggregate HAVING

## Status

This slice admits grouped `HAVING` predicates over selected bitwise aggregate
results:

```sql
SELECT group_column, BIT_OR(integer_expression) AS bits
FROM source
GROUP BY group_column
HAVING bits > 11
```

It covers selected `BIT_AND()`, `BIT_OR()`, and `BIT_XOR()` aliases and repeated
selected bitwise aggregate expressions in the current grouped aggregate
envelopes. The right-hand comparison value is a nonnegative integer or boolean
literal that fits in MySQL's unsigned 64-bit bitwise aggregate result domain.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline grouped `HAVING`:
  `docs/specs/baseline-having-grouped-aggregate/specs.md`
- Baseline grouped bitwise aggregate alias order:
  `docs/specs/baseline-grouped-bitwise-aggregate-alias-order/specs.md`
- Baseline grouped selected bitwise aggregate expression order:
  `docs/specs/baseline-grouped-selected-bitwise-aggregate-expression-order/specs.md`
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

`packages/libmylite/tests/mysql_baseline_grouped_bitwise_aggregate_having_expectations.sh`
records MySQL 8.4.9 behavior for this slice. Observed behavior:

- `HAVING selected_bitwise_alias <op> literal` filters grouped results by the
  unsigned numeric bitwise aggregate value.
- `HAVING BIT_AND(column) <op> literal`, `HAVING BIT_OR(column) <op> literal`,
  and `HAVING BIT_XOR(column) <op> literal` are accepted when the repeated
  expression matches the selected aggregate result.
- Bitwise aggregate results remain displayed as unsigned decimal text.
- Comparison literals above signed 64-bit range are valid when they fit in the
  unsigned 64-bit bitwise result domain.
- `WHERE`, `ORDER BY`, and `LIMIT` keep the existing grouped aggregate clause
  order: `WHERE` before grouping, `HAVING` after grouping, and ordering/limit
  after `HAVING`.

## Supported Surface

Supported shape:

```sql
SELECT group_projection [, ...],
       bitwise_aggregate(supported_integer_argument) [AS alias] [, ...]
FROM source
[WHERE predicate]
GROUP BY group_key [, ...]
HAVING bitwise_having_operand comparison_operator nonnegative_integer_literal
[ORDER BY supported_grouped_order_key]
[LIMIT supported_limit]
```

`bitwise_aggregate` is `BIT_AND`, `BIT_OR`, or `BIT_XOR`.
`bitwise_having_operand` is either:

- an unqualified alias that uniquely identifies a selected bitwise aggregate;
- a selected descriptor-column bitwise aggregate expression repeated exactly in
  `HAVING`;
- a selected supported row-scalar bitwise aggregate expression repeated exactly
  in `HAVING`.

`comparison_operator` is one of:

```sql
=  <=>  <>  !=  <  <=  >  >=
```

The right-hand literal may be a nonnegative decimal integer in the inclusive
range `0` through `18446744073709551615`, optionally prefixed by unary `+`, or
`TRUE` / `FALSE`. Boolean literals convert to `1` and `0`.

MyLite Lemon-syntax grammar snippets are unchanged from the grouped `HAVING`
and selected aggregate-expression slices:

```lemon
having_predicate_atom ::= having_operand comparison_operator having_integer_value.
having_operand ::= qualified_identifier.
having_operand ::= selected_grouped_aggregate_expression.
selected_grouped_aggregate_expression ::= BIT_AND LPAREN sum_aggregate_argument RPAREN.
selected_grouped_aggregate_expression ::= BIT_OR LPAREN sum_aggregate_argument RPAREN.
selected_grouped_aggregate_expression ::= BIT_XOR LPAREN sum_aggregate_argument RPAREN.
```

## Runtime Semantics

MyLite resolves the `HAVING` operand to one selected grouped aggregate result.
For bitwise aggregate result predicates, generated SQLite SQL wraps the bitwise
aggregate expression in the same internal unsigned order-key helper used by
bitwise aggregate ordering:

```sql
HAVING _mylite_uint64_decimal_order_key(_mylite_bit_or(argument)) > ?
```

The bound comparison value is a fixed-width zero-padded decimal key. Both sides
therefore compare lexicographically in the same order as unsigned 64-bit
numeric values. Public result values remain the original unsigned decimal text.

SQLite still performs scanning, grouping, aggregate stepping, `HAVING`
filtering, ordering, and limiting. MyLite owns MySQL-visible descriptor
resolution, selected-expression matching, unsigned comparison-key construction,
result formatting, diagnostics, and parameter binding.

No SQLite fork hook, public ABI change, file-format change, catalog mutation,
or new dependency is required.

## Non-Goals

This slice does not add:

- negative bitwise `HAVING` comparison literals;
- bitwise aggregate predicates where the aggregate is not selected;
- arbitrary expressions around the bitwise aggregate result, such as
  `BIT_OR(col) + 1 > 10`;
- binary-string bitwise aggregate evaluation;
- `DISTINCT` bitwise aggregate arguments;
- broader `HAVING` boolean composition, parameters, subqueries, function
  literals, or general MySQL expression evaluation;
- executable aggregate windows or full grouping support beyond the existing
  grouped aggregate envelopes.

Unsupported forms continue to return deterministic MyLite diagnostics.

## Validation

Required verification for this slice:

```sh
sh -n packages/libmylite/tests/mysql_baseline_grouped_bitwise_aggregate_having_expectations.sh
packages/libmylite/tests/mysql_baseline_grouped_bitwise_aggregate_having_expectations.sh
cmake --build --preset dev --target mylite_runtime_group_by_single_column_aggregate_test
ctest --preset dev -R '^libmylite\.runtime\.group_by_single_column_aggregate$' --output-on-failure
git diff --check
cmake --workflow --preset check
```
