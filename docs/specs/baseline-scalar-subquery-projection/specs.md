# Baseline Scalar Subquery Projection

## Summary

This phase adds the first scalar subquery operand slice. It is intentionally
limited to uncorrelated no-source and `FROM DUAL` scalar subqueries that return
one select item from MyLite's existing scalar projection domain:

```sql
SELECT (SELECT scalar_value)
SELECT (SELECT scalar_value FROM DUAL)
SELECT CONCAT('prefix-', (SELECT scalar_value))
```

The goal is to cover common scalar expression use such as
`CONCAT('test-', (SELECT DATABASE()))` without opening table-backed subqueries,
correlation, subquery predicates, derived tables, or DML assignment subqueries.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing scalar and row-scalar slices:
  - `docs/specs/baseline-session-value-scalar-projection/specs.md`
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
- Official MySQL 8.4 Reference Manual:
  - scalar subqueries:
    <https://dev.mysql.com/doc/refman/8.4/en/scalar-subqueries.html>
  - subquery errors:
    <https://dev.mysql.com/doc/refman/8.4/en/subquery-errors.html>
  - `SELECT` statement:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - expression syntax:
    <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_scalar_subquery_projection_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## Runtime Observations

MySQL 8.4.9 establishes the behavior used by this slice:

- A scalar subquery is a single-value operand and must be parenthesized.
- `SELECT (SELECT DATABASE())` returns the selected schema name or `NULL` when
  no schema is selected.
- `SELECT CONCAT('test-', (SELECT DATABASE()))` uses the scalar subquery value
  as a normal `CONCAT()` argument.
- A scalar subquery returning no rows produces `NULL`. This slice does not yet
  admit table-backed inner selects, so no MyLite-supported shape can currently
  produce an empty inner result set.
- A scalar subquery returning more than one column fails with
  `1241 / 21000`, message `Operand should contain 1 column(s)`.
- A scalar subquery returning more than one row fails with `1242 / 21000`,
  message `Subquery returns more than 1 row`. This is reserved for later
  table-backed scalar subqueries.
- Successful supported outer `SELECT` statements report `@@warning_count = 0`
  for this slice and make a following `ROW_COUNT()` return `-1`.
- Default result labels preserve the scalar subquery source text, for example
  `(SELECT DATABASE())`; explicit aliases override the label.

## Ownership Boundaries

- Public API: no ABI or public-header changes. `mylite_execute()` continues to
  own result handles, diagnostics, and public misuse behavior.
- Statement context: scalar subqueries are evaluated inside the outer statement.
  They must not independently update `ROW_COUNT()`, `FOUND_ROWS()`, diagnostics
  snapshots, or statement-completion state.
- Lexer/parser/AST: the parser admits `( select_statement )` as an expression
  and stores it as a scalar-subquery AST node with the inner `SELECT` as its
  child. Existing source spans provide result labels.
- Analyzer/runtime: validates the inner subquery shape before evaluation,
  rejects unsupported subquery forms deterministically, evaluates admitted
  scalar values through MyLite-owned scalar/session value code, and returns one
  scalar cell to the outer projection or row-scalar `CONCAT()` planner.
- Catalog: not involved for supported inner subqueries. No table descriptors,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are read or mutated by this slice.
- SQLite physical execution: no generated SQLite SQL is needed for supported
  no-source/`DUAL` scalar subqueries. Table-backed subqueries remain deferred.
- Result builder: appends outer result columns only. Inner subqueries do not
  create public result objects.
- Storage/VFS/file format: unchanged. Supported scalar subqueries do not touch
  the `.mylite` preamble or shifted SQLite payload.

## Supported SQL

Supported scalar subquery operands:

```sql
(SELECT scalar_subquery_value)
(SELECT scalar_subquery_value FROM DUAL)
```

Supported outer contexts:

```sql
SELECT scalar_subquery_operand [AS alias]
SELECT scalar_projection_item, scalar_subquery_operand
SELECT CONCAT(non_concat_arg, scalar_subquery_operand[, non_concat_arg ...])
```

`scalar_subquery_value` is limited to the existing warning-free scalar
projection values that MyLite already evaluates without a table:

```sql
scalar_subquery_value:
    DATABASE ( )
  | SCHEMA ( )
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | supported_system_variable
```

