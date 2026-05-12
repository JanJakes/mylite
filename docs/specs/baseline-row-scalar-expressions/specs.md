# Baseline Row Scalar Expressions

## Summary

This phase adds the first descriptor-backed row-context scalar expression
projection path. It deliberately starts with `CONCAT()` because it is common in
application SQL and in the project MySQL expectation probes, while also forcing
the runtime to plan expressions over the current row instead of only handling
bare descriptor columns or no-source scalar functions.

Supported user-visible shapes, when the select list contains at least one
top-level or parenthesized `CONCAT()` expression:

```sql
SELECT scalar_item[, scalar_item ...]
SELECT scalar_item[, scalar_item ...] FROM DUAL
SELECT scalar_item[, scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

For select lists routed through this row-scalar path, the admitted scalar
expression subset is intentionally small:

- descriptor column references in table-backed statements, either as
  non-`CONCAT()` companion select items or as `CONCAT()` arguments;
- string, decimal-integer, boolean, and `NULL` literals;
- existing session scalar functions and system variables that already return a
  MyLite-owned scalar text value or SQL `NULL`, such as `DATABASE()` and
  `@@warning_count`;
- parenthesized admitted expressions; and
- `CONCAT(expr[, expr ...])` over the same admitted non-`CONCAT()` expression
  domain.

This is not a general expression engine. It does not add scalar subqueries,
table-backed arithmetic, general table-backed flow-control functions, casts,
date functions, expression defaults, expression predicates, expression ordering,
expression DML assignments, joins, CTEs, or arbitrary SQLite pass-through.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing scalar and DML expression slices:
  - `docs/specs/baseline-scalar-expression-projection/specs.md`
  - `docs/specs/baseline-select-where-lifecycle/specs.md`
  - `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
  - `docs/specs/baseline-update-lifecycle/specs.md`
  - `docs/specs/baseline-varchar-type/specs.md`
  - `docs/specs/baseline-string-defaults/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `SELECT` statement:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - expression syntax:
    <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
  - string functions and operators, including `CONCAT()`:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
  - type conversion in expression evaluation:
    <https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_row_scalar_expressions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## Runtime Observations

MySQL 8.4.9 probes establish the behavior used by this phase:

- `CONCAT()` accepts one or more arguments and fails with `1582 / 42000` for
  zero arguments.
- `CONCAT()` returns `NULL` if any argument is `NULL`.
- `CONCAT()` converts integer and boolean arguments to their decimal text
  forms; `TRUE` contributes `1` and `FALSE` contributes `0`.
- `CONCAT()` over string, integer, exact decimal, date, and `TEXT` column
  values uses the value's visible text form.
- `SELECT CONCAT(...)` without a source and `SELECT CONCAT(...) FROM DUAL`
  return one row.
- Table-backed `SELECT id, CONCAT(v, '-', id) FROM t ORDER BY id` evaluates
  once per matched source row and preserves the existing `WHERE`, `ORDER BY`,
  and `LIMIT` row envelope.
- Default result labels use the expression source text, while explicit aliases
  override the label.
- Successful supported `SELECT` statements report `@@warning_count = 0` for
  this slice and make a following `ROW_COUNT()` return `-1`.
- Unknown column references inside `CONCAT()` fail with MySQL's unknown-column
  diagnostic in field-list context.

## Ownership Boundaries

- Public API: no public ABI changes. `mylite_execute()` continues to own result
  handles, diagnostics, and public misuse behavior.
- Statement context: successful row-scalar `SELECT` statements use existing
  row-returning `SELECT` conventions: one result object, zero affected rows,
  statement warnings, and previous row-count state `-1`.
- Lexer/parser/AST: syntax admission and source spans remain parser-owned.
  This phase adds `CONCAT()` AST nodes and a zero-argument count-error node.
- Analyzer/planner: resolves descriptor columns through MyLite catalog
  descriptors, rejects unsupported expression shapes, converts literal/session
  values to bound parameters, and builds a row-scalar projection plan.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite physical execution: MyLite lowers supported expressions to SQLite
  expressions over stable physical table names and quoted physical column
  names. MyLite-owned string literals and session values are bound parameters;
  they are not interpolated into generated SQL.
- Result builder: appends labels from aliases, descriptor names, or expression
  source spans, then reads SQLite result values through the existing text-row
  result API.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
SELECT row_scalar_item[, row_scalar_item ...] FROM DUAL
```

