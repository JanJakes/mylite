# Baseline DATE_FORMAT Predicates

## Summary

This phase admits the narrow WordPress-shaped predicate:

```sql
SELECT id FROM options
WHERE DATE_FORMAT(option_value, '%H.%i') = 0.42
```

The scope is intentionally limited to the existing `DATE_FORMAT()` row-scalar
input subset, the exact `'%H.%i'` format, equality against a decimal or integer
numeric literal, and one descriptor-backed table source. It reuses the existing
row-scalar `DATE_FORMAT()` planner and MyLite's registered SQLite
`_mylite_date_format(...)` function so SQLite evaluates the predicate during
the scan. MyLite does not materialize table rows to filter this expression.

## Sources And Evidence

- MyLite architecture and existing DATE_FORMAT design:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
  - `docs/specs/baseline-date-format-function/specs.md`
  - `docs/specs/baseline-select-where-lifecycle/specs.md`
- Official MySQL 8.4 Reference Manual:
  - date and time functions:
    <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
  - comparison functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_date_format_predicates_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `DATE_FORMAT(value, '%H.%i') = 0.42` compares the formatted hour-minute text
  numerically. Values formatting to `00.42` match `0.42`; seconds are not part
  of the formatted value.
- MySQL also accepts the reversed form
  `0.42 = DATE_FORMAT(value, '%H.%i')`, but the parser admits only the
  function-left shape in this slice to keep the grammar isolated.
- Unary `+` on the numeric literal is accepted; unary `-` is accepted and
  compares normally.
- A `NULL` or invalid temporal value produces a `NULL` comparison result and
  does not match the `WHERE` predicate. Invalid temporal strings append warning
  `1292`, preserving the existing `DATE_FORMAT()` warning text.
- `DATE` values formatted with `'%H.%i'` use MySQL's date-at-midnight behavior.
- Successful in-range comparisons produce no warnings.
- MySQL accepts broader shapes such as other comparison operators, other
  formats, quoted numeric operands, expression operands, joins, grouping,
  subqueries, and expression assignment. This phase defers those shapes.

## Supported SQL

Single-table descriptor-backed `SELECT` using the existing row envelope:

```sql
SELECT select_item[, ...]
FROM table_name [AS alias]
WHERE date_format_numeric_equal
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted predicate is:

```sql
date_format_numeric_equal:
    DATE_FORMAT(value, '%H.%i') = numeric_literal

numeric_literal:
    decimal_integer_literal_with_optional_unary_sign
  | exact_decimal_literal_with_optional_unary_sign
```

`value` uses the already supported single-table row-scalar `DATE_FORMAT()`
argument subset: descriptor `DATE`, `DATETIME`, `TIMESTAMP`, and nonbinary
string-family columns, supported string literals, and `NULL`. Descriptor names
are resolved through MyLite catalog descriptors and the selected/default schema
policy, not through SQLite schema text.

## Deferred Surface

This slice intentionally does not support:

- `DATE_FORMAT()` predicates outside single-table `SELECT WHERE`;
- `<>`, `!=`, `<=>`, `<`, `<=`, `>`, `>=`, `IS`, `BETWEEN`, `IN`, `LIKE`,
  `REGEXP`, truth-only, boolean, arithmetic, control-flow, aggregate, grouping,
  having, or ordering expressions around `DATE_FORMAT()`;
- reversed predicate forms such as `0.42 = DATE_FORMAT(value, '%H.%i')`;
- format strings other than the exact `'%H.%i'` numeric comparison format;
- `TIME` descriptor inputs, week format specifiers, dynamic format arguments,
  quoted numeric operands, hex/bit/float operands, parameter markers,
  variables, column operands, subqueries, or arbitrary expressions;
- joined table sources, CTEs, `DELETE`, `UPDATE`, other DML assignments,
  defaults, generated columns, constraints, expression indexes, or arbitrary
  SQLite pass-through.

## Grammar

No new tokens are required. MyLite parses the `DATE_FORMAT()` predicate with a
comparison operator so unsupported operators can be rejected with the same
deterministic diagnostic path as other row-scalar DATE_FORMAT numeric
comparisons. Only `=` succeeds in this phase:

```lemon
date_format_numeric_predicate(A) ::=
    DATE_FORMAT LPAREN expression COMMA string_literal RPAREN predicate_comparison_operator
    numeric_literal.
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning:

1. Detect the exact top-level or parenthesized binary equality whose one side is
   `DATE_FORMAT(value, '%H.%i')` and whose other side is an integer or exact
   decimal literal with optional unary sign.
2. Reject joined source contexts for this baseline predicate slice.
3. Reuse `plan_row_scalar_date_format_numeric_equal_expression()` with
   `has_source=true` so descriptor resolution, unknown-column diagnostics, and
   generated SQL match the existing row-scalar projection behavior.
4. Generate SQLite `WHERE` SQL from the planned row-scalar expression. All
   format, numeric, and discriminator values are bound parameters.

Execution:

- The row-scalar expression returns `1`, `0`, or `NULL`; the `WHERE` predicate
  passes only `1`.
- Invalid temporal values preserve the existing `DATE_FORMAT()` warning
  behavior.
- The public result object follows existing `SELECT` conventions.
- No catalog rows, descriptor versions, descriptor caches, storage preamble, or
  SQLite schema text are mutated.

## Diagnostics

Required diagnostics:

- joined source:
  `DATE_FORMAT() numeric predicates support only one descriptor table source`;
- unsupported nonliteral or nonnumeric equality operand in this `WHERE` grammar:
  parse error near the unsupported operand;
- unsupported non-equality numeric predicate:
  `DATE_FORMAT() numeric comparison supports only DATE_FORMAT(value, format) = numeric_literal`;
- unsupported format:
  `DATE_FORMAT() numeric comparison supports only DATE_FORMAT(value, '%H.%i') = numeric_literal`;
- unsupported `DATE_FORMAT()` arguments use existing `DATE_FORMAT()`
  diagnostics;
- unknown descriptor columns use existing MySQL-compatible unknown-column
  diagnostics;
- allocation failures use existing MyLite out-of-memory diagnostics.

## Tests

Extend `packages/libmylite/tests/runtime_date_format_function_test.c` and add
a MySQL expectation script covering:

- `DATE_FORMAT(option_value, '%H.%i') = 0.42`;
- unary signed integer/decimal numeric operands;
- descriptor `VARCHAR`, `DATE`, `DATETIME`, and `TIMESTAMP` inputs;
- `NULL` and invalid string behavior, including warnings;
- `ORDER BY` / `LIMIT` envelope preservation;
- deterministic diagnostics for joined sources, unknown columns, unsupported
  formats, unsupported comparison operators, unsupported operands, and DML
  contexts.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-temporal.md`, and
`docs/compatibility/sql-query-expressions.md` to state that only this limited
single-table `WHERE` predicate form is supported. Do not claim full
`DATE_FORMAT()` predicates or general expression predicates.
