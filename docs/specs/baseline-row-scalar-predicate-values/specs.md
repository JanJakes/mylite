# Baseline Row-Scalar Predicate Values

## Status

Implemented in this slice.

## Sources

- MySQL 8.4 Reference Manual, Expressions:
  https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- MySQL 8.4 Reference Manual, Comparison Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- MySQL 8.4.9 runtime observations recorded in
  `packages/libmylite/tests/mysql_baseline_where_and_predicates_expectations.sh`.

## Scope

MySQL accepts expressions in comparison operands, `BETWEEN` bounds, and `IN`
list values. MyLite does not yet expose a single unrestricted table-backed
expression IR everywhere, but its row-scalar expression planner already covers
many descriptor-backed value expressions.

This slice admits supported row-scalar value expressions in the existing
descriptor-backed `WHERE` predicate envelope:

- comparison right-hand values;
- `BETWEEN` and `NOT BETWEEN` lower and upper bounds;
- `IN` and `NOT IN` list entries;
- the shared single-table `SELECT`, `UPDATE`, and `DELETE` predicate paths.

The admitted value expression must be a MyLite-supported row-scalar expression
and must contain a row operand. This prevents source-free constant expression
subtrees from being rerouted through the table-backed retry path.

## Semantics

Supported value expressions are planned through the existing
`plan_row_scalar_expression()` machinery and rendered into the generated SQLite
predicate SQL. SQLite still scans and filters rows; MyLite supplies MySQL-shaped
function, conversion, collation, JSON, temporal, and warning behavior through
the row-scalar expression layer.

The slice covers direct supported row-scalar values such as:

```sql
WHERE n = CAST(n_text AS SIGNED)
WHERE n BETWEEN CAST(n_text AS SIGNED) AND CAST(n_text AS SIGNED)
WHERE n IN (CAST(n_text AS SIGNED), 99)
WHERE n BETWEEN JSON_LENGTH(j) AND JSON_LENGTH(j)
WHERE n BETWEEN DATEDIFF(d2, d1) AND DATEDIFF(d2, d1)
```

It also preserves the previously implemented arithmetic value expressions:

```sql
WHERE i = nn - 5
WHERE i BETWEEN nn - 7 AND nn - 5
WHERE i IN (nn - 5, 0)
```

The behavior of each function family remains bounded by that family's own
compatibility contract. For example, this slice admits `JSON_LENGTH()` as a
predicate value only for the JSON inputs already supported by MyLite.

## Parser Strategy

The primary grammar still contains intentionally narrow predicate value
productions. When initial parsing fails for a `SELECT`, `UPDATE`, or `DELETE`
statement, the placeholder retry scanner may replace a supported row-scalar
predicate value span with an integer placeholder, parse the statement, parse the
original value span as a `DO <expr>` expression, and splice the cloned expression
AST back into the predicate.

The retry scanner is limited to tokens in a `WHERE` predicate context. It does
not run for `SHOW` filters, `INFORMATION_SCHEMA` filters, `ON`, `HAVING`, or
source-free scalar contexts.

## MyLite Lemon-Syntax Snippet

The implemented retry path is equivalent to extending predicate value operands
for the current supported contexts as follows:

```lemon
predicate_comparison_value ::= supported_row_scalar_value_expression.
predicate_range_value ::= supported_row_scalar_value_expression.
predicate_in_value ::= supported_row_scalar_value_expression.

supported_row_scalar_value_expression ::=
    supported_row_scalar_expression_with_row_operand.
```

This is intentionally not a promise that all MySQL expressions are accepted.
Unsupported expression roots continue to fail at parse time or return the
existing targeted runtime diagnostic.

## Deferred Work

- Joined `ON` predicates and grouped `HAVING` predicate values.
- Source-free scalar expression values in table-backed predicates outside the
  current literal/session scalar subset.
- Arbitrary expression composition across every function family.
- Full expression metadata for generated predicate value expressions.
- JSON, temporal, binary, collation, and exact-decimal behavior outside each
  existing function family's documented row-scalar subset.
