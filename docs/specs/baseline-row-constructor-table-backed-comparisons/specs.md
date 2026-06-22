# Baseline Row Constructor Table-Backed Comparisons

This slice extends the existing row-constructor `WHERE` predicate baseline from
explicit equality predicates to MySQL-shaped table-backed tuple comparisons.
It covers both `ROW(a,b)` and parenthesized `(a,b)` tuple syntax in the current
single-source predicate envelope.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/row-subqueries.html
- https://dev.mysql.com/doc/refman/8.4/en/row-constructor-optimization.html
- https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html

Runtime probes are captured in:

- `packages/libmylite/tests/mysql_parser_corpus_query_expression_clause_surfaces_expectations.sh`

## MySQL 8.4.9 Runtime Observations

Observed against MySQL 8.4.9:

```sql
CREATE TABLE t1 (a INT, b INT, c VARCHAR(20));
INSERT INTO t1 VALUES (1,2,'x'), (3,4,'y');

SELECT COUNT(*) FROM t1 WHERE (a,b) = (1,2);
-- 1

SELECT COUNT(*) FROM t1 WHERE ROW(a,b) > ROW(1,2);
-- 1

SELECT COUNT(*) FROM t1 WHERE ROW(a,b) >= ROW(1,2);
-- 2

SELECT COUNT(*) FROM t1 WHERE ROW(a,b) < ROW(3,4);
-- 1

SELECT COUNT(*) FROM t1 WHERE ROW(a,b) <= ROW(3,4);
-- 2
```

With `NULL` tuple elements, ordinary equality and ordering can evaluate to
unknown and therefore filter rows in `WHERE`, while `<=>` compares tuple
elements with null-safe equality.

## Scope

In scope:

- `WHERE ROW(value[, ...]) comparison_operator ROW(value[, ...])` and
  `WHERE (value[, ...]) comparison_operator (value[, ...])` in existing
  single-source `SELECT`, `UPDATE`, and `DELETE` predicate envelopes;
- operators `=`, `<=>`, `<>`, `!=`, `<`, `<=`, `>`, and `>=`;
- tuple elements where each corresponding element pair is one descriptor
  column and one supported literal predicate value, on either side;
- MySQL-shaped `1241 / 21000` arity diagnostics for mismatched tuple sizes;
- runtime lowering to existing descriptor comparison predicate nodes, so
  SQLite receives ordinary column/value predicates.

Out of scope:

- tuple `IN` / `NOT IN`;
- row subqueries, `ALL`, `ANY`, or `SOME`;
- tuple comparisons where both corresponding elements are descriptor columns
  or general expressions;
- tuple comparisons in `ON`, `HAVING`, projection, grouping, ordering, or
  nested general expression contexts outside the current scalar/tableless
  row-constructor evaluator;
- tuple index-planning metadata or optimizer-specific rewrites beyond the
  scalar predicate tree MyLite emits.

## MyLite Grammar Snippet

These snippets describe the intended MyLite-owned grammar shape for this
slice. They are not copied from MySQL grammar sources.

```lemon
predicate_atom ::= predicate_row_value comparison_operator predicate_row_value.
predicate_row_value ::= ROW LPAREN expression COMMA expression_list RPAREN.
predicate_row_value ::= LPAREN expression COMMA expression_list RPAREN.
comparison_operator ::= EQ | NULL_SAFE_EQ | NE | LT | LE | GT | GE.
```

The normal parser already supports the explicit `ROW(...)` node shape after
placeholder retry. This slice extends retry admission to parenthesized tuple
operands in `WHERE` and all supported comparison operators.

## Runtime Behavior

No SQLite fork hook is needed. MyLite keeps compatibility logic in the MyLite
runtime and lowers supported tuples to existing predicate nodes:

- equality: `a = 1 AND b = 2`;
- null-safe equality: `a <=> 1 AND b <=> 2`;
- inequality: `NOT (a = 1 AND b = 2)`;
- strict ordering: `a > 1 OR (a = 1 AND b > 2)`, with the operator direction
  flipped when the descriptor column appears on the right side;
- inclusive ordering: strict ordering plus tuple equality.

The resulting tree preserves SQL three-valued logic for `NULL` operands through
SQLite's normal `AND` / `OR` / `NOT` evaluation and MyLite's existing
descriptor value conversion.

## Tests

Tests cover:

- MySQL 8.4.9 expectations for parenthesized tuple equality, null-safe
  equality, inequality, strict/inclusive order, literal-left order, and `NULL`
  tuple behavior;
- parser admission for parenthesized tuple predicates and order operators;
- runtime row-count coverage for table-backed `SELECT` plus `UPDATE` and
  `DELETE` reuse through the shared predicate planner;
- preserved arity diagnostics and unsupported diagnostics for tuple contexts
  that remain out of scope.

## Compatibility Status

This slice narrows the row-constructor and `WHERE` gaps for table-backed tuple
comparison predicates. Tuple membership, row subqueries, column-to-column tuple
elements, and tuple comparisons in non-`WHERE` table-backed contexts remain
documented gaps.
