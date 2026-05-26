# Baseline GET_FORMAT Function

## Goal

Add a narrow `GET_FORMAT()` scalar-format helper for existing temporal
formatting paths:

```sql
SELECT DATE_FORMAT(created_at, GET_FORMAT(DATETIME, 'ISO')) FROM posts;
```

This feature is a constant expression slice. It does not add general temporal
expression planning, locale handling, row-backed dynamic format expressions, or
new temporal storage semantics.

## Sources

- Official MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Official MySQL 8.4 Reference Manual, function-name parsing and resolution:
  <https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html>
- Existing temporal format designs:
  - `docs/specs/baseline-date-format-function/specs.md`
  - `docs/specs/baseline-time-format-function/specs.md`
  - `docs/specs/baseline-str-to-date-function/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_get_format_function_expectations.sh`.

The MyLite grammar and implementation are independently authored from official
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. Do not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `GET_FORMAT(DATE, format)` returns a date format string for recognized
  format names.
- `GET_FORMAT(TIME, format)` returns a time format string for recognized
  format names.
- `GET_FORMAT(DATETIME, format)` and `GET_FORMAT(TIMESTAMP, format)` return the
  same datetime format strings.
- The first argument is a keyword-like temporal class, not a string value:
  `GET_FORMAT('DATE', 'USA')` is a syntax error.
- The temporal class and recognized format names are case-insensitive.
- Recognized format names are `USA`, `JIS`, `ISO`, `EUR`, and `INTERNAL`.
- Unknown, numeric, or `NULL` format arguments return `NULL` without warnings.
- MySQL accepts expression-valued second arguments such as
  `GET_FORMAT(DATE, CONCAT('U','SA'))`. MyLite defers expression arguments in
  this baseline to keep the slice constant and deterministic.
- `GET_FORMAT()` with wrong arity is a syntax error in MySQL 8.4.9, not a
  native-function parameter-count diagnostic.
- Successful supported statements record no warnings. A successful `SELECT`
  makes a following `ROW_COUNT()` return `-1`; a successful `DO` makes it return
  `0`.
- `GET_FORMAT` is not a whitespace-sensitive built-in function name. Unquoted
  `get_format` remains usable as a table name in nonexpression contexts.

## Supported Surface

MyLite supports:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` using the existing row envelope, where
  `GET_FORMAT()` is a constant projection repeated for each output row;
- `GET_FORMAT(temporal_class, format_value)` with exactly two arguments;
- `temporal_class` as one of `DATE`, `TIME`, `DATETIME`, or `TIMESTAMP`,
  case-insensitive in SQL text;
- `format_value` as:
  - a single- or double-quoted string literal, decoded by the active SQL mode;
  - a decimal integer literal, including optional unary sign;
  - `TRUE` / `FALSE`;
  - `NULL`;
- return values:

  | Class | USA | JIS | ISO | EUR | INTERNAL |
  | --- | --- | --- | --- | --- | --- |
  | `DATE` | `%m.%d.%Y` | `%Y-%m-%d` | `%Y-%m-%d` | `%d.%m.%Y` | `%Y%m%d` |
  | `TIME` | `%h:%i:%s %p` | `%H:%i:%s` | `%H:%i:%s` | `%H.%i.%s` | `%H%i%s` |
  | `DATETIME` | `%Y-%m-%d %H.%i.%s` | `%Y-%m-%d %H:%i:%s` | `%Y-%m-%d %H:%i:%s` | `%Y-%m-%d %H.%i.%s` | `%Y%m%d%H%i%s` |
  | `TIMESTAMP` | `%Y-%m-%d %H.%i.%s` | `%Y-%m-%d %H:%i:%s` | `%Y-%m-%d %H:%i:%s` | `%Y-%m-%d %H.%i.%s` | `%Y%m%d%H%i%s` |

- `GET_FORMAT()` as a scalar format argument to the existing
  `DATE_FORMAT()`, `TIME_FORMAT()`, and `STR_TO_DATE()` subsets when the
  resulting format string is otherwise admitted by those functions;
- output text/`NULL` through existing result APIs;
- warning count `0` for supported forms.

## Deferred Surface

This slice intentionally does not support:

- expression-valued format arguments such as `CONCAT('U','SA')`, variables,
  descriptor columns, subqueries, parameters, or function calls other than the
  top-level `GET_FORMAT()` itself;
- `GET_FORMAT()` in `WHERE`, `ORDER BY`, `GROUP BY`, `HAVING`, DML assignments,
  defaults, generated columns, indexes, constraints, joins, CTEs, or arbitrary
  SQLite pass-through;
- using `GET_FORMAT()` where the consuming function's current format-token
  subset rejects the returned format string. For example, MyLite may still
  reject week-related or otherwise deferred consumers even though
  `GET_FORMAT()` itself produced a valid MySQL format string;
- protocol-grade expression metadata beyond the existing scalar/row-scalar
  result conventions.

## Grammar

MyLite adds these parser productions:

```lemon
get_format_class(A) ::= DATE(T).
get_format_class(A) ::= TIME(T).
get_format_class(A) ::= DATETIME(T).
get_format_class(A) ::= TIMESTAMP(T).

