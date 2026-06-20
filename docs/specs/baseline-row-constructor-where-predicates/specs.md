# Baseline Row Constructor WHERE Predicates

This slice implements a narrow executable table-backed subset of MySQL row
constructor comparison predicates. It follows the scalar row-constructor
baseline and removes the representative parser-corpus placeholder for
`ROW(1,2,'x') = ROW(a,b,c)`.

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

SELECT COUNT(*) FROM t1 WHERE ROW(1,2,'x') = ROW(a,b,c);
-- 1
```

MySQL evaluates row constructor comparisons lexicographically for order
operators and element-wise for equality. In `WHERE`, `NULL` comparison results
filter out rows like other unknown predicates.

## Scope

In scope:

- `WHERE ROW(value[, ...]) comparison_operator ROW(value[, ...])` in existing
  single-table predicate envelopes;
- equality, null-safe equality, and inequality operators (`<>` and `!=`) over tuple elements
  where each pair is one descriptor column and one supported literal predicate
  value;
- MySQL-shaped `1241 / 21000` arity diagnostics for mismatched tuple sizes;
- runtime lowering to existing descriptor comparison predicates, so SQLite
  still evaluates the row filter.

Out of scope:

- tuple `IN` / `NOT IN`;
- row subqueries, `ALL`, `ANY`, or `SOME`;
- tuple comparisons where both corresponding elements are expressions;
- parenthesized tuple predicate syntax such as `(a,b) = (1,2)`;
- column-to-column tuple element comparisons outside already supported
  same-scope column comparison rules;
- lexicographic `<`, `<=`, `>`, and `>=` tuple predicates;
- tuple index planning or optimizer rewrites.

## MyLite Grammar Snippet

These snippets describe the intended MyLite-owned grammar shape.

```lemon
predicate_atom ::= predicate_row_constructor comparison_operator
        predicate_row_constructor.
predicate_row_constructor ::= ROW LPAREN expression COMMA expression_list RPAREN.
```

The main parser admits explicit `ROW(...)` constructors through the normal
predicate grammar. This slice changes the runtime planner for supported
table-backed tuple comparisons from an unsupported placeholder diagnostic to an
executable predicate tree.

## Runtime Behavior

No SQLite fork hook is needed. MyLite decomposes supported tuple equality into
existing descriptor comparison predicate nodes joined with `AND`; inequality is
implemented as the negation of that equality tree. Null-safe equality uses
element-wise null-safe comparisons.

This keeps MyLite's existing type conversion, date/datetime comparison mode,
string collation, parameter binding, and SQLite filtering behavior for each
element. It does not load table rows into memory or post-filter rows.

## Tests

Tests cover:

- MySQL 8.4.9 expectation for the representative row-constructor predicate;
- parser movement from unsupported placeholder to normal predicate admission;
- runtime row-count coverage for equality, null-safe equality, inequality, and
  arity mismatch diagnostics;
- preservation of unsupported diagnostics for parenthesized tuple shapes and
  other tuple forms outside this slice.

## Compatibility Status

This slice narrows the row-constructor and `WHERE` gaps for explicit
descriptor-backed `ROW(...)` equality/null-safe-equality/inequality predicates.
Parenthesized tuple predicates, tuple membership, row subqueries, and
lexicographic tuple ordering remain documented gaps.
