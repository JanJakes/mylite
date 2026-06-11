# Parser Corpus Parenthesized Table Reference Surfaces

This slice reduces high-volume MySQL server-test parser failures around
parenthesized table references, parenthesized join expressions, parenthesized
comma table-reference lists, ODBC join escapes, and mixed comma/explicit join
lists in `FROM` and `JOIN` clauses.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/join.html
- https://dev.mysql.com/doc/refman/8.4/en/nested-join-optimization.html

## MySQL 8.4.9 Runtime Observations

The expectation script for this slice verifies these representative behaviors
against MySQL 8.4.9:

- `(t1 JOIN t2 ON ...) JOIN t3 ON ...` is accepted and, for inner joins, is
  equivalent to the same left-to-right join chain without parentheses.
- `t1 LEFT JOIN (t2 LEFT JOIN t3 ON ...) ON ...` is accepted and preserves the
  nested outer-join grouping. Flattening this shape can change results.
- `t1 LEFT JOIN (t2, t3) ON ...` is accepted. MySQL treats the comma list
  inside the parentheses as an inner-join group for the nested operand.
- `(t1)` is accepted as a parenthesized base table reference.
- `{ OJ t1 LEFT OUTER JOIN t2 ON ... }` is accepted as an ODBC left outer join
  escape.
- `t1, t2 LEFT JOIN t3 ON ...` is accepted and groups the explicit join as the
  right operand of the comma join.
- `t2 LEFT JOIN t3 ON ..., t1` is accepted and combines the explicit join with
  the trailing comma source.
- Derived table aliases continue to be required for subqueries in `FROM`.

Observed probe:

```sql
CREATE TABLE t1 (id INT PRIMARY KEY, v INT);
CREATE TABLE t2 (id INT PRIMARY KEY, v INT);
CREATE TABLE t3 (id INT PRIMARY KEY, v INT);
INSERT INTO t1 VALUES (1,10),(2,20);
INSERT INTO t2 VALUES (1,100),(3,300);
INSERT INTO t3 VALUES (1,1000),(3,3000);
SELECT COUNT(*) FROM (t1 JOIN t2 ON t1.id=t2.id) JOIN t3 ON t2.id=t3.id;
SELECT COUNT(*) FROM t1 LEFT JOIN (t2 LEFT JOIN t3 ON t2.id=t3.id) ON t1.id=t2.id;
SELECT COUNT(*) FROM t1 LEFT JOIN (t2, t3) ON t1.id=t2.id;
SELECT COUNT(*) FROM (t1);
SELECT COUNT(*) FROM { OJ t1 LEFT OUTER JOIN t2 ON t1.id=t2.id };
SELECT COUNT(*) FROM t1, t2 LEFT JOIN t3 ON t2.id=t3.id;
SELECT COUNT(*) FROM t2 LEFT JOIN t3 ON t2.id=t3.id, t1;
```

MySQL returns `1`, `2`, `3`, `2`, `2`, `4`, and `4`, respectively.

## Scope

In scope:

- parenthesized base table references, such as `FROM (t1)` or
  `JOIN (t2) ON ...`, classified as explicit unsupported placeholders after
  normal parse failure;
- repeated parenthesized base table references, such as `FROM ((t1))`,
  classified as explicit unsupported placeholders after normal parse failure;
- parenthesized joined table references, such as
  `(t1 JOIN t2 ON ...) JOIN t3 ON ...`, classified as explicit unsupported
  placeholders after normal parse failure;
- repeated parenthesized joined table references, such as
  `FROM ((t1 JOIN t2 ON TRUE))`, classified as explicit unsupported
  placeholders after normal parse failure;
- parenthesized comma table-reference lists, such as
  `t1 LEFT JOIN (t2, t3) ON ...`, classified as explicit unsupported
  placeholders after normal parse failure;
- nested parenthesized comma table-reference lists, such as
  `FROM ((t1, t2))` and `FROM ((t1, (t2, t3)))`, classified as explicit
  unsupported placeholders after normal parse failure;
