# Baseline Parenthesized Row Arithmetic Predicates

## Goal

This slice extends the baseline row arithmetic predicate subset to accept a
single redundant parenthesized wrapper around the arithmetic predicate subject:

- comparison predicates, such as `WHERE (a + b * 2) >= 5`;
- `IS [NOT] NULL`;
- `IS [NOT] TRUE`, `IS [NOT] FALSE`, and `IS [NOT] UNKNOWN`;
- `[NOT] BETWEEN`;
- `[NOT] IN` literal lists;
- `MOD(left, right)` wrapped as `(MOD(left, right))`.

The runtime semantics are the same as the unparenthesized
[baseline row arithmetic predicates](../baseline-row-arithmetic-predicates/specs.md)
slice. The goal is syntax admission for common MySQL expression grouping, not a
general `WHERE expression` grammar.

## Compatibility Authority

Normative behavior comes from official MySQL 8.4 documentation and MySQL 8.4.9
runtime observations:

- `https://dev.mysql.com/doc/refman/8.4/en/expressions.html`
- `https://dev.mysql.com/doc/refman/8.4/en/select.html`
- `https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html`
- `https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html`

The MySQL expression grammar admits parenthesized expressions as expression
terms and admits arithmetic operators, `IS`, comparison, `BETWEEN`, and `IN`
in predicate contexts.

Observed MySQL 8.4.9 probe:

```sql
CREATE TABLE t(id INT PRIMARY KEY, a INT, b INT, s VARCHAR(20), n INT NULL);
INSERT INTO t VALUES
  (1, 1, 2, '8x', NULL),
  (2, -1, 0, 'x8', 3),
  (3, NULL, 5, NULL, NULL),
  (4, 0, 0, '0', 0);

SELECT 'cmp', GROUP_CONCAT(id ORDER BY id) FROM t WHERE (a + b * 2) >= 5;
SELECT 'truth', GROUP_CONCAT(id ORDER BY id) FROM t WHERE (a + b);
SELECT 'is_true', GROUP_CONCAT(id ORDER BY id) FROM t WHERE (a + b) IS TRUE;
SELECT 'is_not_true', GROUP_CONCAT(id ORDER BY id) FROM t WHERE (a + b) IS NOT TRUE;
SELECT 'is_false', GROUP_CONCAT(id ORDER BY id) FROM t WHERE (a + b) IS FALSE;
SELECT 'is_not_false', GROUP_CONCAT(id ORDER BY id) FROM t WHERE (a + b) IS NOT FALSE;
SELECT 'is_unknown', GROUP_CONCAT(id ORDER BY id) FROM t WHERE (a + b) IS UNKNOWN;
SELECT 'is_not_unknown', GROUP_CONCAT(id ORDER BY id) FROM t WHERE (a + b) IS NOT UNKNOWN;
SELECT 'between', GROUP_CONCAT(id ORDER BY id) FROM t WHERE (a + b) BETWEEN 2 AND 4;
SELECT 'not_between', GROUP_CONCAT(id ORDER BY id) FROM t WHERE (a + b) NOT BETWEEN 2 AND 4;
SELECT 'in', GROUP_CONCAT(id ORDER BY id) FROM t WHERE (a + b) IN (0, 3, NULL);
SELECT 'not_in', GROUP_CONCAT(id ORDER BY id) FROM t WHERE (a + b) NOT IN (0, 3, NULL);
SELECT 'mod', GROUP_CONCAT(id ORDER BY id) FROM t WHERE (MOD(a + 5, b + 2)) = 0;
SELECT 'grouped', GROUP_CONCAT(id ORDER BY id)
  FROM t WHERE ((a + b) >= 0 AND id > 1);
SELECT 'tableless', 'visible' WHERE (1 + 2 * 3) = 7;
```

Key observations:

- Parenthesized comparison returns row `1`.
- Parenthesized bare truth and `IS TRUE` return rows `1,2`; `IS FALSE`
  returns row `4`; `IS UNKNOWN` returns row `3`.
- `IS NOT TRUE` returns rows `3,4`; `IS NOT FALSE` returns rows `1,2,3`;
  `IS NOT UNKNOWN` returns rows `1,2,4`.
- Parenthesized `BETWEEN` returns row `1`; parenthesized `NOT BETWEEN`
  returns rows `2,4`.