Descriptor-backed table forms, with at least one `row_scalar_item` containing
`CONCAT()`:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

Each item may use the existing alias surface:

```sql
row_scalar_item:
    row_scalar_expr
  | row_scalar_expr AS alias
  | row_scalar_expr alias
```

The admitted expression subset is:

```sql
row_scalar_expr:
    row_scalar_non_concat_expr
  | ( row_scalar_expr )
  | CONCAT ( row_scalar_non_concat_expr_list )

row_scalar_non_concat_expr:
    descriptor_column_reference
  | string_literal
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | session_scalar_function
  | ( row_scalar_non_concat_expr )

row_scalar_non_concat_expr_list:
    row_scalar_non_concat_expr
  | row_scalar_non_concat_expr_list , row_scalar_non_concat_expr
```

`descriptor_column_reference` may be unqualified, table-qualified,
schema-qualified, or source-alias-qualified according to the existing
single-source table alias policy for `SELECT`. No-source and `DUAL` forms must
not contain descriptor column references.

`session_scalar_value` is limited to existing session scalar functions and
system variables that already return a MyLite-owned scalar text value or SQL
`NULL`, initially including `DATABASE()`, `SCHEMA()`, and the currently
supported `@@...` system-variable reads. Existing scalar-read warnings are
preserved at statement level. Warning-producing numeric functions stay on the
existing no-source scalar path until expression metadata and warning propagation
are generalized.

### MyLite Lemon-Syntax Snippet

The parser adds independently authored productions for `CONCAT()`:

```lemon
expression(A) ::= CONCAT(T) LPAREN function_argument_list(B) RPAREN(R).
expression(A) ::= CONCAT(T) LPAREN RPAREN(R).
```

The analyzer/runtime acceptance grammar for this phase is:

