# Baseline Session Diagnostics Row-Scalar Contexts

## Scope

This slice extends the existing baseline support for session diagnostics
information functions into source-backed row-scalar `SELECT` contexts:

- `ROW_COUNT()`
- `FOUND_ROWS()`
- zero-argument `LAST_INSERT_ID()`

The supported row-scalar contexts are projection, `WHERE` predicates, and
`ORDER BY` expressions over already supported descriptor table sources. The
functions are evaluated from the statement-start session snapshot, matching the
observed MySQL 8.4.9 behavior for row-backed selects.

## Sources

- MySQL 8.4 Reference Manual, `Information Functions`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-functions.html>
- MySQL 8.4 Reference Manual, `SELECT Statement`:
  <https://dev.mysql.com/doc/refman/8.4/en/select.html>
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_row_count_function_expectations.sh`,
  `packages/libmylite/tests/mysql_baseline_found_rows_function_expectations.sh`,
  and
  `packages/libmylite/tests/mysql_baseline_last_insert_id_function_expectations.sh`.

## MySQL Behavior

`ROW_COUNT()` returns the affected-row state for the previous completed
statement. Inside a row-backed `SELECT`, it reads that previous value for every
produced row; after the `SELECT` completes, the session row-count state becomes
`-1`.

`FOUND_ROWS()` returns the previous found-row state at the start of the
statement. A row-backed `SELECT FOUND_ROWS() FROM t` repeats that snapshot value
for each produced row. The statement then updates the session found-row state to
the produced-row count, or to the `SQL_CALC_FOUND_ROWS` total for supported
`SQL_CALC_FOUND_ROWS` selects. Each statement that invokes `FOUND_ROWS()` emits
warning 1287 once for each syntactic invocation in the supported row-scalar
contexts.

Zero-argument `LAST_INSERT_ID()` returns the connection-local current insert-id
state without changing it. The value is stable for every row produced by a
row-backed `SELECT`.

## MyLite Behavior

MyLite plans these three read functions as statement-start literal values in
the row-scalar select plan. Values that fit signed 64-bit range are bound as
integer parameters so numeric predicates behave naturally. Values outside that
range, notably possible unsigned `LAST_INSERT_ID()` states, are bound as text
for display fidelity.

`FOUND_ROWS()` warning collection scans supported row-scalar projection,
`WHERE`, and `ORDER BY` contexts. This preserves the existing per-expression
warning behavior while extending it beyond the select list.

`LAST_INSERT_ID(expr)` remains outside this slice for source-backed
row-scalar evaluation because it mutates session state per evaluated row. MyLite
continues to support the existing source-free literal-argument subset and to
reject source-backed mutating forms predictably until row-evaluation side
effects have a dedicated design.

## Grammar

The row-scalar predicate grammar admits the following independently authored
Lemon-shape productions:

```lemon
predicate_comparison_value(A) ::= ROW_COUNT(T) LPAREN RPAREN(R).
predicate_comparison_value(A) ::= FOUND_ROWS(T) LPAREN RPAREN(R).
predicate_comparison_value(A) ::= LAST_INSERT_ID(T) LPAREN RPAREN(R).

predicate_row_scalar_expression(A) ::= ROW_COUNT(T) LPAREN RPAREN(R).
predicate_row_scalar_expression(A) ::= FOUND_ROWS(T) LPAREN RPAREN(R).
predicate_row_scalar_expression(A) ::= LAST_INSERT_ID(T) LPAREN RPAREN(R).
```

The general expression grammar already admits these functions for projection
and `ORDER BY` expression parsing.

## Exclusions

- source-backed `LAST_INSERT_ID(expr)` side effects;
- `FOUND_ROWS()` protocol-level metadata beyond current MyLite result metadata;
- `CLIENT_FOUND_ROWS` affected-row mode;
- stored-program, trigger, and replication semantics;
- unsigned `LAST_INSERT_ID()` numeric comparison parity above signed 64-bit
  range.
