# Parser Corpus Query Expression Clause Surfaces

This slice reduces high-volume MySQL server-test parser failures where MySQL
accepts general expression syntax in query clauses, but MyLite's currently
implemented query executors still support only documented descriptor-backed
subsets.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- https://dev.mysql.com/doc/refman/8.4/en/select.html
- https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- https://dev.mysql.com/doc/refman/8.4/en/json-search-functions.html
- https://dev.mysql.com/doc/refman/8.4/en/group-by-modifiers.html

## MySQL 8.4.9 Runtime Observations

The expectation script for this slice verifies representative MySQL behavior
against MySQL 8.4.9:

- arithmetic predicates such as `a + 1 > 1` are valid `WHERE` expressions and
  are executable through the baseline row arithmetic predicate subset;
- operator-bearing function and arithmetic expressions are valid `ORDER BY`,
  `GROUP BY`, and `HAVING` operands;
- row tuple comparisons such as `(a,b) = (1,2)` are valid predicates;
- comparison-result postfix `IS [NOT] NULL` / `IS [NOT] UNKNOWN` predicates,
  JSON `->` / `->>` column-path predicates, ODBC expression escapes, qualified
  column-to-column `BETWEEN`,
  qualified descriptor `IN` lists, `ROW(...)` comparisons, parenthesized
  `MATCH(...) AGAINST(...)`, and string-literal `ORDER BY` keys in `VALUES`
  statements are valid syntax and execute through the baseline `VALUES`
  statement subset;
- subqueries may contain their own expression predicates;
- multi-table `UPDATE` and ordered `DELETE` statements may contain expression
  assignments and predicates.

Observed probe:

```sql
CREATE TABLE t1 (a INT, b INT, c VARCHAR(20));
CREATE TABLE t2 (a INT, b INT);
CREATE TABLE t (u INT NULL);
INSERT INTO t1 VALUES (1,2,'x'),(3,4,'y');
INSERT INTO t2 VALUES (1,20),(3,40);
INSERT INTO t VALUES (256),(257),(NULL);
SELECT COUNT(*) FROM t1 WHERE a + 1 > 1;
SELECT a FROM t1 ORDER BY ABS(b - 5);
SELECT a, COUNT(*) FROM t1
  GROUP BY a + 0
  HAVING COUNT(*) >= 1 AND a > 0
  ORDER BY a + 0;
SELECT a FROM t1 WHERE (a,b) = (1,2);
SELECT COUNT(*) FROM t WHERE u=256 IS NOT NULL;
SELECT COUNT(*) FROM t WHERE u=256 IS UNKNOWN;
SELECT COUNT(*) FROM t1 WHERE j->"$.id" = 5;
SELECT COUNT(*) FROM t1 WHERE j->>"$.name" = "James";
SELECT {fn CONCAT('a','b')};
SELECT COUNT(*) FROM t1 LEFT JOIN t2 ON t1.a = t2.a
  WHERE t1.a BETWEEN t2.b AND t1.b;
SELECT COUNT(*) FROM t1 LEFT JOIN t2 ON t1.a = t2.a
  WHERE t1.a IN(t2.a, t2.b);
SELECT COUNT(*) FROM t1 WHERE ROW(1,2,'x')=ROW(a,b,c);
SELECT x FROM ft GROUP BY x, MATCH(x) AGAINST ('abc')
  HAVING MATCH(x) AGAINST ('abc') ORDER BY x;
VALUES ROW(1),ROW(2) ORDER BY '1' DESC;
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
- arithmetic, bitwise, and logical expression operators in query clauses,
  except the separately executable baseline row arithmetic predicate subset;
- expression operators inside function arguments and aggregate arguments;
- row tuple comparison and tuple `IN` surfaces;
- postfix `IS` predicates over broader expression operands, with
  comparison-result `IS NULL`, `IS NOT NULL`, `IS UNKNOWN`, and
  `IS NOT UNKNOWN` now executable through the baseline comparison-result slice;
- JSON column extraction operators `->` and `->>` when broad query-clause
  expression planning is not yet available;
- ODBC `{fn ...}`, `{d ...}`, `{t ...}`, and `{ts ...}` expression escapes;
- qualified column-to-column `BETWEEN` and descriptor `IN` lists in joined
  predicates;
- `ROW(...)` comparison predicates in table-backed clauses;
- parenthesized full-text `MATCH(column[, ...]) AGAINST(...)` expressions;
- string-literal `ORDER BY` keys in `VALUES` statement order clauses, now
  executable through the baseline standalone `VALUES` subset;
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
- executable JSON extraction through query-clause predicates outside existing
  documented JSON expression slices;
- executable ODBC expression escape conversion;
- executable column-to-column range or membership semantics beyond documented
  descriptor predicates;
- executable full-text search ranking or index lookup;
- visible string-literal `VALUES` sorting semantics beyond the current
  constructor-order behavior;
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
expression ::= ROW LPAREN expression COMMA expression_list RPAREN
               comparison_operator
               ROW LPAREN expression COMMA expression_list RPAREN.
expression ::= expression IS NULL.
expression ::= expression IS NOT NULL.
expression ::= expression IS TRUE.
expression ::= expression IS FALSE.
expression ::= expression IS UNKNOWN.
expression ::= qualified_identifier JSON_EXTRACT_OPERATOR string_literal.
expression ::= qualified_identifier JSON_UNQUOTE_EXTRACT_OPERATOR string_literal.
expression ::= ODBC_ESCAPE_START identifier expression ODBC_ESCAPE_END.
expression ::= expression BETWEEN expression AND expression.
expression ::= expression IN LPAREN expression_list RPAREN.
expression ::= MATCH LPAREN identifier_list RPAREN AGAINST LPAREN expression RPAREN.

update_assignment ::= qualified_identifier EQ expression.
delete_order_key ::= expression order_direction_opt.
values_order_key ::= expression order_direction_opt.
```

