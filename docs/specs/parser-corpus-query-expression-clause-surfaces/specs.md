# Parser Corpus Query Expression Clause Surfaces

This slice reduces high-volume MySQL server-test parser failures where MySQL
accepts general expression syntax in query clauses, but MyLite's currently
implemented query executors still support only documented descriptor-backed
subsets.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- https://dev.mysql.com/doc/refman/8.4/en/select.html
- https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- https://dev.mysql.com/doc/refman/8.4/en/group-by-modifiers.html

## MySQL 8.4.9 Runtime Observations

The expectation script for this slice verifies representative MySQL behavior
against MySQL 8.4.9:

- arithmetic predicates such as `a + 1 > 1` are valid `WHERE` expressions;
- operator-bearing function and arithmetic expressions are valid `ORDER BY`,
  `GROUP BY`, and `HAVING` operands;
- row tuple comparisons such as `(a,b) = (1,2)` are valid predicates;
- subqueries may contain their own expression predicates;
- multi-table `UPDATE` and ordered `DELETE` statements may contain expression
  assignments and predicates.

Observed probe:

```sql
CREATE TABLE t1 (a INT, b INT, c VARCHAR(20));
CREATE TABLE t2 (a INT, b INT);
INSERT INTO t1 VALUES (1,2,'x'),(3,4,'y');
INSERT INTO t2 VALUES (1,20),(3,40);
SELECT COUNT(*) FROM t1 WHERE a + 1 > 1;
SELECT a FROM t1 ORDER BY ABS(b - 5);
SELECT a, COUNT(*) FROM t1
  GROUP BY a + 0
  HAVING COUNT(*) >= 1 AND a > 0
  ORDER BY a + 0;
SELECT a FROM t1 WHERE (a,b) = (1,2);
SELECT a FROM t1
  WHERE a IN (SELECT a FROM t2 WHERE b + 1 > 20)
  ORDER BY a;
UPDATE t1, t2 SET t1.b = t1.b + 1 WHERE t1.a = t2.a;
DELETE FROM t1 WHERE a = a + sleep(0) ORDER BY a LIMIT 1;
```

MySQL returns the result rows recorded in
`mysql_parser_corpus_query_expression_clause_surfaces_expectations.sh`.

## Scope

In scope:

- parser fallback classification for query statements that fail normal parsing
  but contain complete, recognized general-expression surfaces in `WHERE`,
  `ON`, `GROUP BY`, `HAVING`, `ORDER BY`, DML assignment, or subquery
  expression positions;
- arithmetic, bitwise, and logical expression operators in query clauses;
- expression operators inside function arguments and aggregate arguments;
- row tuple comparison and tuple `IN` surfaces;
- bare truth expressions in table-backed predicate clauses;
- deterministic unsupported diagnostics instead of generic syntax errors for
  recognized expression-clause surfaces;
- parser corpus movement measurement over
  `build/perf-data/mysql-server-tests-queries.csv`.

Out of scope:

- executable arbitrary expression planning in table-backed `WHERE`, `ON`,
  `GROUP BY`, `HAVING`, or `ORDER BY`;
- row tuple comparison execution beyond the existing documented scalar
  row-constructor subset;
- pure function-call query-clause admission without an expression operator or
  another recognized expression-clause signal;
- broad multi-table DML execution beyond current MyLite support;
- aggregate/window expression semantics not already implemented;
- MySQL optimizer rewrites or index planning for expression predicates;
- replacing the Lemon expression grammar with an ambiguous catch-all rule.

## MyLite Grammar Snippet

These snippets describe the intended MyLite-owned grammar shape and do not copy
MySQL grammar.

```lemon
query_expression_operand ::= expression.

where_clause ::= WHERE expression.
join_condition ::= ON expression.
group_key ::= expression.
having_clause ::= HAVING expression.
order_key ::= expression order_direction_opt.

row_tuple ::= LPAREN expression COMMA expression_list RPAREN.
expression ::= row_tuple comparison_operator row_tuple.
expression ::= row_tuple IN LPAREN query_expression RPAREN.
expression ::= row_tuple NOT IN LPAREN query_expression RPAREN.

update_assignment ::= qualified_identifier EQ expression.
delete_order_key ::= expression order_direction_opt.
```

This slice does not install the broad grammar above directly. MyLite's current
Lemon grammar and runtime planner are intentionally narrower. The implemented
behavior is a fallback classifier that admits recognized surfaces as explicit
unsupported placeholders after the normal parser fails.

## Runtime Behavior

No SQLite fork hook is needed. Execution is intentionally unchanged.
Recognized expression-clause query surfaces that fail normal parsing become
unsupported-utility placeholders and return the existing deterministic
unsupported diagnostic. Queries already supported by the normal parser keep
their existing AST and runtime behavior.

This preserves correctness: MyLite must not accidentally route arbitrary MySQL
expressions through SQLite with different type, collation, tuple, aggregate, or
NULL semantics.

## Tests

Tests cover:

- MySQL 8.4.9 expectations for arithmetic predicates, expression ordering and
  grouping, tuple predicates, subquery expression predicates, multi-table
  `UPDATE`, and ordered `DELETE`;
- parser placeholder acceptance for the representative expression-clause
  surfaces;
- runtime unsupported diagnostics for recognized expression-clause surfaces;
- regression coverage that existing simple predicates and flat joins continue
  to parse normally.

## Compatibility Status

This slice improves parser coverage by converting recognized general
expression-clause syntax failures into unsupported placeholders. It does not add
general executable expression semantics for table-backed query clauses.
