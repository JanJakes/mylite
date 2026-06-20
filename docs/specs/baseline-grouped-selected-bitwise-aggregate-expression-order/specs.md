# Baseline Grouped Selected Bitwise Aggregate Expression Order

## Status

This slice admits grouped `ORDER BY` keys that repeat a selected bitwise
aggregate expression:

```sql
SELECT group_column, BIT_OR(integer_column) AS bits
FROM source
GROUP BY group_column
ORDER BY BIT_OR(integer_column) [ASC|DESC]
```

The repeated expression must match one selected bitwise aggregate result in the
current grouped aggregate planner. This extends bitwise alias ordering. Hidden
bitwise aggregate order keys are covered separately by
`docs/specs/baseline-hidden-grouped-aggregate-order-keys/specs.md`.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline grouped bitwise aggregate alias order:
  `docs/specs/baseline-grouped-bitwise-aggregate-alias-order/specs.md`
- Baseline selected aggregate expression order:
  `docs/specs/baseline-grouped-selected-aggregate-expression-order/specs.md`
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

`packages/libmylite/tests/mysql_baseline_group_by_multiple_aggregates_expectations.sh`
records MySQL 8.4.9 behavior for repeated selected bitwise aggregate order
keys. Observed behavior:

- `ORDER BY BIT_AND(column)`, `ORDER BY BIT_OR(column)`, and
  `ORDER BY BIT_XOR(column)` sort by the selected unsigned numeric aggregate
  value.
- The selected bitwise aggregate result is still displayed as unsigned decimal
  text.
- Descending bitwise expression order and `LIMIT` follow normal grouped
  `ORDER BY` behavior.

## Supported Surface

Supported shape:

```sql
SELECT group_projection [, ...],
       bitwise_aggregate(integer_descriptor_column) [AS alias] [, ...]
FROM source
[WHERE predicate]
GROUP BY group_key [, ...]
[HAVING supported_group_having]
ORDER BY bitwise_aggregate(integer_descriptor_column) [ASC|DESC]
       [, supported_grouped_order_key ...]
[LIMIT supported_limit]
```

`bitwise_aggregate` is `BIT_AND`, `BIT_OR`, or `BIT_XOR`. This slice covers
descriptor-column bitwise aggregate arguments. The repeated aggregate
expression must match exactly one selected aggregate result after MyLite
descriptor resolution.

MyLite Lemon-syntax grammar snippets:

```lemon
selected_grouped_aggregate_expression ::= BIT_AND LPAREN sum_aggregate_argument RPAREN.
selected_grouped_aggregate_expression ::= BIT_OR LPAREN sum_aggregate_argument RPAREN.
selected_grouped_aggregate_expression ::= BIT_XOR LPAREN sum_aggregate_argument RPAREN.
```

Selected bitwise aggregate-expression ordering for supported row-scalar
aggregate arguments is covered by
`docs/specs/baseline-grouped-selected-row-scalar-aggregate-expression-order/specs.md`.

## Runtime Semantics

MyLite resolves the `ORDER BY` bitwise aggregate expression with the same
grouped aggregate planner used for selected aggregate items. If the planned
expression matches one selected bitwise aggregate, the generated SQLite
`ORDER BY` reuses the selected aggregate's existing unsigned decimal order-key
builder:

```sql
ORDER BY _mylite_uint64_decimal_order_key(_mylite_bit_or(argument)) [ASC|DESC]
```

The public result value remains the selected aggregate's unsigned decimal text.
SQLite continues to perform scanning, grouping, aggregation, ordering, and
limiting. MyLite owns descriptor resolution, selected-expression matching,
unsigned bitwise order-key construction, result formatting, diagnostics, and
parameter binding.

No SQLite fork hook is required.

## Non-Goals

This slice does not add:

- binary-string bitwise aggregate evaluation;
- `DISTINCT` bitwise aggregate arguments;
- grouped `HAVING` predicates on bitwise aggregate results;
- statistical aggregate-expression order keys, which are tracked by
  `docs/specs/baseline-grouped-selected-statistical-aggregate-expression-order/specs.md`;
- broader aggregate argument domains, full grouping, or executable aggregate
  windows.

Unsupported forms continue to return deterministic MyLite diagnostics.
