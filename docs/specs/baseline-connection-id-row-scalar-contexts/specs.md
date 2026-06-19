# Baseline CONNECTION_ID Row-Scalar Contexts

## Status

This feature completes the SQL-visible `CONNECTION_ID()` baseline inside
MyLite's current row-scalar SELECT envelope. The function is a stable
connection-local runtime value for the lifetime of a MyLite handle, and it can
now be used in source-backed projection, `WHERE` comparisons, and `ORDER BY`
expressions for row-scalar SELECT statements.

The existing AST node is admitted through a shared statement-context
row-scalar function grammar helper and routed through the existing row-scalar
session-value planner. No SQLite fork, SQLite UDF, catalog row, storage change,
or public API change is required.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline connection-id function:
  `docs/specs/baseline-connection-id-function/specs.md`
- Baseline session value scalar projection:
  `docs/specs/baseline-session-value-scalar-projection/specs.md`
- MySQL 8.4 Reference Manual, information functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against a TCP MySQL 8.4.9 session:

- `CONNECTION_ID()` returns a nonzero decimal connection identifier in normal
  sessions.
- Repeated calls in one statement and one connection return the same value.
- With a table containing rows `(1), (2)`, `SELECT CONNECTION_ID(), id FROM t
  WHERE CONNECTION_ID() = CONNECTION_ID() ORDER BY CONNECTION_ID(), id` returns
  the same connection id for each source row, ordered by `id`.
- The statement produces no warnings.
- A following `ROW_COUNT()` after the source-backed select returns `-1`.

## Scope

The implementation must add:

- source-backed row-scalar `SELECT CONNECTION_ID() FROM table` support;
- `CONNECTION_ID()` as a row-scalar predicate expression on either side of
  comparison predicates in the current row-scalar expression family;
- `CONNECTION_ID()` as a row-scalar `ORDER BY` expression;
- repeated result values for each physical source row without changing
  connection, catalog, or storage state;
- preservation of nonreserved `CONNECTION_ID` identifier behavior through
  parser fallback and bare-expression identifier parsing;
- MySQL-runtime-verified expectation coverage and fast C regression coverage.

The accepted source shapes are the existing row-scalar source envelope to the
extent those envelopes already support the surrounding SELECT shape.

## Parser And AST Handling

No new AST node kind is required. The existing zero-argument
`CONNECTION_ID()` expression node is additionally accepted by the shared
statement-context row-scalar helper:

```lemon
statement_context_row_scalar_function ::= CONNECTION_ID LPAREN RPAREN.
statement_context_row_scalar_function ::= VERSION LPAREN RPAREN.
predicate_atom ::= statement_context_row_scalar_function comparison_operator
                   predicate_comparison_value.
predicate_atom ::= statement_context_row_scalar_function comparison_operator
                   statement_context_row_scalar_function.
predicate_atom ::= predicate_scalar_literal comparison_operator
                   statement_context_row_scalar_function.
predicate_atom ::= qualified_identifier comparison_operator
                   statement_context_row_scalar_function.
predicate_atom ::= predicate_row_scalar_expression comparison_operator
                   statement_context_row_scalar_function.
select_order_key ::= statement_context_row_scalar_function.
```

`CONNECTION_ID` remains a nonreserved identifier through parser fallback in
identifier-only contexts. Bare expression `CONNECTION_ID` continues to parse as
an identifier expression so the runtime can report the existing MySQL-shaped
unknown-column diagnostic instead of treating it as a function.

## Non-Goals

This feature does not implement:

- public API additions or ABI changes;
- a MySQL server thread, socket connection, process-list row, protocol
  connection id, Performance Schema thread id, or `pseudo_thread_id`;
- `SHOW PROCESSLIST`, `INFORMATION_SCHEMA.PROCESSLIST`,
  `performance_schema.threads`, `PROCESSLIST_ID`, or kill/explain-for-
  connection behavior;
- grouping, window clauses, locking clauses, or other SELECT clauses not
  already supported by row-scalar SELECT planning;
- user variables, prepared-statement parameters, defaults, generated columns,
  check constraints, or broader DML expressions;
- a SQLite UDF or SQLite fork hook.

## Runtime Semantics

`CONNECTION_ID()` is planned as a source-independent row-scalar value. Planning
reads MyLite's handle-local connection id for the statement, creates a constant
planned value, and binds that value into the generated SQLite query. SQLite
still performs source scanning, filtering, ordering, and row production.

Successful source-backed reads return the same function label and decimal text
shape as the existing scalar connection-id baseline, with zero statement
warnings and row-result affected-row semantics.

## SQLite And Storage Handling

The implementation is MyLite wrapper/planner logic using public SQLite
prepared statement parameters. It does not require a SQLite extension function,
virtual table, VFS change, or targeted SQLite fork patch. No catalog rows or
`.mylite` file header fields are read or modified.

## Tests

Fast C tests must cover:

- the stale unsupported `SELECT CONNECTION_ID() FROM t` form now returns the
  connection id once per source row in the focused function test;
- source-backed projection combined with
  `WHERE CONNECTION_ID() = CONNECTION_ID()`;
- `ORDER BY CONNECTION_ID(), id` in a source-backed row-scalar SELECT;
- parser coverage for `CONNECTION_ID()` on both sides of a row-scalar
  comparison;
- nonreserved identifier and bare-name behavior remains unchanged.

The MySQL expectation script must verify the source-backed projection,
predicate, ordering, row values, and warning-free behavior against MySQL 8.4.9.
