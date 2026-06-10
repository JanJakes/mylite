# Parser Corpus Row Constructor Predicates

This slice reduces parser-corpus failures around MySQL `ROW(...)` constructors
used in scalar comparison projections. It extends the earlier parser-corpus
expression-query surface so MyLite has a dedicated row-constructor AST instead
of treating `ROW(...)` as an ordinary generic function.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/row-subqueries.html
- https://dev.mysql.com/doc/refman/8.4/en/row-constructor-optimization.html
- https://dev.mysql.com/doc/refman/8.4/en/subquery-restrictions.html
- https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html

Runtime probes are verified against MySQL 8.4.9.

## Scope

MySQL row constructors are written as either `(expr, expr[, ...])` or
`ROW(expr, expr[, ...])`; the forms are equivalent in comparison contexts.
`ROW(1)` is a syntax error, `(1)` remains an ordinary parenthesized expression,
and a multi-column row constructor used as a plain scalar result, such as
`SELECT ROW(1,2)` or `SELECT (1,2)`, fails with `1241 / 21000`.

This slice supports:

- parser acceptance for `ROW(expr, expr[, ...])` row constructor expressions
  with at least two elements through the existing keyword-function expression
  grammar;
- parser acceptance for parenthesized `(expr, expr[, ...])` row constructor
  expressions with at least two elements through a targeted parser-driver retry
  for `SELECT` statements that fail the normal grammar pass;
- parser acceptance for `ROW(...)` and parenthesized row constructor comparison
  operators `=`, `<=>`, `<>`, `!=`, `<`, `<=`, `>`, and `>=` in no-source and
  `FROM DUAL` scalar projections;
- scalar projection execution for keyword `NOT` around those admitted row
  constructor comparisons;
- tableless scalar projection execution for `ROW(...)` and parenthesized row
  constructor comparisons when every row element is an existing scalar
  expression supported by MyLite's scalar comparison evaluator, plus string
  literal and current scalar string helper pairs compared with MyLite's current
  ASCII case-insensitive padded string semantics;
- MySQL-shaped arity diagnostics for row constructor comparisons with mismatched
  element counts;
- MySQL-shaped syntax rejection for `ROW()` and `ROW(single_expr)`;
- MySQL-shaped operand-count diagnostics for plain scalar `ROW(...)`
  projections.

This slice does not implement:

- tuple literal `IN` / `NOT IN`;
- row constructors as ordinary scalar values beyond MySQL's operand-count
  diagnostic;
- row subquery execution;
- `ALL`, `ANY`, or `SOME` multi-column row comparisons;
- table-backed tuple predicate lowering, tuple index planning, or row
  constructor optimizer rewrites;
- general expression elements beyond MyLite's current tableless scalar
  comparison subset.

## Observed MySQL 8.4.9 Behavior

Representative probes:

```sql
SELECT ROW(1,2);
-- ERROR 1241 (21000): Operand should contain 1 column(s)

SELECT ROW(1);
-- ERROR 1064 (42000)

SELECT (1,2) = (1,2), (1,2) = (1,3), (1,2) <> (1,3);
-- 1, 0, 1

SELECT ROW(1,2) = ROW(1,2), ROW(1,2) = ROW(1,3), ROW(1,2) <> ROW(1,3);
-- 1, 0, 1

SELECT ROW(1,NULL) = ROW(1,NULL), ROW(1,NULL) <=> ROW(1,NULL),
       ROW(2,NULL) > ROW(1,9);
-- NULL, 1, 1

SELECT NOT ROW(1,2) = ROW(1,3), NOT ROW(1,NULL) = ROW(1,NULL);
-- 1, NULL

SELECT ROW('b',1) > ROW('a',9), ROW('b',1) = ROW('B',1),
       ROW('b ',1) = ROW('b',1), ROW('123',1) = ROW(123,1);
-- 1, 1, 1, 1

SELECT ROW(1,2) = ROW(1,2,3);
-- ERROR 1241 (21000): Operand should contain 2 column(s)
```

Table-backed probes show that MySQL also supports `WHERE (a,b) = (1,2)`,
`WHERE ROW(a,b) = ROW(1,2)`, `WHERE (a,b) IN ((1,2),(3,4))`, and
`WHERE (a,b) IN (SELECT x,y FROM t2)`. MyLite leaves those execution paths
outside this slice.

## MyLite Grammar Snippets

These snippets describe MyLite-owned Lemon grammar shape and do not copy MySQL
grammar.

```lemon
row_constructor ::= ROW LPAREN function_argument_list RPAREN.
row_constructor ::= LPAREN expression COMMA function_argument_list RPAREN.
expression ::= row_constructor.
expression ::= expression predicate_comparison_operator expression.
```

The implementation reuses MyLite's existing keyword-function expression grammar
and maps `ROW(...)` with at least two arguments to a dedicated row-constructor
AST in the parser helper. The parenthesized tuple production above is the
semantic shape: the implementation does not add that production directly to the
main Lemon grammar. Instead, after a normal `SELECT` parse failure, the parser
pre-scans for tuple-shaped parentheses and retries with a synthetic `ROW` token
before matching parenthesized row constructors. `(expr)` keeps the ordinary
parenthesized-expression AST. This avoids broad grammar expansion while still
giving runtime and future tuple planning a row-specific node kind.

## Runtime Behavior

No SQLite fork hook is needed. This is MyLite parser, AST, scalar runtime, and
predicate-planner work:

- tableless scalar row comparison evaluates left-to-right using MySQL's
  three-valued row comparison semantics;
- corresponding string row elements are compared with the current MyLite
  nonbinary ASCII string comparison behavior: case-insensitive and ignoring
  trailing spaces; mixed string/numeric elements use the existing scalar numeric
  comparison coercion path;
- `<=>` returns `1` when both corresponding elements are `NULL`, `0` when only
  one side is `NULL`, and otherwise compares non-`NULL` values;
- keyword `NOT` inverts `0` and `1` row-comparison results and preserves
  `NULL`;
- ordinary comparison operators return `NULL` when the decisive comparison is
  unknown, except when an earlier non-`NULL` element comparison already decides
  lexicographic order;
- mismatched tuple arity reports `1241 / 21000` with the expected-column count;
- row constructor values used as plain scalar projections report
  `1241 / 21000`.

## Tests

MySQL 8.4.9 expectations cover syntax, scalar projection results, NULL
semantics, arity diagnostics, and standalone row-constructor diagnostics.
MyLite parser tests cover AST shape for `ROW(...)` and parenthesized tuple
constructors. Runtime tests cover executable tableless scalar comparison
results.

The parser corpus benchmark over
`build/perf-data/mysql-server-tests-queries.csv` must be rerun before commit.
The latest local benchmark after adding parenthesized tuple normalization was:

```text
parse.csv.mysql_server_tests: queries=69595 ok=68997 errors=598
parse_status: lexer_error=21 syntax_error=576 stack_overflow=1
```

## Compatibility Status

This slice moves `ROW(...)` and parenthesized tuple constructor comparisons
from parser failures or generic placeholder behavior to limited supported
scalar behavior. Tuple `IN`, table-backed tuple predicates, and row subqueries
remain documented incompatibilities until predicate planning grows tuple
support.
