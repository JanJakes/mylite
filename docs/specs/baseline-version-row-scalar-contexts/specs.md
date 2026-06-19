# Baseline VERSION Row-Scalar Contexts

## Status

This feature completes the SQL-visible `VERSION()` baseline inside MyLite's
current row-scalar SELECT envelope. The function is a statement-independent
runtime constant that reports the MySQL 8.4.9 compatibility version, and it can
now be used in source-backed projection, `WHERE` comparisons, and `ORDER BY`
expressions for row-scalar SELECT statements.

The existing AST node is admitted into row-scalar predicate grammar and routed
through the existing row-scalar session-value planner as a bound constant. No
SQLite fork, SQLite UDF, catalog row, or storage change is required.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline version function:
  `docs/specs/baseline-version-function/specs.md`
- Baseline MySQL server version identity:
  `docs/specs/baseline-mysql-server-version-identity/specs.md`
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

Observed against `SELECT VERSION()` returning `8.4.9`:

- `VERSION()` returns the server version string.
- With a table containing rows `(2), (1)`, `SELECT VERSION(), id FROM t WHERE
  VERSION() = VERSION() ORDER BY VERSION(), id` returns one version string per
  source row, ordered by `id`.
- The unaliased result label for the function is `VERSION()`.
- The statement produces no warnings.
- A following `ROW_COUNT()` after the source-backed select returns `-1`.

## Scope

The implementation must add:

- source-backed row-scalar `SELECT VERSION() FROM table` support where not
  already accepted by newer row-scalar planning;
- `VERSION()` as a row-scalar predicate expression on either side of comparison
  predicates;
- `VERSION()` as a row-scalar `ORDER BY` expression;
- repeated result values for each physical source row without changing session,
  catalog, or storage state;
- MySQL-runtime-verified expectation coverage and fast C regression coverage.

The accepted source shapes are the existing row-scalar source envelope:
descriptor table sources, joined descriptor sources, and derived sources to the
extent those envelopes already support the surrounding SELECT shape.

## Parser And AST Handling

No new AST node kind is required. The existing zero-argument `VERSION()`
expression node is additionally accepted in dedicated row-scalar comparison and
ordering contexts:

```lemon
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

The existing `VERSION(...)` argument-count-error grammar continues to produce
the native-function parameter-count diagnostic when such calls are parsed as
ordinary expressions.

`VERSION` remains a nonreserved identifier through parser fallback in
identifier-only contexts. Bare expression `VERSION` continues to parse as an
identifier expression so the runtime can report the existing MySQL-shaped
unknown-column diagnostic instead of treating it as a function.

## Non-Goals

This feature does not implement:

- protocol handshake version reporting;
- configurable server-version identity;
- build or compile metadata parity beyond existing fixed system-variable rows;
- `VERSION()` as a stored-program local semantic surface;
- grouping, window clauses, locking clauses, or other SELECT clauses not
  already supported by row-scalar SELECT planning;
- user variables, prepared-statement parameters, defaults, generated columns,
  check constraints, or broader DML expressions;
- a SQLite UDF or SQLite fork hook.

## Runtime Semantics

`VERSION()` is planned as a source-independent row-scalar value. Planning reads
MyLite's SQL-visible MySQL compatibility version constant once for the
statement, creates a constant planned value, and binds that value into the
generated SQLite query. SQLite still performs source scanning, filtering,
ordering, and row production.

Successful source-backed reads return the same function label and value shape
as the existing scalar version-function baseline, with zero statement warnings
and row-result affected-row semantics.

## SQLite And Storage Handling

The implementation is MyLite wrapper/planner logic using public SQLite
prepared statement parameters. It does not require a SQLite extension function,
virtual table, VFS change, or targeted SQLite fork patch. No catalog rows or
`.mylite` file header fields are read or modified.

## Tests

Fast C tests must cover:

- the stale unsupported `SELECT VERSION() FROM t` form now returns the
  compatibility version once per source row;
- source-backed projection combined with `WHERE VERSION() = VERSION()`;
- `ORDER BY VERSION(), id` in a source-backed row-scalar SELECT;
- parser coverage for `VERSION()` on both sides of a row-scalar comparison;
- the original no-source and `DUAL` behavior remains unchanged.

The MySQL expectation script must verify the source-backed projection,
predicate, ordering, labels, row values, and warning-free behavior against
MySQL 8.4.9.