This slice does not install the broad grammar above directly. MyLite's current
Lemon grammar and runtime planner are intentionally narrower. The implemented
behavior is mostly a fallback classifier that admits recognized surfaces as
explicit unsupported placeholders after the normal parser fails. The
representative row arithmetic predicate cases are now handled by the normal
parser/runtime through
[baseline row arithmetic predicates](../baseline-row-arithmetic-predicates/specs.md)
and
[baseline parenthesized row arithmetic predicates](../baseline-parenthesized-row-arithmetic-predicates/specs.md).

## Runtime Behavior

No SQLite fork hook is needed. Recognized expression-clause query surfaces that
fail normal parsing become unsupported-utility placeholders and return the
existing deterministic unsupported diagnostic. Queries already supported by the
normal parser keep their existing AST and runtime behavior. The executable
exception in this corpus slice is the separately documented row arithmetic
predicate subset.

This preserves correctness: MyLite must not accidentally route arbitrary MySQL
expressions through SQLite with different type, collation, tuple, aggregate, or
NULL semantics.

## Tests

Tests cover:

- MySQL 8.4.9 expectations for arithmetic predicates, expression ordering and
  grouping, tuple predicates, postfix `IS`, JSON arrows, ODBC escapes,
  qualified column-to-column range and membership predicates, `ROW(...)` comparisons,
  parenthesized full-text `MATCH`, `VALUES` string order keys, subquery
  expression predicates, multi-table `UPDATE`, and ordered `DELETE`;
- parser placeholder acceptance for the still-unsupported representative
  expression-clause surfaces;
- normal parser/runtime execution for the row arithmetic predicate
  representatives;
- runtime unsupported diagnostics for recognized expression-clause surfaces,
  except `VALUES` string order keys, which execute through the baseline
  standalone `VALUES` subset;
- regression coverage that existing simple predicates and flat joins continue
  to parse normally.

## Compatibility Status

This slice improves parser coverage by converting recognized general
expression-clause syntax failures into unsupported placeholders. The row
arithmetic predicate representatives have since moved from placeholders to
normal execution, but the slice does not add general executable expression
semantics for table-backed query clauses.
