# Baseline Column Predicate Values

## Status

Implemented in this slice.

## Sources

- MySQL 8.4 Reference Manual, Expressions:
  https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- MySQL 8.4 Reference Manual, Comparison Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- MySQL 8.4.9 runtime observations recorded in
  `packages/libmylite/tests/mysql_baseline_where_and_predicates_expectations.sh`.

## Scope

MySQL accepts column references as ordinary expressions in predicate value
positions. MyLite already supports descriptor-column right-hand values in
comparison predicates. This slice extends the same row-dependent value concept
to bare descriptor-column values in the existing predicate value retry envelope:

- `BETWEEN` and `NOT BETWEEN` lower and upper bounds;
- `IN` and `NOT IN` list entries;
- shared single-table `SELECT`, `UPDATE`, and `DELETE` `WHERE` paths.

The admitted value is a single uncalled descriptor identifier, optionally wrapped
in redundant parentheses. Function calls, arithmetic, and other direct
row-scalar roots remain covered by the existing row-scalar predicate value slice.
Source-free constants are not rerouted through this table-backed retry path.

## Semantics

Column predicate values are planned through the existing row-scalar expression
planner and rendered into generated SQLite predicate SQL. SQLite evaluates the
row filter while MyLite keeps descriptor validation and MySQL-shaped diagnostics
in the row-scalar/predicate planning layer.

Examples covered by this slice:

```sql
WHERE i BETWEEN -2 AND nn
WHERE i BETWEEN nn AND 2147483647
WHERE i IN (nn, 0)
WHERE i NOT IN (nn, 0)
```

This slice intentionally does not widen predicate subjects, joined predicates,
grouped predicates, metadata filters, scalar-only expressions, or arbitrary
expression composition.

## Parser Strategy

The primary grammar remains narrow. When parsing fails for a `SELECT`, `UPDATE`,
or `DELETE` statement in a `WHERE` predicate context, the existing placeholder
retry scanner may replace an admitted predicate value span with an integer
placeholder, parse the statement, parse the original value as a `DO <expr>`
expression, and splice the cloned AST into the predicate.

This slice adds bare descriptor identifiers to the direct row-scalar value shape
recognized by that retry scanner. The retry still requires a row operand, so
literal-only bounds and list entries remain on the normal predicate value paths.

## MyLite Lemon-Syntax Snippet

The implemented retry path is equivalent to adding the following value form to
the current supported predicate contexts:

```lemon
predicate_range_value ::= descriptor_column_value.
predicate_in_value ::= descriptor_column_value.

descriptor_column_value ::= identifier.
descriptor_column_value ::= LP descriptor_column_value RP.
```

This is not a full `predicate_value ::= expression` rule. Unsupported expression
roots continue to fail at parse time or return existing targeted runtime
diagnostics.

## Deferred Work

- Joined `ON` predicates and grouped `HAVING` predicate values.
- Row constructor predicates.
- Metadata predicates and `SHOW` filters.
- Column-only values inside arbitrary expression composition.
- Full expression metadata for generated predicate value expressions.
