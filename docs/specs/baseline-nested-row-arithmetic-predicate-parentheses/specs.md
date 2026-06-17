# Baseline Nested Row Arithmetic Predicate Parentheses

## Goal

This slice extends the supported row arithmetic predicate subset to admit
semantic parentheses inside a larger arithmetic predicate expression:

- comparison predicates, such as `WHERE ((a + b) * 2) > 10`;
- `[NOT] BETWEEN`;
- `[NOT] IN` literal lists;
- `IS [NOT] TRUE`, `IS [NOT] FALSE`, and `IS [NOT] UNKNOWN`;
- arithmetic function arguments and larger arithmetic wrappers, such as
  `WHERE (MOD((a + 2), b) + 1) = 1`;
- source-free tableless filters that already lower through the row arithmetic
  predicate path.

The runtime semantics stay aligned with the existing
[baseline row arithmetic predicates](../baseline-row-arithmetic-predicates/specs.md)
and
[baseline parenthesized row arithmetic predicates](../baseline-parenthesized-row-arithmetic-predicates/specs.md)
slices. The goal is nested arithmetic grouping in documented predicate
contexts, not broad `WHERE expression` support.

## Compatibility Authority

Normative behavior comes from official MySQL 8.4 documentation and MySQL 8.4.9
runtime observations:

- `https://dev.mysql.com/doc/refman/8.4/en/expressions.html`
- `https://dev.mysql.com/doc/refman/8.4/en/select.html`
- `https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html`
- `https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html`

The MySQL manual describes `WHERE` as an expression context, arithmetic
operators as expression terms, parenthesized expressions as simple expressions,
and `BETWEEN`, `IN`, `IS`, and comparison operators as predicate surfaces.

Observed MySQL 8.4.9 probe:

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
  (3, 2147483647, 4294967295, 9223372036854775807,
   9223372036854775807, NULL, 7, 2),
  (4, 0, 8, 8, 8, 9, 8, 2);

SELECT 'cmp', GROUP_CONCAT(id ORDER BY id)
  FROM numbers WHERE ((i + nn) * 2) > 10;
SELECT 'eq', GROUP_CONCAT(id ORDER BY id)
  FROM numbers WHERE ((i + nn) * 2) = 16;
SELECT 'between', GROUP_CONCAT(id ORDER BY id)
  FROM numbers WHERE ((i + nn) * 2) BETWEEN 6 AND 16;
SELECT 'in', GROUP_CONCAT(id ORDER BY id)
  FROM numbers WHERE ((i + nn) * 2) IN (6, 16, NULL);
SELECT 'is_true', GROUP_CONCAT(id ORDER BY id)
  FROM numbers WHERE ((i + nn) * 2) IS TRUE;
SELECT 'is_false', GROUP_CONCAT(id ORDER BY id)
  FROM numbers WHERE ((i + nn) * 0) IS FALSE;
SELECT 'mod', GROUP_CONCAT(id ORDER BY id)
  FROM numbers WHERE (MOD((i + 2), nn) + 1) = 1;
SELECT 'tableless', 'visible' WHERE ((1 + 2) * 3) = 9;
```

Key observations:

- The comparison predicate returns rows `2,3,4`.
- The equality predicate returns row `4`.
- The `BETWEEN` predicate returns rows `1,2,4`.
- The `IN (6, 16, NULL)` predicate returns rows `1,4`.
- The `IS TRUE` and `IS FALSE` probes return all four rows for the selected
  nonzero and zero expressions respectively.
- The nested `MOD()` probe returns row `1`.
- A source-free nested arithmetic comparison can filter a tableless `SELECT`.

## Syntax

MyLite keeps the core predicate Lemon grammar narrow. A direct parenthesized
arithmetic-primary production overlaps with boolean predicate grouping, so this
slice is implemented as a targeted syntax retry after a normal parse failure.
The retry recognizes balanced row arithmetic predicate subjects, substitutes
synthetic identifiers so the surrounding predicate parses through existing
productions, parses each original subject through the existing expression
grammar, and splices those expression AST nodes back into the predicate.

Equivalent Lemon-shape intent:

```lemon
row_scalar_arithmetic_primary ::= LPAREN row_scalar_arithmetic_expression RPAREN.
```

The executable implementation is intentionally narrower than a general
predicate term. It relies on the existing predicate envelopes for comparison,
`BETWEEN`, `IN`, `IS`, and tableless filtering. Unsupported general expression
surfaces continue to parse through their current supported grammar or
placeholder classifiers.

## Runtime Semantics

Runtime planning is unchanged. The parser retry produces a parenthesized
expression AST node inside the row arithmetic predicate tree, and the existing
row-scalar arithmetic lowering removes redundant grouping while preserving
operator precedence and MyLite-owned arithmetic UDF calls.

Supported operands, coercions, warning behavior, diagnostics, and SQLite
execution remain those documented for the row arithmetic predicate subset. No
targeted SQLite fork hook is needed.

## Diagnostics

Malformed grouped arithmetic remains a syntax error. Parenthesized expressions
outside the documented row arithmetic predicate subset continue to use existing
diagnostics or unsupported-placeholder classification.

## Metadata

This slice changes predicate syntax admission only. It does not change result
column metadata, expression labels, table metadata, or protocol metadata.

## Tests

Add MySQL-runtime expectation tests and MyLite parser/runtime tests for:

- nested row arithmetic comparison and equality predicates;
- multiple nested row arithmetic predicates in one boolean expression;
- nested row arithmetic `BETWEEN` and `IN` predicates;
- nested row arithmetic `IS TRUE` and `IS FALSE` predicates;
- nested parentheses inside `MOD()` arguments and larger arithmetic wrappers;
- source-free nested tableless filters;
- parser-corpus representatives for table-backed and subquery predicate
  contexts.

## Deferred Work

- General `WHERE expression` parsing and execution.
- Arbitrary expression operands outside the documented row-scalar arithmetic
  subset; arithmetic predicate value operands are covered by
  [baseline row arithmetic predicate values](../baseline-row-arithmetic-predicate-values/specs.md).
- Joined `ON`, grouped `HAVING`, broad DML assignments, aggregate/window
  expression surfaces, and expression indexes beyond paths already documented.
- Binary-string, JSON, temporal interval, unsigned-width, fixed-decimal, and
  broad expression metadata parity.
