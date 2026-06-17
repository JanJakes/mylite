# Baseline Row Arithmetic Predicates

## Goal

MyLite already lowers supported single-table row-scalar arithmetic expressions to
SQLite SQL through MyLite-owned UDFs. This slice admits that same arithmetic
envelope in table-backed and source-free predicate contexts that already have
row-scalar predicate planners:

- bare truth predicates, such as `WHERE a + b`;
- comparison predicates, such as `WHERE a + b * 2 >= 5`;
- `IS [NOT] NULL`;
- `IS [NOT] TRUE`, `IS [NOT] FALSE`, and `IS [NOT] UNKNOWN`;
- `[NOT] BETWEEN`;
- `[NOT] IN` literal lists.

This advances the documented predicate part of the general expression IR slice
without installing a broad `WHERE expression` grammar catch-all.

## Compatibility Authority

Normative behavior comes from official MySQL 8.4 documentation and MySQL 8.4.9
runtime observations:

- `https://dev.mysql.com/doc/refman/8.4/en/select.html`
- `https://dev.mysql.com/doc/refman/8.4/en/expressions.html`
- `https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html`
- `https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html`

The MySQL manual defines `WHERE` as an expression context, includes arithmetic
operators in expression syntax, and documents `BETWEEN`, `IN`, `IS NULL`, and
`IS boolean_value` as comparison operators.

Observed MySQL 8.4.9 probe:

```sql
CREATE TABLE t(id INT PRIMARY KEY, a INT, b INT, s VARCHAR(20), n INT NULL);
INSERT INTO t VALUES
  (1, 1, 2, '8x', NULL),
  (2, -1, 0, 'x8', 3),
  (3, NULL, 5, NULL, NULL),
  (4, 0, 0, '0', 0);

SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE a + b * 2 >= 5;
SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE a + b;
SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE a + b IS TRUE;
SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE a + b IS NOT TRUE;
SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE a + b IS FALSE;
SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE a + b IS NOT FALSE;
SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE a + b IS UNKNOWN;
SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE a + b IS NOT UNKNOWN;
SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE a / b IS NULL;
SHOW WARNINGS;
SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE a + b BETWEEN 2 AND 4;
SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE a + b NOT BETWEEN 2 AND 4;
SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE a + b IN (0, 3, NULL);
SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE a + b NOT IN (0, 3, NULL);
SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE MOD(a + 5, b + 2) = 0;
SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE s + 1 > 1;
SHOW WARNINGS;
```

Key observations:

- `a + b * 2 >= 5` returns row `1`.
- Bare arithmetic truth and `IS TRUE` return rows `1,2`; `IS FALSE` returns
  row `4`; `IS UNKNOWN` returns row `3`.
- `IS NOT TRUE` returns rows `3,4`; `IS NOT FALSE` returns rows `1,2,3`; and
  `IS NOT UNKNOWN` returns rows `1,2,4`.
- `a / b IS NULL` returns rows `2,3,4` and emits division-by-zero warning
  `1365`.
- `a + b BETWEEN 2 AND 4` returns row `1`; `NOT BETWEEN` returns rows `2,4`.
- `a + b IN (0, 3, NULL)` returns rows `1,4`; `NOT IN` returns no rows because
  unmatched values see the `NULL` list member.
- `MOD(a + 5, b + 2) = 0` returns row `2`.
- String arithmetic follows MySQL numeric-prefix coercion and emits warning
  `1292` for `'8x'`.

## Syntax

MyLite keeps `WHERE` on the existing predicate grammar. The new executable
surface is a narrow arithmetic operand grammar that feeds the existing
row-scalar predicate productions:

