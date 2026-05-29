# Baseline SHOW DATABASES WHERE

## Summary

MyLite extends its schema catalog listing surface with the MySQL-compatible
`WHERE` form of `SHOW DATABASES` and `SHOW SCHEMAS`.

This slice covers descriptor-owned user schemas and the existing synthetic
built-in schemas:

- `information_schema`
- `mysql`
- `performance_schema`
- `sys`

The feature filters the displayed one-column result by the displayed column
name, `Database`. It does not add privilege filtering, `--skip-show-database`
behavior, NUL-producing pattern escapes, arbitrary expression evaluation,
numeric warning-producing coercions, `ORDER BY`, or `LIMIT`.

## Compatibility Authority

The supported surface is based on:

- MySQL 8.4 Reference Manual, `SHOW DATABASES` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/show-databases.html>
- MySQL 8.4 Reference Manual, extensions to `SHOW` statements:
  <https://dev.mysql.com/doc/refman/8.4/en/extended-show.html>
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_show_databases_where_expectations.sh`.

Observed MySQL 8.4.9 behavior for this slice:

- `SHOW DATABASES WHERE \`Database\` = 'mysql'` returns one `mysql` row with
  column label `Database`.
- `SHOW SCHEMAS WHERE \`Database\` = 'mysql'` is equivalent to the
  `SHOW DATABASES` form.
- The displayed column reference is case-insensitive when quoted as
  `Database`, `database`, or `DATABASE`.
- The unquoted token sequence `WHERE Database = 'mysql'` is a syntax error
  because `DATABASE` is parsed as a keyword rather than an output column
  identifier.
- Database-name comparisons, `LIKE`, `REGEXP`, and `RLIKE` are case-sensitive.
- `WHERE` predicates are evaluated after the schema list is built and sorted.
- `LIKE ... WHERE`, trailing `ORDER BY`, and trailing `LIMIT` are syntax
  errors.
- Unknown output columns, including qualified references such as
  `schemas.Database`, return `1054 / 42S22`.
- MySQL accepts numeric literals in these predicates and reports conversion
  warnings. MyLite intentionally leaves that warning-producing coercion outside
  this baseline slice.

## Syntax

The parser admits either a `LIKE` filter or a `WHERE` predicate after
`DATABASES` / `SCHEMAS`, matching the existing MyLite `SHOW` filter pattern:

```lemon
show_databases_statement ::= SHOW DATABASES show_databases_filter_opt.
show_databases_statement ::= SHOW SCHEMAS show_databases_filter_opt.

show_databases_filter_opt ::= .
show_databases_filter_opt ::= LIKE STRING.
show_databases_filter_opt ::= WHERE predicate.
```

`SHOW DATABASES LIKE 'm%' WHERE ...`, `SHOW DATABASES WHERE ... ORDER BY ...`,
and `SHOW DATABASES WHERE ... LIMIT ...` remain syntax errors.

## Semantics

`SHOW DATABASES` and `SHOW SCHEMAS` continue to return the same one-column
schema-name result. Without a filter, the result includes the four built-in
schema names plus descriptor-owned user schemas sorted by MyLite's existing
case-insensitive ASCII ordering. With a `LIKE` filter, existing behavior and
column-label rendering are unchanged.

With a `WHERE` predicate, MyLite evaluates the predicate over the displayed
output column:

- `Database` is the only supported output column.
- Column-name resolution is ASCII case-insensitive.
- Qualified output-column references are rejected as unknown columns.
- String comparisons and string `IN` list membership are case-sensitive for
  database names.
- `LIKE` patterns are matched with the current MyLite `SHOW LIKE` pattern
  engine in case-sensitive mode.
- `REGEXP` and `RLIKE` use MyLite's existing baseline ASCII regular expression
  subset in case-sensitive mode.
- `IS NULL`, `IS NOT NULL`, `NULL` in comparisons, and `NULL` in `IN` lists use
  MySQL-shaped three-valued filtering.
- `NOT`, `AND`, `OR`, and parentheses are supported through the existing
  predicate AST evaluation shape.

The baseline supports only string and `NULL` literal predicate values. Numeric
literals, functions, arithmetic expressions, subqueries, parameters, explicit
`ESCAPE`, and other non-output-column expressions return MyLite's current
unsupported-expression diagnostics instead of attempting MySQL's warning-
producing coercions.

`WHERE` result column metadata is the same as unfiltered `SHOW DATABASES`:
one text column named `Database`. `SHOW SCHEMAS` uses the same label.

## Diagnostics

Supported diagnostics:

- unknown output column: `1054 / 42S22`, `Unknown column '<name>' in 'where clause'`.
- `LIKE ... WHERE`, `ORDER BY`, and `LIMIT` forms: syntax error.
- invalid regular expression in the baseline regex subset: existing MyLite
  regex diagnostic.
- non-string and non-`NULL` literal predicate values: unsupported expression
  diagnostics for this `SHOW DATABASES WHERE` surface.
- allocation failure: existing `MYLITE_NOMEM` / diagnostics policy.

## Architecture

- Parser/AST: `SHOW DATABASES` and `SHOW SCHEMAS` gain the same single filter
  child shape used by other `SHOW` statements: either a string literal `LIKE`
  pattern or a `WHERE` clause.
- Runtime: `execute_show_databases_statement()` resolves the filter child,
  builds the existing schema-name list, and evaluates the `WHERE` predicate
  while appending sorted rows.
- Catalog metadata: unchanged. Built-in schemas remain synthetic descriptors;
  user schemas remain catalog descriptors.
- Storage/VFS/SQLite: unchanged. No SQLite object or fork hook is needed.

## Tests

MySQL 8.4.9 expectations cover:

- headers for `SHOW DATABASES WHERE` and `SHOW SCHEMAS WHERE`;
- `Database` column resolution with different quoted identifier casing;
- case-sensitive equality, `LIKE`, `REGEXP`, and `IN`;
- `NULL` and three-valued `NOT IN` behavior;
- warning and row-count status after no-match predicates;
- unknown column and qualified-column diagnostics;
- syntax errors for `LIKE ... WHERE`, `ORDER BY`, `LIMIT`, and unquoted
  `Database`.

MyLite C coverage covers:

- `WHERE` filters over built-in and user schemas;
- `SHOW SCHEMAS` synonym behavior;
- case-sensitive comparisons and patterns;
- boolean, `IN`, `NULL`, and regex predicates;
- MySQL-shaped status counters after no-match filters;
- diagnostics for unknown columns, qualified columns, numeric literals, and
  unsupported trailing clauses.
