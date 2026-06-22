# Baseline Row Constructor IN Predicates

This slice extends the table-backed row-constructor predicate baseline from
tuple comparisons to tuple membership. It covers MySQL-shaped `IN` and
`NOT IN` predicates for explicit `ROW(...)` constructors and parenthesized
row tuples in the existing single-source predicate planner.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/row-subqueries.html
- https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html

Runtime probes are captured in:

- `packages/libmylite/tests/mysql_parser_corpus_query_expression_clause_surfaces_expectations.sh`

## MySQL 8.4.9 Runtime Observations

Observed against MySQL 8.4.9:

```sql
CREATE TABLE t1 (a INT, b INT, c VARCHAR(20));
CREATE TABLE t_tuple (a INT, b INT, c VARCHAR(20));
INSERT INTO t1 VALUES (1,2,'x'), (3,4,'y');
INSERT INTO t_tuple VALUES (1,2,'x'), (1,NULL,'n'), (3,4,'y');

SELECT COUNT(*) FROM t1 WHERE (a,b) IN ((1,2),(9,9));
-- 1

SELECT COUNT(*) FROM t1 WHERE ROW(a,b) IN (ROW(1,2), ROW(3,4));
-- 2

SELECT COUNT(*) FROM t1 WHERE (a,b) NOT IN ((1,2),(9,9));
-- 1

SELECT COUNT(*) FROM t1 WHERE ROW(1,2) IN (ROW(a,b));
-- 1

SELECT COUNT(*) FROM t_tuple
WHERE ROW(a,b) IN (ROW(1,NULL), ROW(3,4));
-- 1

SELECT COUNT(*) FROM t_tuple
WHERE ROW(a,b) NOT IN (ROW(1,NULL), ROW(9,9));
-- 1

SELECT COUNT(*) FROM t_tuple
WHERE (a,b) NOT IN ((1,NULL),(3,4));
-- 0

SELECT COUNT(*) FROM t1 WHERE (a,b) IN ((1,2,3));
-- ERROR 1241 (21000): Operand should contain 2 column(s)
```

`IN` returns true when any tuple equality is true. It returns unknown when no
tuple equality is true and at least one tuple equality is unknown, so `WHERE`
filters that row. `NOT IN` is the boolean negation of `IN`, preserving unknown.

## Scope

In scope:

- `WHERE ROW(value[, ...]) IN (ROW(value[, ...]) [, ...])`;
- `WHERE (value[, ...]) IN ((value[, ...]) [, ...])`;
- mixed explicit `ROW(...)` and parenthesized tuple values on either side;
- `NOT IN` as `NOT (tuple IN tuple_list)`;
- existing single-source `SELECT`, `UPDATE`, and `DELETE` predicate envelopes;
- tuple elements where each compared element pair is one descriptor column and
  one supported literal predicate value, on either side;
- MySQL-shaped `1241 / 21000` arity diagnostics for mismatched tuple sizes.

Out of scope:

- row-constructor `IN (SELECT ...)` and other row subqueries;
- tuple elements where both sides are descriptor columns or general
  expressions;
- tuple `IN` in `ON`, `HAVING`, projection, grouping, ordering, or nested
  general expression contexts outside the current scalar/tableless evaluator;
- tuple index planning or optimizer rewrites beyond the scalar predicate tree
  MyLite emits.

## MyLite Grammar Snippet

These snippets describe the intended MyLite-owned grammar shape for this
slice. They are not copied from MySQL grammar sources.

```lemon
predicate_atom ::= predicate_row_value IN LPAREN predicate_row_value_list RPAREN.
predicate_atom ::= predicate_row_value NOT IN LPAREN predicate_row_value_list RPAREN.
predicate_row_value_list ::= predicate_row_value.
predicate_row_value_list ::= predicate_row_value_list COMMA predicate_row_value.
predicate_row_value ::= ROW LPAREN expression COMMA expression_list RPAREN.
predicate_row_value ::= LPAREN expression COMMA expression_list RPAREN.
```

The normal parser continues to receive placeholder identifiers for row values
and then clones the parsed row-value ASTs back into the resulting `IN`
predicate. This keeps the parser path narrow while the Lemon grammar grows
incrementally.

## Runtime Behavior

No SQLite fork hook is needed. MyLite lowers supported tuple membership into
the same descriptor predicate tree used for row-constructor comparisons:

- `ROW(a,b) IN (ROW(1,2), ROW(3,4))` becomes
  `(a = 1 AND b = 2) OR (a = 3 AND b = 4)`;
- `NOT IN` wraps that tree in logical `NOT`;
- literal-left tuple membership flips each element comparison as needed;
- `NULL` behavior comes from the existing comparison, `AND`, `OR`, and `NOT`
  nodes, preserving SQL three-valued logic for the supported subset.

The lowering validates every list member against the subject arity before
execution. Empty tuple lists are not admitted by the parser, matching MySQL's
syntax rejection for empty `IN` lists.

## Tests

Tests cover:

- MySQL 8.4.9 expectations for tuple `IN`, `NOT IN`, mixed `ROW(...)` and
  parenthesized tuple syntax, literal-left tuple membership, `NULL` tuple
  behavior, and arity diagnostics;
- parser admission for explicit and parenthesized tuple membership predicates;
- runtime row-count coverage for table-backed `SELECT` plus `UPDATE` and
  `DELETE` reuse through the shared predicate planner;
- preserved unsupported diagnostics for tuple membership in `ON` and `HAVING`.

## Compatibility Status

This slice narrows the row-constructor and `WHERE` gaps for table-backed tuple
membership predicates. Row subqueries, column-to-column tuple elements, tuple
membership in non-`WHERE` table-backed contexts, and tuple-specific optimizer
planning remain documented gaps.