- Parenthesized `IN (0, 3, NULL)` returns rows `1,4`; parenthesized
  `NOT IN (0, 3, NULL)` returns no rows because unmatched values see the
  `NULL` list member.
- Wrapped `MOD(a + 5, b + 2) = 0` returns row `2`.
- Parenthesized row arithmetic can appear inside a grouped predicate expression.
- A source-free parenthesized arithmetic comparison can filter a tableless
  `SELECT`.

## Syntax

MyLite does not add a broad parenthesized arithmetic branch to the Lemon
predicate grammar because that overlaps with the existing `LPAREN predicate
RPAREN` boolean grouping production and creates an impractical parser state
space. Instead, the normal parser runs first. On a syntax failure, a targeted
retry removes only a redundant wrapper around an arithmetic predicate subject
when all of the following are true:

- the left parenthesis starts a predicate subject after `WHERE`, `ON`,
  `HAVING`, a predicate-clause boolean connective (`AND`, `&&`, `OR`, `||`,
  `XOR`, or `NOT`), or nested predicate grouping parentheses that trace back
  to one of those clause starts or connectives;
- the matching right parenthesis is followed by a comparison operator, `IS`,
  `BETWEEN`, `NOT BETWEEN`, `IN`, or `NOT IN`;
- the wrapped expression has a top-level row arithmetic operator or a top-level
  `MOD(...)` call;
- the statement has balanced parentheses and no non-trailing semicolon.

The retry then feeds the original token stream to Lemon while omitting only the
selected wrapper tokens. The resulting AST is the same AST the already
implemented unparenthesized form would produce.

Equivalent Lemon-shape intent:

```lemon
predicate_atom ::= parenthesized_row_arithmetic_subject comparison_operator predicate_value.
predicate_atom ::= parenthesized_row_arithmetic_subject IS [NOT] NULL.
predicate_atom ::= parenthesized_row_arithmetic_subject IS [NOT] TRUE.
predicate_atom ::= parenthesized_row_arithmetic_subject IS [NOT] FALSE.
predicate_atom ::= parenthesized_row_arithmetic_subject IS [NOT] UNKNOWN.
predicate_atom ::= parenthesized_row_arithmetic_subject [NOT] BETWEEN lower AND upper.
predicate_atom ::= parenthesized_row_arithmetic_subject [NOT] IN LPAREN value_list RPAREN.
parenthesized_row_arithmetic_subject ::= LPAREN row_arithmetic_predicate_expression RPAREN.
```

The executable implementation uses parser retry rather than these direct
productions to keep the current Lemon grammar tractable.

## Runtime Semantics

Runtime planning is unchanged from the unparenthesized row arithmetic predicate
slice. The retry removes only redundant grouping tokens before AST construction,
so all supported predicates flow through the existing row-scalar arithmetic SQL
lowering, MyLite-owned UDFs, warning accounting, and SQLite execution path.

Supported operands, coercions, warning behavior, and diagnostics remain exactly
those documented for the baseline row arithmetic predicate subset. No targeted
SQLite fork hook is needed.

## Diagnostics

Malformed statements remain syntax errors. Parenthesized expressions outside
this narrow predicate-subject shape continue through the normal parser or the
existing unsupported-placeholder classifiers. Unsupported operands still report
the current targeted MyLite diagnostics after parsing.

## Metadata

This slice changes predicate syntax admission only. It does not alter result
column labels, expression metadata, table metadata, or protocol metadata.

## Tests

Add or update MySQL-verified expectation tests and C runtime/parser tests for:

- parenthesized arithmetic comparison predicates;
- parenthesized arithmetic predicates inside boolean grouping;
- parenthesized `[NOT] BETWEEN` and `[NOT] IN` subjects;
- parenthesized `IS TRUE` and `IS FALSE` subjects;
- parenthesized `MOD(...)` comparison subjects;
- source-free tableless parenthesized arithmetic filters;
- parser-corpus representative parenthesized arithmetic predicates, including
  a subquery predicate.

## Deferred Work

- Nested arithmetic parentheses inside a larger arithmetic expression, such as
  `((a + b) * 2) > 0`.
- General `WHERE expression` parsing and execution.
- Joined `ON`, grouped `HAVING`, and DML contexts beyond forms that already
  lower to the same documented predicate subset.
- Binary-string, JSON, temporal interval, unsigned-width, fixed-decimal, and
  broad expression metadata parity.
