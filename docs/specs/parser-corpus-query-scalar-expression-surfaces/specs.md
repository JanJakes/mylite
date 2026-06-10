# Parser Corpus Query Scalar Expression Surfaces

This slice reduces high-volume MySQL server-test parser failures around scalar
predicates used as expressions, expression keys in query clauses, query
expression containers inside scalar predicates, and lateral derived-table
syntax. The goal is to admit MySQL 8.4.9-shaped syntax where MyLite can safely
preserve semantics or return a deterministic unsupported diagnostic.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- https://dev.mysql.com/doc/refman/8.4/en/any-in-some-subqueries.html
- https://dev.mysql.com/doc/refman/8.4/en/subqueries-all.html
- https://dev.mysql.com/doc/refman/8.4/en/group-by-modifiers.html
- https://dev.mysql.com/doc/refman/8.4/en/lateral-derived-tables.html

Runtime probes are verified against MySQL 8.4.9 before this slice is marked
complete.

## Scope

### Scalar `IN` Expressions

MySQL accepts `expr [NOT] IN (expr[, ...])` and
`expr [NOT] IN (subquery)` wherever scalar expressions are accepted. It also
accepts row-constructor membership, such as `(a, b) IN ((1, 2), (3, 4))`, and
query-expression containers such as `IN (VALUES ROW(...))`.

The broad Lemon grammar form overlaps with existing predicate and function-call
rules in this parser. For this slice, existing supported descriptor-column
`WHERE ... IN (...)` predicates keep their current runtime behavior, while
top-level query and DML statements that still fail the normal parse but contain
recognizable scalar `IN` / `NOT IN` parenthesized operands are classified as
explicit unsupported placeholders. Existing descriptor-style `WHERE`, `HAVING`,
and `ON` predicates with a non-parenthesized left operand stay on the normal
parser/runtime path so malformed or unsupported predicate value lists keep their
current diagnostics. MyLite must not silently lower these broad forms to SQLite
because row membership, multi-row subqueries, and mixed-domain membership use
MySQL semantics that are not implemented yet.

### Quantified Subquery Comparisons

MySQL accepts `operand comparison_operator {ANY | SOME | ALL} (subquery)`.
`ANY` and `SOME` are synonyms; `ALL` requires every subquery value to satisfy
the comparison. This slice recognizes the syntax after a normal parse failure
and returns an AST placeholder with a deterministic unsupported diagnostic.
MyLite must not lower these forms to scalar-subquery comparisons because that
would be semantically wrong for multi-row subqueries.

### Query Expression Subquery Containers

MySQL's expression grammar uses `subquery` containers rather than only a
single unparenthesized `SELECT` query block. This slice admits `VALUES`
containers in the existing `EXISTS` and scalar subquery expression grammar. It
also recognizes unsupported `IN` and quantified-subquery containers through
the parse-failure placeholder path. The intended broad subquery bodies are:

- `SELECT` query blocks;
- `VALUES` query blocks;
- parenthesized query expressions admitted by the existing query-expression
  surface slice.

Execution remains limited to MyLite's current supported subquery subsets.
Unsupported `VALUES` and parenthesized query-expression containers fail
deterministically.

### Query Clause Expression Keys

MySQL accepts expressions in `GROUP BY`, `HAVING`, and `ORDER BY`. MyLite
already has a main expression grammar with operator precedence and many scalar
function nodes, while those clauses still maintain narrower local mini
grammars. The broad direct grammar rewrite caused unacceptable parser
conflicts. This slice therefore keeps the existing clause grammar intact and
relies only on already parser-admitted clause placeholders, such as current
`GROUPING()` and `WITH ROLLUP` syntax.

Runtime support remains unchanged except where existing planners already
support the expression shape. Unsupported grouping, ordering, `GROUPING()`,
rollup, window, and broad HAVING expressions must keep returning explicit
unsupported diagnostics.

### Lateral Derived Tables

MySQL accepts `LATERAL (subquery) alias` only for derived tables in `FROM`
clauses and join specifications. This slice recognizes `LATERAL (subquery)`
after a normal parse failure for query and DML statements and returns an
unsupported placeholder. MyLite does not implement correlated lateral
execution in this slice; all lateral derived sources must be rejected with a
deterministic unsupported diagnostic.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned grammar shape and do not copy
MySQL grammar. The direct scalar `IN`, quantified-subquery, broad clause
expression, and lateral productions remain future work because they currently
conflict with MyLite's local predicate and function-call grammar. This slice
implements the `VALUES` subquery-container grammar plus explicit parse-failure
placeholder classification for the remaining snippets.

```lemon
expression ::= expression IN LPAREN scalar_in_expression_list RPAREN.
expression ::= expression NOT IN LPAREN scalar_in_expression_list RPAREN.
expression ::= expression IN LPAREN subquery_expression_body RPAREN.
expression ::= expression NOT IN LPAREN subquery_expression_body RPAREN.

scalar_in_expression_list ::= expression.
scalar_in_expression_list ::= scalar_in_expression_list COMMA expression.
scalar_in_expression_list ::= row_constructor_expression.
scalar_in_expression_list ::= scalar_in_expression_list COMMA row_constructor_expression.

expression ::= expression comparison_operator quantified_subquery.
quantified_subquery ::= ANY LPAREN subquery_expression_body RPAREN.
quantified_subquery ::= SOME LPAREN subquery_expression_body RPAREN.
quantified_subquery ::= ALL LPAREN subquery_expression_body RPAREN.

subquery_expression_body ::= select_statement.
subquery_expression_body ::= values_statement.
subquery_expression_body ::= parenthesized_query_expression.
```

```lemon
group_key ::= expression.
select_order_key ::= expression.
having_predicate ::= expression.
```

```lemon
derived_table_source ::= LATERAL LPAREN subquery_expression_body RPAREN derived_table_alias.
```

## Runtime Behavior

No SQLite fork hook is needed. This is MyLite parser/AST work plus explicit
runtime classification:

- existing supported `WHERE ... IN (...)` subsets continue to execute through
  the current predicate planner;
- `VALUES` containers in `EXISTS` and scalar subquery positions parse to the
  corresponding expression AST nodes and fail with the existing unsupported
  non-`SELECT` diagnostics at execution time;
- scalar `IN`, row membership, quantified subqueries, and lateral derived
  tables that reach the fallback classifier fail with deterministic unsupported
  placeholder diagnostics;
- expression-level broad HAVING/GROUP/ORDER forms remain limited to existing
  parser-admitted shapes;
- newly admitted syntax must not be translated to SQLite SQL in a way that
  changes MySQL semantics.

## Tests

MySQL 8.4.9 expectations cover representative syntax and observed results for
scalar `IN`, `NOT IN`, quantified comparisons, `VALUES` containers, existing
`GROUPING()` syntax, and lateral derived tables.

MyLite parser tests cover parse acceptance and AST shape. Runtime tests cover
existing supported paths plus explicit diagnostics for newly admitted
unsupported forms.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` must be rerun before commit to measure accepted
query movement.

## Compatibility Status

This slice moves selected expression/query syntax from parser failures to
support or documented parser-placeholder behavior. It does not mark broad
scalar `IN` evaluation, quantified subquery execution, lateral derived-table
execution, full grouping/order expression execution, or general subquery
query-expression execution as supported.
