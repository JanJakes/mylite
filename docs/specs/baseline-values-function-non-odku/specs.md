# Baseline VALUES() Function Outside ODKU

## Purpose

This slice adds MySQL-compatible handling for deprecated `VALUES(column_name)`
when it is used as a row-scalar function outside an
`INSERT ... ON DUPLICATE KEY UPDATE` assignment. MySQL still accepts the syntax
in table-backed `SELECT` projections, but the value is not meaningful outside
the duplicate-key update context.

Compatibility source:

- Official MySQL 8.4 Reference Manual, "INSERT ... ON DUPLICATE KEY UPDATE
  Statement": `https://dev.mysql.com/doc/refman/8.4/en/insert-on-duplicate.html`.
- Runtime probes against MySQL 8.4.9, captured in
  `packages/libmylite/tests/mysql_baseline_values_function_non_odku_expectations.sh`.

## Supported Syntax

The supported MyLite grammar is deliberately narrow and independently authored:

```lemon
row_scalar_expression(A) ::= VALUES(T) LPAREN qualified_identifier(C) RPAREN(R).
```

`qualified_identifier` may be an unqualified column name, `table.column`, or
`schema.table.column` that resolves against the current single table-backed
row-scalar `SELECT` source.

The following are syntax errors in this slice, matching MySQL 8.4.9 behavior:

- `VALUES()`;
- `VALUES(column, other_column)`;
- `VALUES(1)`;
- `VALUES(expr)`.

`SELECT VALUES(column)` without a table source and unknown column references
return MySQL-shaped unknown-column diagnostics.

## Semantics

Outside `INSERT ... ON DUPLICATE KEY UPDATE`, `VALUES(column)` returns SQL
`NULL` for every selected row. The referenced column is resolved only to prove
that MySQL would accept the reference; the source column value is not read.

Each `VALUES(column)` occurrence in a successful top-level table-backed
`SELECT` increments the statement warning count once, not once per output row.
The warning is count-only in MySQL 8.4.9:

- `mylite_result_warning_count()` and later `@@warning_count` include it;
- `SHOW WARNINGS` returns no warning rows for it;
- `SHOW COUNT(*) WARNINGS` returns `0`.

`SELECT VALUES(column), @@warning_count FROM t` observes the current
statement's count-only warning through `@@warning_count`, matching MySQL's
runtime behavior.

## Metadata

MySQL 8.4.9 reports non-ODKU `VALUES(column)` result metadata like a nullable
binary string with display length zero. MyLite exposes the same public result
shape for this slice:

- logical/public result type: `VAR_STRING`;
- charset and collation: binary (`63`);
- display length: `0`;
- flags: `BINARY`;
- nullable: true.

MySQL `CREATE TABLE ... AS SELECT VALUES(column)` creates `varbinary(0)`
columns and reports no warnings. MyLite's existing CTAS expression projection
surface remains outside this slice; result metadata is verified directly
through the public result API.

## Implementation

The parser already represents `VALUES(...)` as a generic system function. The
row-scalar planner intercepts generic `VALUES` calls after the existing ODKU
special case. Valid descriptor column references plan to a dedicated
`PLANNED_ROW_SCALAR_EXPRESSION_VALUES_OUTSIDE_ODKU` node that renders to SQLite
as `NULL`.

The diagnostics layer carries a count-only warning counter so the public result
warning count and `@@warning_count` can include this MySQL quirk without
inventing `SHOW WARNINGS` rows.

No storage format, SQLite fork, public ABI, or SQLite schema changes are
required.

## Errors

- Unknown or missing source column: `1054 / 42S22`.
- Wrong arity or non-column argument: `1064 / 42000`.
- Unsupported statement envelopes continue to use their existing MyLite
  diagnostics rather than silently broadening expression execution.

## Out Of Scope

- General expression use outside supported row-scalar `SELECT` projections.
- Executable CTAS expression inference for `VALUES(column)`.
- Additional ODKU behavior, already covered by
  `docs/specs/baseline-odku-values-cross-column/specs.md`.
- Removal or replacement of the deprecated MySQL function with row aliases.
