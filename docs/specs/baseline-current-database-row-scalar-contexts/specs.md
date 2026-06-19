# Baseline Current Database Row-Scalar Contexts

## Status

This feature expands the existing `DATABASE()` / `SCHEMA()` selected-schema
readback slice from no-source and `DUAL` scalar selects into MyLite's
source-backed row-scalar SELECT envelope. The functions remain connection-local
session values, but they can now be projected, compared in `WHERE`, and used in
`ORDER BY` for descriptor-backed SELECTs that already use row-scalar planning.

The existing AST nodes are admitted into the predicate row-scalar grammar,
routed through the existing MyLite row-scalar planner as constant planned
values, and bound to SQLite as parameters. No SQLite fork change is required.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline current database function:
  `docs/specs/baseline-current-database-function/specs.md`
- Baseline row-scalar expressions:
  `docs/specs/baseline-row-scalar-expressions/specs.md`
- MySQL 8.4 Reference Manual, information functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against `SELECT VERSION()` returning `8.4.9`:

- `DATABASE()` returns the default database name as a string and returns `NULL`
  when no default database is selected.
- `SCHEMA()` returns the same value as `DATABASE()`.
- With a selected database and a table containing two rows, `SELECT
  DATABASE(), SCHEMA(), id FROM t WHERE DATABASE() = '<db>' ORDER BY SCHEMA(),
  id` returns one current-database pair per source row, ordered by `id`.
- Result column labels for unaliased projection items remain the source
  expression text, such as `DATABASE()` and `SCHEMA()`.
- The statement produces no warnings.

## Scope

The implementation must add:

- source-backed row-scalar `SELECT DATABASE(), SCHEMA() FROM table` support;
- `DATABASE()` and `SCHEMA()` as row-scalar predicate expressions, including
  comparisons such as `WHERE DATABASE() = 'db'`;
- `DATABASE()` and `SCHEMA()` as row-scalar `ORDER BY` expressions;
- the same tableless predicate expression when a supported `INSERT ... SELECT`
  statement uses the shared `FROM DUAL` SELECT source path;
- repeated result values for each physical source row without changing session,
  catalog, or storage state;
- MySQL-runtime-verified expectations and C regression coverage.

The accepted source shapes are the existing row-scalar source envelope:
descriptor table sources, joined descriptor sources, and derived sources to the
extent those envelopes already support the surrounding SELECT shape.

## Parser And AST Handling

No new AST node kinds are required. The existing zero-argument
`DATABASE()` / `SCHEMA()` expression nodes must also be accepted as
comparison-left row-scalar predicate expressions:

```lemon
predicate_row_scalar_expression ::= DATABASE LPAREN RPAREN.
predicate_row_scalar_expression ::= SCHEMA LPAREN RPAREN.
```

The existing predicate comparison-value grammar continues to accept the same
functions on the right side of comparisons, preserving metadata predicates such
as `TABLE_SCHEMA = DATABASE()`.

## Non-Goals

This feature does not implement:

- stored routine semantics where MySQL may bind `DATABASE()` to the routine's
  schema instead of the caller's current schema;
- arbitrary scalar-expression support outside the existing row-scalar envelope;
- grouping, window clauses, locking clauses, or other SELECT clauses not already
  supported by row-scalar SELECT planning;
- user variables, prepared-statement parameters, protocol metadata changes, or
  wire-protocol behavior;
- a SQLite UDF or SQLite fork hook.

## Runtime Semantics

`DATABASE()` and `SCHEMA()` are planned as source-independent row-scalar values.
Planning reads the connection's selected schema once for the statement, creates
a constant planned value, and binds that value into the generated SQLite query.
SQLite still performs the table scan, filtering, ordering, and row production.
MyLite does not materialize the source table or evaluate rows outside SQLite.

If no schema is selected, the planned value is SQL `NULL`. Comparisons then
follow the existing row-scalar predicate `NULL` behavior. If a schema is
selected, the planned value is text and string predicate literals are decoded
with the existing row-scalar text-literal path.

## SQLite And Storage Handling

The implementation is MyLite wrapper/planner logic using public SQLite prepared
statement parameters. It does not require a SQLite extension function, virtual
table, VFS change, or targeted SQLite fork patch. No catalog rows or `.mylite`
file header fields are read or modified.

## Tests

Fast C tests must cover:

- the prior unsupported `SELECT DATABASE(), SCHEMA() FROM t` form now returns
  the selected schema once per source row;
- source-backed projection combined with `WHERE DATABASE() = 'schema'`;
- `ORDER BY SCHEMA(), id` in a source-backed row-scalar SELECT;
- `INSERT ... SELECT ... FROM DUAL WHERE DATABASE() = 'schema'` through the
  shared tableless SELECT source path;
- the no-source and `DUAL` behavior from the original slice remains unchanged.

The MySQL expectation script must verify the table-backed projection,
predicate, ordering, labels, row values, and warning-free behavior against
MySQL 8.4.9.
