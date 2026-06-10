# Parser Corpus Query Function And Subquery Surfaces

This slice reduces high-volume MySQL server-test parser failures where MySQL
accepts nested function-call, scalar-subquery, and named window-function syntax
in query expression positions, but MyLite's current executors still support
only documented descriptor-backed expression subsets.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- https://dev.mysql.com/doc/refman/8.4/en/select.html
- https://dev.mysql.com/doc/refman/8.4/en/window-functions-usage.html
- https://dev.mysql.com/doc/refman/8.4/en/comments.html

## MySQL 8.4.9 Runtime Observations

The expectation script for this slice verifies representative MySQL behavior
against MySQL 8.4.9:

- nested function calls may appear in projection and ordering expressions;
- scalar subqueries may appear in projection, ordering, `VALUES`, and DML value
  positions;
- aggregate window functions may use a named window and may also be referenced
  from `ORDER BY`;
- optimizer-hint comments do not change the result for the verified no-op
  query shape.

Observed probe:

```sql
CREATE TABLE t1 (a INT, b INT, c VARCHAR(20));
CREATE TABLE t2 (a INT, b INT);
INSERT INTO t1 VALUES (1,2,'x'),(3,4,'yy');
INSERT INTO t2 VALUES (1,20),(3,40);
SELECT HEX(c), HEX(LOWER(c)), HEX(UPPER(c)) FROM t1 ORDER BY BINARY(c);
SELECT a, (SELECT MAX(b) FROM t2 WHERE t2.a = t1.a) AS max_b
  FROM t1 ORDER BY a;
SELECT SUM(a) OVER w FROM t1
  WINDOW w AS (ORDER BY b ROWS CURRENT ROW)
  ORDER BY SUM(b) OVER w;
VALUES ROW((SELECT 1), 10);
INSERT INTO t1 (a, b, c) VALUES (10, (SELECT MAX(b) FROM t2), 'subq');
SELECT a,b,c FROM t1 WHERE a=10;
```

MySQL returns the result rows recorded in
`mysql_parser_corpus_query_function_subquery_surfaces_expectations.sh`.

## Scope

In scope:

- parser fallback classification for query statements that fail normal parsing
  but contain complete nested function-call syntax in table-backed query
  expression positions;
- parser fallback classification for complete parenthesized query expressions
  such as scalar subqueries in projection/order, `VALUES` row subqueries, and
  `INSERT` value subqueries after normal parsing fails;
- parser fallback classification for named aggregate-window expression
  surfaces after normal parsing fails;
- deterministic unsupported diagnostics instead of generic syntax errors for
  recognized function/subquery query surfaces;
- preserving existing normally parsed supported query, scalar function, window,
  and subquery paths;
- parser corpus movement measurement over
  `build/perf-data/mysql-server-tests-queries.csv`.

Out of scope:

- executable broad function-call semantics in table-backed projection,
  predicate, grouping, ordering, DML value, or `VALUES` row positions;
- executable broad scalar-subquery semantics beyond existing documented
  no-source, `DUAL`, assignment, and descriptor-backed predicate subsets;
- executable aggregate-window or named-window semantics beyond existing
  documented window-function envelopes;
- table-function execution such as `JSON_TABLE`;
- optimizer-hint payload parsing or optimizer behavior;
- malformed built-in function signatures, empty function argument lists for
  this fallback surface, `REPLACE` value subqueries, and `WITH ... UPDATE`;
- replacing the Lemon expression grammar with a broad conflict-prone catch-all.

## MyLite Grammar Snippet

These snippets describe the intended MyLite-owned grammar shape and do not copy
MySQL grammar.

```lemon
query_expression_operand ::= expression.

expression ::= function_name LPAREN function_argument_list_opt RPAREN.
expression ::= LPAREN query_expression RPAREN.
expression ::= aggregate_function_call OVER window_spec_or_name.
expression ::= window_function_call OVER window_spec_or_name.

insert_value ::= expression.
update_assignment ::= qualified_identifier EQ expression.
values_row_item ::= expression.
order_key ::= expression order_direction_opt.
```

This slice does not install the broad grammar above directly. MyLite's current
Lemon grammar and runtime planner are intentionally narrower. The implemented
behavior is a fallback classifier that admits recognized surfaces as explicit
unsupported placeholders after the normal parser fails.

## Runtime Behavior

No SQLite fork hook is needed. Execution is intentionally unchanged.
Recognized nested function/subquery query surfaces that fail normal parsing
become unsupported-utility placeholders and return the existing deterministic
unsupported diagnostic. Queries already supported by the normal parser keep
their existing AST and runtime behavior. Malformed built-in function forms and
the explicitly excluded DML/CTE shapes remain syntax errors.

This preserves correctness: MyLite must not accidentally route arbitrary MySQL
function calls, scalar subqueries, window functions, table functions, collations,
or expression metadata through SQLite with different MySQL semantics.

## Tests

Tests cover:

- MySQL 8.4.9 expectations for nested function projection/order expressions,
  scalar subqueries, named aggregate windows, `VALUES` row subqueries, and DML
  subquery values;
- parser placeholder acceptance for representative nested function/subquery
  surfaces;
- runtime unsupported diagnostics for recognized function/subquery surfaces;
- regression coverage that existing supported simple functions, scalar
  subqueries, and descriptor predicates still parse or execute normally;
- regression coverage that malformed function syntax, `REPLACE` value
  subqueries, and `WITH ... UPDATE` are not classified by this fallback.

## Compatibility Status

This slice improves parser coverage by converting recognized nested
function-call and subquery query syntax failures into unsupported placeholders.
It does not add general executable function, subquery, table-function, or
aggregate-window semantics.
