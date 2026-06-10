# Parser Corpus CTE Placeholder Surfaces

This slice reduces parser-corpus failures where MySQL accepts a statement that
starts with a common table expression, while MyLite does not yet implement CTE
planning or execution.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/with.html
- https://dev.mysql.com/doc/refman/8.4/en/select.html
- https://dev.mysql.com/doc/refman/8.4/en/update.html
- https://dev.mysql.com/doc/refman/8.4/en/delete.html
- https://dev.mysql.com/doc/refman/8.4/en/parenthesized-query-expressions.html

Runtime probes are verified against MySQL 8.4.9 before this slice is marked
complete.

## Scope

MySQL common table expressions begin with `WITH`, may include `RECURSIVE`, may
name one or more CTEs, may include optional CTE column lists, and attach to a
following query expression or supported DML statement shape. MyLite does not
execute CTEs yet, but applications and corpus fixtures should receive a
deterministic unsupported diagnostic instead of a generic syntax error for
recognized complete CTE-started statements.

In scope:

- `WITH name AS (<query>) SELECT ...`;
- `WITH name(column[, ...]) AS (<query>) SELECT ...`;
- comma-separated CTE definitions;
- duplicate CTE names, which MySQL parses before reporting semantic errors;
- `WITH RECURSIVE name AS (<query>) SELECT ...`;
- CTE-started parenthesized query expressions such as
  `WITH cte AS (SELECT 1) (SELECT * FROM cte) LIMIT 1`;
- CTE-prefixed `UPDATE` and `DELETE` statements;
- deterministic unsupported utility diagnostics at runtime.

Out of scope:

- CTE execution or materialization;
- recursive CTE fixpoint semantics;
- CTE name resolution, shadowing, column aliases, or duplicate-name errors;
- CTEs inside `INSERT ... SELECT`, `REPLACE ... SELECT`, `CREATE TABLE ...
  SELECT`, view bodies, subqueries, or stored programs;
- privilege, locking, optimizer, metadata, or protocol behavior;
- broad syntactic validation of every legal CTE subquery body.

## MyLite Grammar Snippet

These snippets describe the intended MyLite-owned grammar shape and do not copy
MySQL grammar.

```lemon
cte_statement ::= WITH recursive_opt cte_definition_list cte_consumer.

recursive_opt ::= .
recursive_opt ::= RECURSIVE.

cte_definition_list ::= cte_definition.
cte_definition_list ::= cte_definition_list COMMA cte_definition.

cte_definition ::= identifier cte_column_list_opt AS LPAREN query_expression RPAREN.

cte_column_list_opt ::= .
cte_column_list_opt ::= LPAREN identifier_list RPAREN.

cte_consumer ::= select_statement.
cte_consumer ::= parenthesized_query_expression.
cte_consumer ::= table_statement.
cte_consumer ::= values_statement.
cte_consumer ::= update_statement.
cte_consumer ::= delete_statement.
```

The implementation does not add these productions to Lemon yet. It uses the
existing syntax-error fallback classifier to recognize complete CTE-started
statements and return an unsupported-utility AST node. This keeps parser state
growth low while preserving a clear future path for real CTE grammar.

## Runtime Behavior

No SQLite fork hook is needed. A recognized CTE-started statement that normal
parsing cannot handle becomes `MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT`.
Runtime execution returns the existing MySQL-shaped syntax error code and
SQLSTATE with MyLite's deterministic unsupported utility message. Existing
non-CTE supported statements keep their current AST and runtime behavior.

This is intentionally conservative. MyLite must not execute CTE syntax by
rewriting it into SQLite because MySQL and SQLite differ in name resolution,
recursive behavior, typing, collation, optimizer rules, and diagnostics.

## Tests

Tests cover:

- MySQL 8.4.9 expectations for simple, column-list, recursive, parenthesized,
  `UPDATE`, and `DELETE` CTE forms;
- parser placeholder acceptance for representative corpus CTE surfaces;
- runtime unsupported diagnostics for the same CTE surfaces;
- regression coverage that existing non-CTE simple predicates continue to parse
  and run normally.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` must be rerun before commit to measure accepted
query movement.

## Compatibility Status

This slice improves parser compatibility by converting recognized complete
CTE-started syntax failures into unsupported placeholders. It does not add
general executable CTE semantics.
