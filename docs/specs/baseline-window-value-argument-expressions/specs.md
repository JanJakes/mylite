# Baseline window value argument expressions

## Status

Implemented.

## Compatibility Source

- Official MySQL 8.4 Reference Manual, "Window Function Descriptions":
  `https://dev.mysql.com/doc/refman/8.4/en/window-function-descriptions.html`.
- MySQL 8.4.9 runtime probes in
  `packages/libmylite/tests/mysql_baseline_window_rank_navigation_expectations.sh`.

The MySQL manual describes `LAG(expr [, N [, default]])` and
`LEAD(expr [, N [, default]])` with a normal value expression for `expr` and
`default`, while the row offset `N` is restricted to a nonnegative integer
literal, positional parameter, user variable, or local stored-routine variable.
Runtime probes confirmed that descriptor-column defaults, `CONCAT()` defaults,
and arithmetic defaults are accepted, descriptor-column offsets are rejected as
undeclared variables, and arbitrary offset expressions are syntax errors.

## Scope

This slice expands the existing projection-only baseline window function
envelope:

```sql
window_value_expr ::= LAG LPAREN value_expr RPAREN OVER LPAREN window_spec_opt RPAREN
window_value_expr ::= LAG LPAREN value_expr COMMA literal_integer RPAREN OVER LPAREN window_spec_opt RPAREN
window_value_expr ::= LAG LPAREN value_expr COMMA literal_integer COMMA value_expr RPAREN OVER LPAREN window_spec_opt RPAREN
window_value_expr ::= LEAD LPAREN value_expr RPAREN OVER LPAREN window_spec_opt RPAREN
window_value_expr ::= LEAD LPAREN value_expr COMMA literal_integer RPAREN OVER LPAREN window_spec_opt RPAREN
window_value_expr ::= LEAD LPAREN value_expr COMMA literal_integer COMMA value_expr RPAREN OVER LPAREN window_spec_opt RPAREN
window_value_expr ::= FIRST_VALUE LPAREN value_expr RPAREN OVER LPAREN window_spec_opt RPAREN
window_value_expr ::= LAST_VALUE LPAREN value_expr RPAREN OVER LPAREN window_spec_opt RPAREN
window_value_expr ::= NTH_VALUE LPAREN value_expr COMMA literal_integer RPAREN OVER LPAREN window_spec_opt RPAREN
```

`value_expr` uses MyLite's current row-scalar nested value-expression envelope:
descriptor columns, scalar literals, supported row arithmetic, string,
numeric, temporal, JSON, binary-string, conversion, control-flow, collation,
session-value, UUID, and related registered scalar functions. Nested window
functions and aggregate functions remain unsupported.

`literal_integer` remains the existing literal-integer path. User-variable,
parameter-marker, and stored-routine-local offset forms are MySQL-compatible
syntax but outside this baseline slice.

## Semantics

MyLite lowers admitted value/default expressions into SQLite SQL using the
same row-scalar planner, SQL builder, and parameter binder used by ordinary
single-table row-scalar projections. This avoids materializing rows in MyLite
and leaves window execution to SQLite's native window functions.

`LAG()` and `LEAD()` still ignore admitted frame clauses, matching observed
MySQL behavior. `FIRST_VALUE()`, `LAST_VALUE()`, and `NTH_VALUE()` use the
supported frame subset from the value-frame slice.

## Errors

- `LAG()` / `LEAD()` offsets must remain nonnegative integer literals in this
  baseline. Other descriptor-column offset forms are rejected before SQLite
  execution.
- `NTH_VALUE()` and `NTILE()` index/bucket arguments keep their existing
  positive-literal validation.
- Unsupported nested row-scalar expressions return MyLite's existing
  row-scalar unsupported-expression diagnostics.

## Tests

The runtime test covers:

- value expressions in `LAG()`, `LEAD()`, `FIRST_VALUE()`, and `NTH_VALUE()`;
- descriptor-column and function-expression defaults for `LAG()`;
- arithmetic defaults for `LEAD()`;
- continued rejection of nonliteral offset columns.

The MySQL expectation script records matching MySQL 8.4.9 outputs and errors.