```lemon
row_scalar_expr(A) ::= row_scalar_non_concat_expr(B).
row_scalar_expr(A) ::= CONCAT(T) LPAREN row_scalar_non_concat_expr_list(B) RPAREN(R).
row_scalar_non_concat_expr(A) ::= qualified_identifier(B).
row_scalar_non_concat_expr(A) ::= STRING(T).
row_scalar_non_concat_expr(A) ::= INTEGER(T).
row_scalar_non_concat_expr(A) ::= PLUS(P) INTEGER(T).
row_scalar_non_concat_expr(A) ::= MINUS(M) INTEGER(T).
row_scalar_non_concat_expr(A) ::= TRUE(T).
row_scalar_non_concat_expr(A) ::= FALSE(T).
row_scalar_non_concat_expr(A) ::= NULL(T).
row_scalar_non_concat_expr(A) ::= DATABASE(T) LPAREN RPAREN(R).
row_scalar_non_concat_expr(A) ::= SCHEMA(T) LPAREN RPAREN(R).
row_scalar_non_concat_expr(A) ::= system_variable_reference(T).
row_scalar_non_concat_expr(A) ::= LPAREN row_scalar_non_concat_expr(B) RPAREN(R).
row_scalar_non_concat_expr_list(A) ::= row_scalar_non_concat_expr(B).
row_scalar_non_concat_expr_list(A) ::= row_scalar_non_concat_expr_list(B)
                                      COMMA row_scalar_non_concat_expr(C).
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.
The parser can build nested `CONCAT()` ASTs, but this phase's runtime rejects
nested `CONCAT()` deterministically until general expression planning replaces
the deliberately flat argument planner.

Non-`CONCAT()` expressions in this grammar are admitted by this phase only as
arguments to `CONCAT()` or as companion select-list items in a `SELECT` that
also contains `CONCAT()`. Existing non-row-scalar planners continue to own
standalone literal, session-scalar, wildcard, descriptor-column, aggregate, and
distinct projection forms that do not contain `CONCAT()`.

## Semantics

Planning proceeds as follows:

1. Detect row-scalar projection attempts when a supported `SELECT` list contains
   `CONCAT()`.
2. Resolve a table source through the existing selected/default schema policy
   when a `FROM table` source exists.
3. Load MyLite column descriptors and resolve descriptor column references
   against those descriptors, not SQLite schema text.
4. Convert admitted literal and session scalar arguments into bound
   `planned_value` parameters.
5. Generate SQLite projection SQL from the expression tree and bind all values
   before execution.
6. Reuse existing descriptor-driven `WHERE`, `ORDER BY`, and `LIMIT` planning
   for the table row envelope.

`CONCAT()` semantics:

- one argument returns that argument converted to text, or `NULL` if the
  argument is `NULL`;
- two or more arguments concatenate from left to right;
- any `NULL` argument makes the result `NULL`;
- string literals and string descriptor values contribute their UTF-8 text;
- integer literals, boolean literals, and integer descriptor values contribute
  canonical decimal text;
- exact decimal and temporal descriptor values contribute their stored
  canonical visible text;
- approximate numeric descriptor columns are deferred because SQLite's default
  text conversion does not by itself establish MySQL-compatible formatting.

No-source and `DUAL` expressions are evaluated by SQLite over bound parameters
and session values without touching catalog descriptors or physical tables.
Table-backed expressions are evaluated by SQLite while scanning the physical
table, preserving streaming behavior. MyLite must not read all rows into memory
to evaluate this slice.

## SQLite Lowering

Supported `CONCAT()` lowers to SQLite concatenation over MyLite-planned
subexpressions:

```sql
SELECT (?1 || "column_name" || ?2) FROM "_mylite_user_table_N"
```

For a one-argument `CONCAT(arg)`, MyLite emits a text-forcing equivalent such
as:

```sql
('' || arg)
```

SQLite `||` returns `NULL` if either side is `NULL`, matching the supported
`CONCAT()` null-propagation rule. Generated SQL still quotes every physical
identifier and binds MyLite-owned literal/session values through prepared
statement parameters.

## Diagnostics

Supported diagnostics include:

- parser syntax errors for malformed SQL using existing parse diagnostics;
- `1582 / 42000` for `CONCAT()` with zero arguments;
- missing default schema, unknown schema, unknown table, and reserved
  `_mylite_*` table names using existing table-resolution diagnostics;
- unknown descriptor columns using MySQL-compatible unknown-column diagnostics
  in field-list context;
- deterministic unsupported diagnostics for unsupported scalar expressions,
  unsupported descriptor column types, unsupported source clauses, `DISTINCT`,
  `SQL_CALC_FOUND_ROWS`, joins, subqueries, casts, date functions, expression
  defaults, and DML expression assignments;
- allocation failures through existing `MYLITE_NOMEM` and diagnostics;
- physical SQLite failures through existing physical row diagnostics.

## Tests

Add MySQL-runtime expectation coverage for:

- no-source and `DUAL` `CONCAT()` over string, integer, boolean, `NULL`, and
  `DATABASE()` / `SCHEMA()` values;
- table-backed mixed projection such as `SELECT id, CONCAT(v, '-', i)`;
- `NULL` propagation from literals and nullable columns;
- one-argument `CONCAT()`;
- exact decimal, `DATE`, `TIME`, `DATETIME`, `TIMESTAMP`, and `TEXT` column
  visible text contribution;
- `WHERE`, `ORDER BY`, and `LIMIT` interaction through the existing table row
  envelope;
- default labels and explicit aliases;
- zero-argument `CONCAT()` diagnostics;
- unknown columns inside `CONCAT()`;
- deterministic MyLite rejections for scalar subqueries, `CAST()`,
  `DATE_ADD()`, `DATE_FORMAT()`, expression ordering, and update expression
  assignments until later phases own those semantics.

Add fast C tests under `packages/libmylite/tests/`, preferably
`runtime_row_scalar_expressions_test.c`, registered as
`libmylite.runtime.row_scalar_expressions`.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/sql-query-expressions.md`
- `docs/compatibility/functions-string.md`

Do not mark general expressions, `CONCAT_WS()`, `FIELD()`, `CAST()`, temporal
functions, scalar subqueries, expression defaults, or DML expression
assignments as supported.