- ODBC `{ OJ ... }` join escapes classified as explicit unsupported
  placeholders after normal parse failure;
- mixed comma/explicit join lists, such as
  `t1, t2 LEFT JOIN t3 ON ...` and `t2 LEFT JOIN t3 ON ..., t1`, classified as
  explicit unsupported placeholders after normal parse failure;
- delayed nested join condition chains, such as
  `t1 LEFT JOIN t2 LEFT JOIN t3 ON ... ON ...`, classified as explicit
  unsupported placeholders after normal parse failure;
- deterministic unsupported diagnostics instead of generic parse errors for
  recognized table-reference groups;
- parser corpus movement measurement over
  `build/perf-data/mysql-server-tests-queries.csv`.

Out of scope:

- executable nested outer-join semantics;
- executable right-nested join trees, such as
  `t1 LEFT JOIN (t2 LEFT JOIN t3 ON ...) ON ...`;
- executable parenthesized comma groups used as the inner side of an outer
  join, such as `t1 LEFT JOIN (t2, t3) ON ...`;
- executable ODBC `{ OJ ... }` join escapes;
- executable mixed comma/explicit join precedence;
- executable delayed nested join condition chains;
- general table-reference aliases on parenthesized join groups;
- lateral derived tables and table functions;
- broader join condition expressions beyond existing MyLite join support.

## MyLite Grammar Snippet

These snippets describe the intended MyLite-owned grammar shape and do not copy
MySQL grammar.

```lemon
table_source ::= table_name table_alias_opt table_index_hints_opt.
table_source ::= LPAREN table_source RPAREN.
table_source ::= LPAREN joined_table_source RPAREN.
table_source ::= LPAREN comma_table_sources RPAREN.
table_source ::= LBRACE OJ joined_table_source RBRACE.

joined_table_source ::= table_source join_operator table_source join_condition_opt.
joined_table_source ::= joined_table_source join_operator table_source join_condition_opt.
joined_table_source ::= LPAREN joined_table_source RPAREN.
joined_table_source ::= LPAREN comma_table_sources RPAREN.

comma_table_sources ::= table_source COMMA table_source.
comma_table_sources ::= comma_table_sources COMMA table_source.
comma_table_sources ::= LPAREN comma_table_sources RPAREN.
comma_table_sources ::= table_source COMMA joined_table_source.
comma_table_sources ::= joined_table_source COMMA table_source.
```

The implementation may add a narrower conflict-free subset if the full snippet
creates Lemon conflicts. It must not flatten right-nested outer joins or
parenthesized inner operands of outer joins into a different executable plan.

## Runtime Behavior

No SQLite fork hook is needed. This slice uses a MyLite parser fallback
classifier because the direct table-reference grammar shape currently creates
Lemon conflicts or impractically expensive parser-generation recursion with
MyLite's existing join and derived-source grammar.

Execution is intentionally unchanged. Recognized parenthesized table-reference
groups, ODBC join escapes, and mixed comma/explicit join lists that fail the
normal parser become unsupported-utility placeholders and return the existing
deterministic unsupported diagnostic. Existing flat joins and existing
derived-table sources continue to parse normally and use their current runtime
paths. This protects MySQL outer-join and comma-precedence semantics until
MyLite owns a conflict-free table-reference grammar and planner for nested
joins.

## Tests

Tests cover:

- MySQL 8.4.9 expectations for left-deep, right-nested, comma-group, and
  parenthesized-base forms;
- parser placeholder acceptance for left-deep parenthesized joins,
  parenthesized base table references, parenthesized join operands, and comma
  groups, ODBC join escapes, and mixed comma/explicit join lists;
- runtime unsupported diagnostics for recognized table-reference groups;
- regression coverage for existing derived-table alias diagnostics and flat
  join behavior.

## Compatibility Status

This slice improves parser coverage by converting recognized parenthesized,
ODBC-escaped, and mixed comma/explicit table-reference syntax failures into
unsupported placeholders. It does not add executable parenthesized
table-reference, ODBC escape, mixed-precedence, or nested join semantics.
