# UPDATE JOIN

## Scope

This feature implements MySQL multiple-table `UPDATE` syntax for joined table
references:

- `UPDATE t JOIN u ON ... SET t.col = expr [WHERE expr]`
- `UPDATE t LEFT JOIN u ON ... SET t.col = expr [WHERE expr]`
- `UPDATE t, u SET t.col = expr [WHERE expr]`
- assignments to one or more joined base tables

`ORDER BY` and `LIMIT` remain single-table-only. MySQL rejects them for
multiple-table `UPDATE`, so MyLite keeps them outside the joined grammar.

## Sources

The behavior is specified from MySQL 8.4 `UPDATE` and `JOIN` documentation plus
runtime behavior already covered by MyLite's SELECT join implementation. The
implementation is independently authored.

Official references:

- https://dev.mysql.com/doc/refman/8.4/en/update.html
- https://dev.mysql.com/doc/refman/8.4/en/join.html

## Syntax

MyLite Lemon grammar shape:

```lemon
update_statement ::= UPDATE joined_update_table_references
    SET update_assignment_list opt_where_clause.

joined_update_table_references ::= joined_update_explicit_reference.
joined_update_table_references ::= table_factor COMMA joined_table_reference.
joined_update_table_references ::= joined_update_table_references
    COMMA joined_table_reference.

joined_update_explicit_reference ::= table_factor inner_join_operator
    table_factor opt_inner_join_condition.
joined_update_explicit_reference ::= table_factor outer_join_operator
    table_factor outer_join_condition.
joined_update_explicit_reference ::= joined_update_explicit_reference
    inner_join_operator table_factor opt_inner_join_condition.
joined_update_explicit_reference ::= joined_update_explicit_reference
    outer_join_operator table_factor outer_join_condition.
```

The AST stores the joined source as the same `FROM_TABLE_REFERENCES` node used
by `SELECT` and multi-table `DELETE`. Assignment nodes are unchanged.

## Semantics

1. Bind joined table references with the existing SELECT join binder.
2. Bind join `ON`/`USING` predicates and optional `WHERE` with SELECT predicate
   semantics.
3. Resolve each assignment target against writable joined base tables.
4. Materialize the joined row source.
5. For each target table, update each matched physical row at most once.
6. Evaluate assignment expressions against the matched joined row.
7. Validate target values and unique indexes, then write all changed target rows
   in one statement-level transaction.

For duplicate joined matches to the same target row, MyLite updates the row once
using the first materialized joined row. This matches the practical MySQL rule
that each matching target row is updated once while avoiding a dependency on an
optimizer-specific duplicate-match order.

`LEFT`/`RIGHT` outer joins only update target rows that are present on the
target side. A null-extended non-preserved target side is skipped because there
is no physical row to update.

## Diagnostics

- Missing target columns use `1054` in `field list`.
- Ambiguous unqualified assignment targets use `1052` in `field list`.
- `ORDER BY` and `LIMIT` after joined updates are syntax errors.
- Assignment expression warnings are promoted with the same strict DML policy as
  single-table `UPDATE`.
- Duplicate-key conflicts roll back the statement.

## Deferred

- Optimizer behavior and physical join-order fidelity.
- Generated-column execution and triggers.
- Foreign-key interaction and lock ordering.
- Exact behavior for duplicate joins that assign different values to the same
  target row.

## Tests

Coverage includes:

- parser acceptance for explicit join, comma join, and outer join forms
- parser rejection for joined `ORDER BY`
- joined assignment from source table columns
- base table-name qualifiers in `JOIN ... ON` and `SET` targets, such as
  `UPDATE t1 JOIN t2 ON t1.id = t2.t1_id SET t1.a = t2.new_a`
- no-op affected-row behavior
- multi-target assignment in one statement
- left-join unmatched target updates
- ambiguous assignment diagnostics