```lemon
predicate_row_scalar_expression ::= row_scalar_arithmetic_predicate_expression.

row_scalar_arithmetic_predicate_expression ::= row_scalar_arithmetic_additive.
row_scalar_arithmetic_additive ::= row_scalar_arithmetic_expression PLUS row_scalar_arithmetic_expression.
row_scalar_arithmetic_additive ::= row_scalar_arithmetic_expression MINUS row_scalar_arithmetic_expression.
row_scalar_arithmetic_additive ::= row_scalar_arithmetic_multiplicative.
row_scalar_arithmetic_multiplicative ::= row_scalar_arithmetic_expression STAR row_scalar_arithmetic_expression.
row_scalar_arithmetic_multiplicative ::= row_scalar_arithmetic_expression SLASH row_scalar_arithmetic_expression.
row_scalar_arithmetic_multiplicative ::= row_scalar_arithmetic_expression DIV row_scalar_arithmetic_expression.
row_scalar_arithmetic_multiplicative ::= row_scalar_arithmetic_expression PERCENT row_scalar_arithmetic_expression.
row_scalar_arithmetic_multiplicative ::= row_scalar_arithmetic_expression MOD row_scalar_arithmetic_expression.
row_scalar_arithmetic_multiplicative ::= row_scalar_arithmetic_function.
row_scalar_arithmetic_expression ::= row_scalar_arithmetic_expression PLUS row_scalar_arithmetic_expression.
row_scalar_arithmetic_expression ::= row_scalar_arithmetic_expression MINUS row_scalar_arithmetic_expression.
row_scalar_arithmetic_expression ::= row_scalar_arithmetic_expression STAR row_scalar_arithmetic_expression.
row_scalar_arithmetic_expression ::= row_scalar_arithmetic_expression SLASH row_scalar_arithmetic_expression.
row_scalar_arithmetic_expression ::= row_scalar_arithmetic_expression DIV row_scalar_arithmetic_expression.
row_scalar_arithmetic_expression ::= row_scalar_arithmetic_expression PERCENT row_scalar_arithmetic_expression.
row_scalar_arithmetic_expression ::= row_scalar_arithmetic_expression MOD row_scalar_arithmetic_expression.
row_scalar_arithmetic_expression ::= row_scalar_arithmetic_primary.
row_scalar_arithmetic_expression ::= row_scalar_arithmetic_function.
row_scalar_arithmetic_function ::= MOD LPAREN expression COMMA expression RPAREN.
row_scalar_arithmetic_primary ::= qualified_identifier.
row_scalar_arithmetic_primary ::= literal.
row_scalar_arithmetic_primary ::= PLUS numeric_literal.
row_scalar_arithmetic_primary ::= MINUS numeric_literal.
row_scalar_arithmetic_primary ::= row_scalar_numeric_predicate_expression.
row_scalar_arithmetic_primary ::= string_length_expression.
```

The grammar is intentionally not `predicate_atom ::= expression`. Unsupported
general expression surfaces must continue to parse either through existing
documented grammar or through the explicit unsupported-placeholder classifier.
The executable arithmetic predicate root must contain an arithmetic operator or
be a `MOD(left, right)` function call; a lone descriptor column remains on the
existing descriptor predicate paths. Parenthesized top-level arithmetic
predicate roots such as `(a + b) IS TRUE` are covered by the follow-up
[baseline parenthesized row arithmetic predicates](../baseline-parenthesized-row-arithmetic-predicates/specs.md)
slice. Nested arithmetic parentheses inside larger predicate arithmetic, such
as `((a + b) * 2) > 0`, remain deferred.

## Runtime Semantics

Planning reuses the existing row-scalar arithmetic planner. Supported operands
therefore stay aligned with the current arithmetic projection and ordering
slice:

- numeric and nonbinary string descriptor columns in one-table query sources;
- numeric, string, boolean, and `NULL` literals;
- nested supported arithmetic operators;
- covered numeric functions, string length functions, and numeric temporal
  extractors where the row-scalar arithmetic planner can lower them.

SQLite still performs scanning, filtering, sorting, and limiting. MyLite emits
SQLite SQL calling MyLite-owned arithmetic UDFs so MySQL coercion, division by
zero, and warning behavior remain under MyLite control. No targeted SQLite fork
hook is needed.

`expr IS boolean_value` over row-scalar arithmetic is lowered without repeating
the expression:

- `IS TRUE` -> `COALESCE(expr <> 0, 0)`;
- `IS NOT TRUE` -> `COALESCE(expr = 0, 1)`;
- `IS FALSE` -> `COALESCE(expr = 0, 0)`;
- `IS NOT FALSE` -> `COALESCE(expr <> 0, 1)`;
- `IS UNKNOWN` -> `expr IS NULL`;
- `IS NOT UNKNOWN` -> `expr IS NOT NULL`.

## Diagnostics

Warnings from arithmetic UDFs belong to the current statement diagnostics area
and must remain visible through result warning counts and `SHOW WARNINGS`.
Unsupported operands continue to fail with targeted MyLite diagnostics instead
of silently falling back to SQLite semantics.

## Metadata

This slice changes predicate admission only. Projection labels and expression
metadata remain governed by the existing row-scalar expression slices.

## Tests

Add or update MySQL-verified expectation tests and C runtime tests for:

- table-backed arithmetic comparison predicates;
- bare arithmetic truth predicates;
- all six `IS [NOT] TRUE/FALSE/UNKNOWN` variants;
- arithmetic `IS [NOT] NULL`;
- arithmetic `[NOT] BETWEEN`;
- arithmetic `[NOT] IN`;
- nested `MOD()` and operator precedence;
- string numeric coercion warnings;
- parser-corpus representative `SELECT COUNT(*) FROM t1 WHERE a + 1 > 1`;
- tableless arithmetic filters where the row-scalar filter path already applies.

## Deferred Work

- General `WHERE expression` parsing and execution.
- Nested arithmetic parentheses inside larger row arithmetic predicate roots.
- Arithmetic predicates in joined `ON`, grouped `HAVING`, broad DML
  assignments, and aggregate/window expression surfaces not already documented.
- Binary-string, JSON, temporal interval, unsigned-width, and full fixed-decimal
  parity.
- Full expression metadata and optimizer/index parity for expression
  predicates.
