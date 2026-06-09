# Parser Corpus Query Expression Containers

This slice reduces high-volume parser-corpus failures where MySQL accepts a
query expression in a container that MyLite currently restricts to a plain
`SELECT` statement. The implemented corpus targets are `CREATE VIEW ... AS`
query-expression bodies and one derived-table query-compound form. Scalar and
predicate subqueries containing broader query expressions remain deferred
because the current scalar expression grammar needs a larger redesign to admit
them without Lemon conflicts.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/create-view.html
- https://dev.mysql.com/doc/refman/8.4/en/set-operations.html
- https://dev.mysql.com/doc/refman/8.4/en/parenthesized-query-expressions.html
- https://dev.mysql.com/doc/refman/8.4/en/scalar-subqueries.html
- https://dev.mysql.com/doc/refman/8.4/en/any-in-some-subqueries.html
- https://dev.mysql.com/doc/refman/8.4/en/exists-and-not-exists-subqueries.html
- https://dev.mysql.com/doc/refman/8.4/en/values.html

Runtime probes are verified against MySQL 8.4.9 before this slice is marked
complete.

## Scope

### Query Expression Containers

MySQL query expressions are made from query blocks and set operations. A query
block can be `SELECT`, `TABLE`, or `VALUES`, and set operations combine those
blocks with `UNION`, `INTERSECT`, or `EXCEPT`.

MyLite already parses these forms as top-level statements and in some derived
table positions. This slice extends the same query-expression acceptance to:

- `CREATE VIEW ... AS <query-expression>` and `ALTER VIEW ... AS
  <query-expression>`, including compound `SELECT`, `TABLE`, `VALUES`, and
  explicit parenthesized query expressions;
- derived table sources that wrap a `TABLE`- or `VALUES`-started set operation,
  such as `(VALUES ROW(1) UNION SELECT 2) AS dt(a)`.

This slice intentionally does not extend scalar subquery, `EXISTS`, or `IN`
predicate containers beyond the existing plain-`SELECT` subsets. Experimental
direct reductions for those forms overlapped with `parenthesized_query_expression`
and produced Lemon conflicts in general scalar expression states.

### Runtime Behavior

No SQLite fork hook is needed. This is parser coverage plus explicit runtime
diagnostics where MyLite's executor remains narrower than MySQL:

- existing supported plain-`SELECT` views keep their behavior;
- newly admitted `TABLE table_name` view bodies normalize through MyLite's
  existing `TABLE`-as-`SELECT * FROM table_name` AST and are supported when they
  fit the baseline persistent-base-table direct projection subset;
- newly admitted compound, `VALUES`, and parenthesized non-baseline view
  definitions fail with the existing `CREATE VIEW supports only a single SELECT
  statement` diagnostic until view execution/storage supports broader query
  expressions;
- existing supported `IN (SELECT ...)`, `EXISTS (SELECT ...)`, and scalar
  subquery subsets continue to plan through current runtime paths;
- newly admitted non-`SELECT` query-compound derived tables parse and then fail
  with a deterministic unsupported diagnostic from the current SELECT planning
  path.

This slice does not implement broad scalar `expr IN (...)` expression lists,
row-constructor membership, or broader query-expression bodies for scalar,
`EXISTS`, or `IN` subqueries. Those need a separate expression grammar design.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned Lemon grammar shape and do
not copy MySQL grammar.

```lemon
view_query_expression ::= select_statement.
view_query_expression ::= compound_select_statement.
view_query_expression ::= query_compound_statement.
view_query_expression ::= parenthesized_query_expression.
view_query_expression ::= table_statement.
view_query_expression ::= values_statement.

create_view_statement ::= CREATE view_options VIEW table_name
                          view_columns AS view_query_expression
                          view_check_option.

derived_table_source ::= LPAREN query_compound_statement RPAREN
                         derived_table_alias.
```

## Tests

MySQL 8.4.9 expectation probes cover:

- compound, `TABLE`, and `VALUES` `CREATE VIEW` bodies;
- `VALUES` `ALTER VIEW` bodies;
- existing top-level and derived-table query-expression behavior.

MyLite parser tests cover acceptance and representative AST shape. Runtime
tests cover preserved supported behavior plus deterministic unsupported errors
for newly admitted view and derived-table forms outside the current executor
subset.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` must be rerun before commit to measure accepted
query movement.

## Compatibility Status

This slice improves parser compatibility for view query-expression containers
and one derived query-compound container. `TABLE table_name` view bodies are
supported when they normalize to the existing baseline view subset; broader
query-expression execution in views, scalar subqueries, `EXISTS`, and `IN`
remains unsupported.
