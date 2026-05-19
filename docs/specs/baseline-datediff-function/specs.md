# Baseline DATEDIFF Function

## Goal

Add a narrow `DATEDIFF(expr1, expr2)` slice for common temporal projection:

```sql
SELECT DATEDIFF(updated_at, created_at) FROM posts;
```

This phase extends MyLite's no-source scalar, `DUAL`, `DO`, and single-table
row-scalar `SELECT` paths. It is not a general temporal expression engine and
does not add expression predicates, expression ordering, grouping, generated
columns, defaults, DML assignment expressions, or relaxed temporal coercion.

## Sources

- Official MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Official MySQL 8.4 Reference Manual, function-name parsing and resolution:
  <https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html>
- Existing MyLite temporal and row-scalar designs:
  - `docs/specs/baseline-temporal-extract-functions/specs.md`
  - `docs/specs/baseline-date-format-function/specs.md`
  - `docs/specs/baseline-unix-timestamp-function/specs.md`
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_datediff_function_expectations.sh`.

This specification is independently authored from official documentation,
observed MySQL 8.4.9 behavior, public SQLite APIs, and existing MyLite code. It
does not copy MySQL, MariaDB, Percona, SQLite implementation internals, or
restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `DATEDIFF(expr1, expr2)` returns `expr1 - expr2` in whole days.
- Only the date parts of date or datetime values participate. Time portions are
  ignored.
- `NULL` arguments make the result `NULL`.
- MySQL still evaluates non-`NULL` invalid arguments when the other argument is
  `NULL`: `DATEDIFF(NULL, 'bad')` and `DATEDIFF('bad', NULL)` both return
  `NULL` and append warning `1292 / 22007`.
- Invalid non-temporal values return `NULL` and append warning `1292 / 22007`
  with text beginning `Incorrect datetime value:`.
- Full-zero dates such as `'0000-00-00'` and partial-zero dates such as
  `'2001-11-00'` return `NULL` with the same warning in this function.
- Stored `DATE`, `DATETIME`, and `TIMESTAMP` descriptor values that are
  full-zero or partial-zero return `NULL` without adding a new warning; string
  descriptor values containing the same text still warn like string literals.
- Year-zero dates with nonzero month and day, such as `'0000-01-02'`, are
  accepted for day arithmetic.
- Day arithmetic follows the Gregorian calendar shape observed in MySQL 8.4.9;
  year zero is accepted for complete dates but is not treated as a leap year.
- `DATEDIFF` accepts whitespace before `(` in default SQL mode.
- `DATEDIFF` is usable as an unquoted table identifier outside function-call
  contexts.
- Wrong argument counts fail with `1582 / 42000`, native function
  parameter-count diagnostics.
- Successful supported statements produce no warnings. A successful scalar
  `SELECT` makes a following `ROW_COUNT()` return `-1`; a successful `DO`
  makes it return `0`.

## Supported Surface

MyLite supports:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` using the existing row envelope:
  `FROM table [AS alias]`, optional existing `WHERE`, single descriptor-column
  `ORDER BY`, and existing `LIMIT`;
- `DATEDIFF(value1, value2)` with exactly two arguments;
- each value as:
  - `NULL`;
  - a single- or double-quoted string literal containing canonical
    `YYYY-MM-DD` or `YYYY-MM-DD HH:MM:SS`;
  - a descriptor column in a table-backed row-scalar `SELECT` whose descriptor
    family is `DATE`, `DATETIME`, `TIMESTAMP`, `CHAR`, `VARCHAR`, or baseline
    `TEXT`;
- valid complete date values in MyLite's current storage baseline range plus
  MySQL-observed year-zero complete dates;
- output signed integer text or `NULL` through existing result APIs.

The supported row-scalar path evaluates through a MyLite-owned SQLite scalar
function so SQLite still performs table scanning, filtering, ordering, and
limiting. MyLite does not materialize the source row set to compute the
difference.

## Deferred Surface

This slice intentionally does not support:

- numeric temporal literals, booleans, bit/hex literals, decimal or float
  values, parameters, variables, subqueries, or arbitrary expressions as
  arguments;
- fractional seconds;
- compact numeric temporal text, two-digit years, locale or time-zone
  coercion, relaxed temporal strings, or broader SQL-mode-sensitive temporal
  parsing;
- `TIME` descriptor arguments;
- use in `WHERE`, `ORDER BY`, `GROUP BY`, `HAVING`, DML assignments, defaults,
  generated columns, indexes, constraints, joins, CTEs, or arbitrary SQLite
  pass-through;
- broader temporal functions such as `TIMEDIFF()`, `TIMESTAMPDIFF()`,
  `TO_DAYS()`, `DAYOFYEAR()`, or `EXTRACT()`.

## Grammar

MyLite adds this parser production:

```lemon
expression(A) ::= DATEDIFF(T) LPAREN expression(B) COMMA expression(C) RPAREN(R).
```

Wrong arities produce an argument-count AST node so runtime can return MySQL's
native-function parameter-count diagnostic:

```lemon
expression(A) ::= DATEDIFF(T) LPAREN RPAREN(R).
expression(A) ::= DATEDIFF(T) LPAREN expression(B) RPAREN(R).
expression(A) ::=
    DATEDIFF(T) LPAREN expression(B) COMMA expression(C)
    COMMA function_argument_list(D) RPAREN(R).
```

`DATEDIFF` is admitted as an identifier where MyLite admits ordinary
nonreserved identifiers:

```lemon
identifier(A) ::= DATEDIFF(T).
```

Analyzer/runtime acceptance is narrower:

```lemon
datediff_expr(A) ::= DATEDIFF LPAREN datediff_value(B) COMMA datediff_value(C) RPAREN.

datediff_value(A) ::= NULL(T).
datediff_value(A) ::= string_literal(T).
datediff_value(A) ::= descriptor_date_column(C).
datediff_value(A) ::= descriptor_datetime_column(C).
datediff_value(A) ::= descriptor_timestamp_column(C).
datediff_value(A) ::= descriptor_string_column(C).
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning:

1. Detect no-source/`DUAL` scalar expressions and row-scalar projection
   attempts containing `DATEDIFF()`.
2. Resolve row sources through the existing selected/default schema policy.
3. Resolve descriptor column arguments through MyLite catalog descriptors, not
   SQLite schema text.
4. Decode string literal arguments using the current statement SQL mode,
   including `ANSI_QUOTES` and `NO_BACKSLASH_ESCAPES`.
5. Reject unsupported argument kinds before generated SQLite SQL exists.
6. Generate SQLite projection SQL over stable physical table names and quoted
   physical column names. String literals and internal discriminators are bound
   parameters.
7. Use a MyLite-owned SQLite scalar function for table-backed row execution.

Evaluation:

1. Each argument is evaluated independently.
2. A SQL `NULL` argument contributes no warning but makes the final result
   `NULL`.
3. A non-`NULL` invalid string argument appends warning `1292 / 22007`,
   `Incorrect datetime value: 'value'`, and makes the final result `NULL`.
   Invalid stored temporal descriptor arguments also make the result `NULL`;
   MySQL 8.4.9 does not add a fresh warning for stored full-zero or
   partial-zero temporal descriptor values.
4. Date-only values use that date. Datetime and timestamp values use their date
   portion and ignore the time portion.
5. Full-zero and partial-zero dates are invalid for `DATEDIFF()`. Year-zero
   complete dates are valid, except invalid calendar days such as
   `'0000-02-29'`.
6. Valid arguments are converted to internal day numbers using the verified
   Gregorian behavior, and the signed result is `left_days - right_days`.

The row-backed generated SQL shape is:

```sql
_mylite_datediff(left_value, left_kind, right_value, right_kind)
```

`left_kind` and `right_kind` are bound MyLite-internal discriminators such as
`string`, `date`, `datetime`, or `timestamp`. They are not user-visible.

## Ownership Boundaries

- Public API: unchanged. Successful statements use existing `mylite_execute()`
  and result APIs.
- Statement context: unchanged. Diagnostics and row-count behavior remain
  statement-owned.
- Lexer/parser/AST: add `DATEDIFF` token handling, AST node kinds, grammar, and
  argument-count nodes. Parser source spans remain result-label authority.
- Analyzer/planner: resolve descriptor arguments and reject unsupported shapes.
  It creates bound planned values and internal function calls; it does not ask
  SQLite metadata which columns exist.
- Catalog: read-only descriptor authority. `DATEDIFF()` must not mutate
  descriptor rows, descriptor versions, descriptor caches, catalog generation,
  or `sqlite_schema_generation`.
- Result builder: returns signed integer text or `NULL`, using aliases or
  source spans for labels.
- Storage/VFS/file format: unchanged. `.mylite` preamble and shifted SQLite
  payload invariants are preserved.
- SQLite: use the public scalar-function registration API for
  `_mylite_datediff`. No SQLite fork patch is required.

## Diagnostics

Required diagnostics:

- wrong arity: `1582 / 42000`, `Incorrect parameter count in the call to
  native function 'DATEDIFF'`;
- invalid non-`NULL` temporal input: warning `1292 / 22007`, `Incorrect
  datetime value: 'value'`;
- unknown descriptor column: existing MySQL-compatible unknown-column
  diagnostic;
- unsupported argument literal/expression:
  `DATEDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor columns, string descriptor columns, and NULL`;
- unsupported `TIME` descriptor column:
  `DATEDIFF() does not yet support TIME values`;
- unsupported NUL-containing literal:
  `DATEDIFF() literals do not support NUL bytes`;
- allocation failure: existing `MYLITE_NOMEM` diagnostic behavior;
- physical SQLite failure: existing wrapped SQLite diagnostics.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`: mark `DATEDIFF()` as limited.
- `docs/compatibility/functions-temporal.md`: document the exact supported
  scalar and row-scalar projection subset.
- `docs/compatibility/type-system-literals-conversion.md`: mention string
  literals in the admitted `DATEDIFF()` argument context.

Do not claim support for `TIMEDIFF()`, `TIMESTAMPDIFF()`, general temporal
arithmetic, temporal predicates, expression ordering, or general expression
evaluation.

## Tests

Add fast C tests under `packages/libmylite/tests/` and register them as
`libmylite.runtime.datediff_function`.

Coverage:

- no-source and `DUAL` scalar `DATEDIFF()` values;
- `DO DATEDIFF(...)` status, affected rows, and warnings;
- labels and whitespace before `(`;
- table-backed row-scalar projection over `DATE`, `DATETIME`, `TIMESTAMP`, and
  string descriptor columns;
- `WHERE`, `ORDER BY`, and `LIMIT` preserved by the existing row envelope;
- close/reopen persistence for source rows and repeated evaluation;
- `NULL` arguments, including invalid non-`NULL` peer arguments still warning;
- full-zero, partial-zero, year-zero complete dates, leap years, and range
  boundaries;
- wrong arity, unknown column, unsupported numeric/boolean/expression
  arguments, unsupported `TIME` descriptor columns, and NUL-containing string
  literals;
- zero-initialized cleanup paths.

Run:

```sh
sh packages/libmylite/tests/mysql_baseline_datediff_function_expectations.sh
ctest --preset dev --output-on-failure -R 'libmylite\.(parser|runtime\.datediff_function|runtime\.temporal_extract_functions|runtime\.date_format_function|runtime\.unix_timestamp_function)'
cmake --workflow --preset check
```