expression(A) ::= GET_FORMAT(T) LPAREN get_format_class(B) COMMA expression(C) RPAREN(R).
```

Wrong arities are not admitted by a dedicated argument-count node in this
baseline. They remain syntax errors, matching observed MySQL 8.4.9 behavior:

```sql
GET_FORMAT()
GET_FORMAT(DATE)
GET_FORMAT(DATE, 'USA', 'extra')
```

`GET_FORMAT` is admitted as an identifier where MyLite admits ordinary
identifiers:

```lemon
identifier(A) ::= GET_FORMAT(T).
```

Analyzer/runtime acceptance is narrower:

```lemon
get_format_expr(A) ::= GET_FORMAT LPAREN get_format_class(B) COMMA get_format_value(C) RPAREN.

get_format_value(A) ::= string_literal(T).
get_format_value(A) ::= signed_integer_literal(T).
get_format_value(A) ::= TRUE(T).
get_format_value(A) ::= FALSE(T).
get_format_value(A) ::= NULL(T).
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning:

1. Detect no-source/`DUAL` scalar expressions and row-scalar projection
   attempts that contain a top-level or parenthesized `GET_FORMAT()` call.
2. Preserve the existing selected/default schema policy for row-scalar `SELECT`
   statements. `GET_FORMAT()` itself has no catalog dependency.
3. Decode string literal format names using the active statement SQL mode,
   including `ANSI_QUOTES` and `NO_BACKSLASH_ESCAPES`.
4. Convert accepted non-string format values to their MySQL-style scalar text
   for lookup. Numeric and boolean values do not match a recognized format name
   and return `NULL`.
5. Return the mapped format string for recognized names, case-insensitively.
   Return SQL `NULL` for unknown or `NULL` format values.
6. When `GET_FORMAT()` is used as a format argument to `DATE_FORMAT()`,
   `TIME_FORMAT()`, or `STR_TO_DATE()`, scalar planning first resolves
   `GET_FORMAT()` to its constant text/`NULL` value and then lets the consuming
   function apply its existing validation.

No SQLite extension function is required for this baseline. `GET_FORMAT()` is a
constant MyLite-owned expression and can be lowered as a bound value or SQL
`NULL` inside existing generated SQL shapes. Row-backed scans, filters,
ordering, and limiting remain delegated to SQLite.

## Diagnostics

- Syntax errors, including wrong arity and invalid first-argument syntax, use
  the existing parser syntax-error diagnostic path.
- Unsupported expression-valued format arguments fail with a deterministic
  MyLite unsupported-feature diagnostic:
  `GET_FORMAT() supports only literal format names`.
- String literals containing embedded `NUL` fail with:
  `GET_FORMAT() format names do not support NUL bytes`.
- Allocation failures follow existing `MYLITE_NOMEM` paths.
- Physical SQLite failures are not expected because no SQLite scalar callback
  is added; existing generated-query failures remain reported by the caller.

## Tests

Add MySQL-runtime expectation coverage for:

- all `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP` mappings;
- `TIMESTAMP` matching `DATETIME`;
- case-insensitive temporal class and format names;
- `NULL`, unknown, numeric, and boolean format values returning `NULL` where
  MySQL does;
- use inside `DATE_FORMAT()`, `TIME_FORMAT()`, and `STR_TO_DATE()` for admitted
  consumer format strings;
- no-source, `FROM DUAL`, `DO`, and row-scalar constant projection;
- labels, row count, warning count, and absence of result rows for `DO`;
- identifier behavior for `get_format` as an unquoted table name;
- syntax errors for wrong arity, string first arguments, and unsupported
  temporal class names;
- explicit MySQL acceptance of deferred expression-valued format arguments.

Add fast C coverage under `packages/libmylite/tests/`, registered with CTest,
covering the same supported subset plus zero-initialized cleanup through the
existing result lifecycle helpers.

## Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/functions-temporal.md`;
- `docs/compatibility/sql-query-expressions.md` only if the documented scalar
  expression surface needs a new mention;
- `docs/compatibility/type-system-literals-conversion.md` if literal
  placement wording changes.

Use limited wording. Do not claim full `GET_FORMAT()` expression arguments,
predicate use, DML assignment use, general function nesting, or broader
expression metadata.
