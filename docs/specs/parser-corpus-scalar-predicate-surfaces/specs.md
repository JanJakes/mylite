# Parser Corpus Scalar Predicate Surfaces

This slice reduces high-volume MySQL server-test parser failures where MySQL
allows predicate operators as scalar expressions. MyLite already has AST nodes
and WHERE-planning paths for `BETWEEN` and `EXISTS`; this work admits those
syntax forms in expression positions while preserving explicit unsupported
diagnostics where execution is still broader than MyLite's current scalar
executor. Scalar `IN` remains deferred because its straightforward grammar shape
conflicts with the current SELECT-expression grammar.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- https://dev.mysql.com/doc/refman/8.4/en/exists-and-not-exists-subqueries.html
- https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html

Runtime probes are verified against MySQL 8.4.9 before this slice is marked
complete.

## Scope

### BETWEEN Expressions

MySQL accepts `expr BETWEEN lower AND upper` and
`expr NOT BETWEEN lower AND upper` wherever scalar expressions are accepted,
including select-list items, nested expression arguments, and WHERE predicates.
This slice parses expression-level `BETWEEN` into MyLite's existing
`MYLITE_SQL_AST_BETWEEN_PREDICATE` node. Existing supported WHERE predicate
forms keep their behavior. Newly admitted select-list and nested expression
forms report an explicit unsupported diagnostic until scalar predicate
evaluation is implemented outside WHERE planning.

### EXISTS Expressions

MySQL accepts `EXISTS (subquery)` and `NOT EXISTS (subquery)` as scalar
predicates. This slice parses expression-level `EXISTS` into MyLite's existing
`MYLITE_SQL_AST_EXISTS_PREDICATE` node. `NOT EXISTS` currently parses through
the existing expression-level logical-not operator.

### Deferred IN Expressions

Expression-level `IN` membership is intentionally left out of this slice. The
straightforward operator grammar creates Lemon conflicts with the current
SELECT-expression grammar. MyLite already supports documented WHERE `IN`
predicate subsets through the predicate grammar; scalar select-list `IN` and
row-constructor membership need a follow-up grammar design that does not
destabilize the parser.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned Lemon grammar shape and do
not copy MySQL grammar.

```lemon
expression ::= EXISTS LPAREN select_statement RPAREN.
expression ::= expression BETWEEN scalar_predicate_value
               AND scalar_predicate_value.
expression ::= expression NOT BETWEEN scalar_predicate_value
               AND scalar_predicate_value.

scalar_predicate_value ::= literal.
scalar_predicate_value ::= signed_integer_literal.
scalar_predicate_value ::= qualified_identifier.
scalar_predicate_value ::= cast_convert_expression.
scalar_predicate_value ::= system_or_user_variable.
```

## Runtime Behavior

No SQLite fork hook is needed. This is parser and diagnostic-routing work:

- existing supported WHERE `BETWEEN` predicates continue to plan through the
  existing predicate executor;
- scalar `BETWEEN` and `EXISTS` in select lists or unsupported nested expression
  positions fail with deterministic unsupported diagnostics rather than syntax
  errors;
- subquery forms are parsed but execute only where MyLite already supports the
  subquery predicate subset.

## Tests

MySQL 8.4.9 expectations cover representative scalar expression results for
`BETWEEN`, `EXISTS`, and `NOT EXISTS`. MyLite parser tests cover parse
acceptance and AST shape. Runtime tests cover existing supported WHERE behavior
and explicit unsupported diagnostics for newly admitted scalar select-list
forms.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` must be rerun before commit to measure accepted
query movement.

## Compatibility Status

This slice improves parser compatibility for predicate-as-expression syntax.
It does not mark broad scalar predicate evaluation, scalar `IN`, row-constructor
membership, or general subquery predicate execution as supported.