The implementation may reuse existing scalar projection value helpers for
integer, boolean, `NULL`, `DATABASE()`, `SCHEMA()`, and supported system
variable reads, but this slice must not admit table-backed column references,
aggregates, `WHERE`, `ORDER BY`, `LIMIT`, `DISTINCT`, locking clauses, or
general expressions inside the inner subquery.

### MyLite Lemon-Syntax Snippet

Parser admission:

```lemon
expression(A) ::= LPAREN(L) select_statement(S) RPAREN(R).
```

Analyzer/runtime acceptance:

```lemon
scalar_value(A) ::= scalar_subquery(B).
row_scalar_non_concat_expr(A) ::= scalar_subquery(B).

scalar_subquery(A) ::= LPAREN SELECT scalar_subquery_value RPAREN.
scalar_subquery(A) ::= LPAREN SELECT scalar_subquery_value FROM DUAL RPAREN.
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Semantics

Evaluation for an admitted scalar subquery:

1. Validate that the subquery contains one inner `SELECT`.
2. Validate that the inner `SELECT` has exactly one select item. If it has more
   than one select item or a wildcard, return `1241 / 21000`.
3. Reject inner `DISTINCT`, select options, `SQL_CALC_FOUND_ROWS`, locking
   clauses, table sources, `WHERE`, `GROUP BY`, `HAVING`, `ORDER BY`, and
   `LIMIT` with deterministic unsupported diagnostics.
4. Evaluate the inner scalar value in the outer statement context.
5. Return SQL `NULL` when the inner scalar value is `NULL`; otherwise return
   the same visible text that the admitted scalar projection would return.

Outer projection behavior:

- The scalar subquery can appear as a select item in no-source or `FROM DUAL`
  scalar projection lists.
- The scalar subquery can appear as a non-`CONCAT()` argument in the existing
  flat row-scalar `CONCAT()` path.
- The scalar subquery result is evaluated per outer expression evaluation.
  For the currently supported no-source/`DUAL` inner forms, this is a constant
  per statement.
- The outer successful `SELECT` result follows existing result conventions:
  one public result object, row-result metadata, `affected_rows == 0`, and
  following `ROW_COUNT() == -1`.

## Unsupported

Deferred until later feature slices:

- table-backed scalar subqueries, including zero-row-to-`NULL` and
  multi-row `1242` runtime cardinality checks;
- scalar subqueries in `UPDATE`, `INSERT`, `REPLACE`, defaults, predicates,
  `ORDER BY`, `HAVING`, aggregate arguments, `DO`, or `SET`;
- correlated subqueries;
- row subqueries, `EXISTS`, `IN (subquery)`, `ANY`, `SOME`, and `ALL`;
- derived tables and CTEs;
- subquery `LIMIT`, `ORDER BY`, grouping, locking clauses, query modifiers,
  and arbitrary inner expressions;
- expression metadata propagation beyond current public result labels/values;
- SQLite fork hooks.

## Diagnostics

Supported diagnostics:

- parser syntax errors through existing parse diagnostics;
- `1241 / 21000`, `Operand should contain 1 column(s)`, when the scalar
  subquery has more than one projected item or a wildcard projection;
- deterministic `1064 / 42000` unsupported diagnostics for table-backed
  subqueries, correlated references, unsupported inner clauses, unsupported
  inner expressions, unsupported outer contexts, nested scalar subqueries, and
  subquery predicates;
- allocation failures through existing `MYLITE_NOMEM` and diagnostics.

`1242 / 21000`, `Subquery returns more than 1 row`, is documented but deferred
because no supported inner subquery can currently return multiple rows.

## Tests

Add MySQL-runtime expectation coverage for:

- `SELECT (SELECT DATABASE())`;
- `SELECT CONCAT('test-', (SELECT DATABASE()))`;
- parenthesized scalar subquery operands;
- `FROM DUAL` inner subqueries;
- integer, signed integer, boolean, and `NULL` inner values;
- default labels and explicit aliases;
- warning count and `ROW_COUNT()` behavior after supported scalar subquery
  selects;
- `1241 / 21000` for multiple projected inner columns.

Add fast C tests under `packages/libmylite/tests/`, preferably
`runtime_scalar_subquery_projection_test.c`, registered as
`libmylite.runtime.scalar_subquery_projection`.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/sql-subqueries.md`
- `docs/compatibility/sql-query-expressions.md`
- `docs/compatibility/functions-string.md`

Do not mark table-backed scalar subqueries, DML assignment subqueries,
subquery predicates, row subqueries, correlation, derived tables, or general
expression subqueries as supported.
