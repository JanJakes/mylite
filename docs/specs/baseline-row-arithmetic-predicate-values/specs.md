# Baseline Row Arithmetic Predicate Values

## Summary

This slice extends the documented single-table row-scalar predicate envelope so
row arithmetic expressions may appear as predicate value operands:

- comparison right operands, such as `WHERE i = nn - 5`;
- row-scalar comparison right operands, such as `WHERE i + nn = nn + i`;
- `BETWEEN` and `NOT BETWEEN` lower and upper bounds, such as
  `WHERE i BETWEEN nn - 7 AND nn - 5`;
- `IN` and `NOT IN` literal-list entries, such as
  `WHERE i IN (nn - 5, 0)`.

It builds on the existing [baseline row arithmetic predicates](../baseline-row-arithmetic-predicates/specs.md),
[baseline parenthesized row arithmetic predicates](../baseline-parenthesized-row-arithmetic-predicates/specs.md),
and [baseline nested row arithmetic predicate parentheses](../baseline-nested-row-arithmetic-predicate-parentheses/specs.md)
slices. The goal is to remove a concrete "direct function only" value-operand
limit without introducing a general expression VM.

## References

- `https://dev.mysql.com/doc/refman/8.4/en/expressions.html`
- `https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html`
- `https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html`

The MySQL 8.4 manual describes predicates as expression forms where `IN` list
items and `BETWEEN` operands are expressions, and describes the arithmetic
operators available to those expressions. This slice verifies the exact baseline
row results against MySQL 8.4.9 before implementation.

## MySQL 8.4.9 Runtime Observations

Fixture:

```sql
CREATE TABLE numbers (
  id INT NOT NULL,
  i INT,
  iu INT UNSIGNED,
  b BIGINT,
  bu BIGINT UNSIGNED,
  n INT NULL,
  nn INT NOT NULL,
  tie INT NULL
);
INSERT INTO numbers VALUES
  (1, -2, 0, -9223372036854775808, 0, NULL, 5, 1),
  (2, 1, 2, 3, 4, 9, 6, 1),
  (3, 2147483647, 4294967295, 9223372036854775807, 9223372036854775807, NULL, 7, 2),
  (4, 0, 8, 8, 8, 9, 8, 2);
```

Observed on MySQL 8.4.9:

```sql
SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i = nn - 5;
-- 2

SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i = ABS(nn - 7);
-- 2

SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i + nn = nn + i;
-- 1,2,3,4

SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i < (nn + tie) * 2;
-- 1,2,4

SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i BETWEEN nn - 7 AND nn - 5;
-- 1,2

SELECT GROUP_CONCAT(id ORDER BY id)
FROM numbers
WHERE i + nn BETWEEN nn - 2 AND nn + 2;
-- 1,2,4

SELECT GROUP_CONCAT(id ORDER BY id)
FROM numbers
WHERE i NOT BETWEEN nn - 7 AND nn - 5;
-- 3,4

SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i IN (nn - 5, 0);
-- 2,4

SELECT GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i NOT IN (nn - 5, 0);
-- 1,3

SELECT GROUP_CONCAT(id ORDER BY id)
FROM numbers
WHERE i + nn IN (nn - 2, nn + 1, NULL);
-- 1,2
```

All observed statements return zero warnings.

## Supported Scope

MyLite supports row arithmetic predicate value expressions in the existing
single-table descriptor-backed `SELECT` predicate envelope and the supported
query shapes that reuse that predicate planner. The admitted arithmetic
operators and operands are the current row arithmetic subset: unary `+` and
unary `-` over descriptor columns, binary `+`, binary `-`, `*`, `/`, `DIV`,
`%`, infix `MOD`, `MOD(left, right)`, numeric/string/boolean and `NULL`
literals, descriptor columns already accepted by row arithmetic, and the
documented covered row numeric and string-length function operands.

This slice admits arithmetic values in:

- comparison RHS operands for descriptor-column or row-scalar subjects;
- `BETWEEN` lower and upper operands for descriptor-column or row-scalar
  subjects;
- `IN` list entries for row-scalar subjects and descriptor-column subjects.

## Grammar

The intended MyLite Lemon grammar additions are narrow aliases into the current
row arithmetic nonterminal:

```lemon
predicate_comparison_value(A) ::= row_scalar_arithmetic_predicate_expression(V). {
    A = V;
}

predicate_range_value(A) ::= row_scalar_arithmetic_predicate_expression(V). {
    A = V;
}
```

`predicate_in_value` already admits `predicate_row_scalar_expression`, which
already includes `row_scalar_arithmetic_predicate_expression`.

## Runtime Design

No new evaluator is needed. The existing predicate plan nodes already have
slots for:

- `row_scalar_value_expression`;
- `row_scalar_upper_value_expression`;
- row-scalar value-list entries.

The planner should classify row arithmetic expression values with the same
`predicate_node_is_supported_row_scalar_expression()` helper used by direct
function values. SQL generation continues to render both subject and value
operands through MyLite's row-scalar SQL builder, and binding continues through
existing row-scalar parameter binding.

SQLite receives a normal scalar SQL expression calling MyLite-owned UDFs where
needed. This is a MyLite wrapper/translation change using public SQLite
extension APIs; no targeted SQLite fork hook is needed.

## Diagnostics

Unsupported operands continue to use the existing row-scalar predicate
diagnostics. Malformed arithmetic remains a syntax error. Unsupported types
inside an admitted arithmetic value, unsupported joined/grouped contexts, and
unsupported DML expression contexts retain current errors.

## Metadata And Storage

No catalog, metadata, or file-format changes are required. Result metadata is
unchanged because these expressions are predicates only.

## Performance

The physical query remains a SQLite statement over the base table. Arithmetic
value operands are evaluated per candidate row by SQLite and MyLite UDFs, the
same path already used for row-scalar predicate subjects and direct function
value operands. This avoids materializing full tables in MyLite.

## Tests

Add MySQL-verified expectation coverage and MyLite runtime coverage for:

- descriptor-column comparison RHS arithmetic;
- documented numeric function calls inside comparison RHS arithmetic;
- row-scalar comparison RHS arithmetic;
- nested arithmetic RHS values;
- arithmetic `BETWEEN` lower/upper bounds;
- `NOT BETWEEN` with arithmetic bounds;
- arithmetic entries in descriptor-column and row-scalar `IN` lists.
- `NOT IN` with arithmetic entries.

## Deferred

- General `WHERE expression` parsing and execution.
- Arbitrary bitwise, temporal interval, JSON-arrow, tuple, aggregate, window,
  subquery, and assignment-expression operands.
- Joined `ON`, grouped `HAVING`, broad DML assignment, expression index, and
  generated-column expression surfaces beyond paths already documented.
- Binary-string, JSON, temporal interval, unsigned-width, fixed-decimal, and
  broad expression metadata parity.
