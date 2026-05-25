# Baseline Temporal Constructor Functions

## Scope

This phase adds a narrow, MySQL-runtime-verified subset of temporal scalar
constructor functions:

- `FROM_DAYS(day_number)`
- `MAKEDATE(year, day_of_year)`
- `MAKETIME(hour, minute, second)`

The supported surface is intentionally limited to no-source `SELECT`, `SELECT
... FROM DUAL`, `DO`, and single-table row-scalar `SELECT` projections. Inputs
are limited to `NULL`, `TRUE`, `FALSE`, signed decimal integer literals with an
optional unary sign, and integer-family descriptor columns in row-scalar
projections. String, decimal, approximate, hex, bit, parameter, subquery,
arithmetic, column-to-column, temporal, and general expression arguments remain
outside this phase.

Official MySQL 8.4 documentation defines these as date/time functions that
construct a date or time value from numeric inputs:

- <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html#function_from-days>
- <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html#function_makedate>
- <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html#function_maketime>

Runtime expectations below were verified against MySQL 8.4.9 using the local
`mysql:8.4.9` container named `mylite-mysql-849`.

## Ownership Boundaries

- Public API: no new public ABI is added. Results and diagnostics flow through
  the existing `mylite_execute()` and result APIs.
- Parser/AST: recognizes the three function names, captures their supported
  arity shapes, and represents arity errors as AST nodes so runtime diagnostics
  can use MySQL-compatible native-function parameter-count errors.
- Analyzer/planner: validates scalar arguments before SQLite SQL generation,
  resolves row-scalar identifier arguments only against MyLite descriptors, and
  admits integer-family descriptor columns only.
- Runtime scalar evaluator: evaluates source-free literal calls directly in
  MyLite-owned code and appends warnings where MySQL does.
- Row-scalar execution: lowers row-backed projections to private SQLite scalar
  callbacks with quoted descriptor-driven column references and bound literal
  parameters. The callbacks call MyLite-owned constructor helpers and use the
  connection owner to append warnings.
- Catalog and storage: descriptors remain authoritative. These functions do
  not read or mutate catalog rows, descriptor versions, table storage, the
  `.mylite` preamble, or SQLite schema text.
- SQLite integration: implementation uses public SQLite scalar-function APIs
  only. No SQLite fork patch or schema change is needed.

## Grammar

Independently authored Lemon-shape snippets:

```lemon
expression(A) ::= FROM_DAYS(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= FROM_DAYS(T) LPAREN RPAREN(R).
expression(A) ::= FROM_DAYS(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R).

expression(A) ::= MAKEDATE(T) LPAREN expression(B) COMMA expression(C) RPAREN(R).
expression(A) ::= MAKEDATE(T) LPAREN RPAREN(R).
expression(A) ::= MAKEDATE(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= MAKEDATE(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R).

expression(A) ::= MAKETIME(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R).
expression(A) ::= MAKETIME(T) LPAREN RPAREN(R).
expression(A) ::= MAKETIME(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= MAKETIME(T) LPAREN expression(B) COMMA expression(C) RPAREN(R).
expression(A) ::= MAKETIME(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) COMMA function_argument_list(E) RPAREN(R).
```

The keywords also remain usable as identifiers where the current MyLite keyword
fallback rules allow function names as column names.

## Argument Conversion

Supported scalar arguments are converted by MyLite before binding or direct
evaluation:

- `NULL` marks the argument null.
- `TRUE` converts to `1`; `FALSE` converts to `0`.
- Unsigned decimal integer tokens with optional unary `+` or `-` must fit the
  signed 64-bit range after sign application.
- Descriptor-column arguments must be integer-family columns stored through the
  current `INTEGER` physical type. Runtime `NULL` values propagate as MySQL
  does.

Unsupported argument shapes fail with deterministic MyLite diagnostics naming
the function and admitted subset. Unknown row-scalar identifiers fail through
the existing unknown-column diagnostic path.

## Semantics

### `FROM_DAYS(day_number)`

Observed MySQL 8.4.9 behavior for the admitted integer subset:

- `NULL` returns `NULL`.
- Day numbers less than `366` return `0000-00-00` without a warning, including
  negative values and `0`.
- Day numbers `366..3652424` return the corresponding date in MySQL's proleptic
  calendar. `366` maps to `0001-01-01`; `3652424` maps to `9999-12-31`.
- Day numbers `3652425..3652499` return `NULL` and append warning
  `1441 / HY000 / Datetime function: from_days field overflow`.
- Day numbers `>= 3652500` return `0000-00-00` without a warning.

This slice preserves the documented MySQL caution that day-number conversion is
not a general historical-calendar feature.

### `MAKEDATE(year, day_of_year)`

Observed MySQL 8.4.9 behavior for the admitted integer subset:

- If either argument is `NULL`, the result is `NULL`.
- `day_of_year <= 0` returns `NULL` without a warning.
- Two-digit year conversion is applied to `0..99`: `0..69` map to
  `2000..2069`, and `70..99` map to `1970..1999`.
- Years `100..9999` are used directly, including years below `1000`.
- Negative years and years greater than `9999` return `NULL`.
- Positive day-of-year values may roll into later years. Results beyond
  `9999-12-31` return `NULL` without a warning.

### `MAKETIME(hour, minute, second)`

Observed MySQL 8.4.9 behavior for the admitted integer subset:

- If any argument is `NULL`, the result is `NULL`.
- `minute` and `second` must be in `0..59`; values outside that range return
  `NULL` without a warning.
- A negative `hour` produces a negative time. `minute` and `second` remain
  nonnegative magnitude parts.
- Absolute times beyond `838:59:59` are clipped to `838:59:59` or
  `-838:59:59` and append warning `1292 / 22007 / Truncated incorrect time
  value: '<original h:m:s>'`.
- Integer seconds only are admitted in this phase. MySQL's fractional-second
  `MAKETIME()` behavior is explicitly deferred.

## Result Shape

Successful functions return one text value or `NULL` through existing scalar
result conventions. Successful in-range calls do not affect rows and report the
existing result conventions for `SELECT` and `DO`; `DO` reports affected rows
`0`. Supported in-range calls have warning count `0`.

## Diagnostics

- Arity errors use native-function parameter-count diagnostics for
  `FROM_DAYS`, `MAKEDATE`, or `MAKETIME`.
- Unsupported argument expressions use deterministic MyLite unsupported
  diagnostics naming the admitted literal/descriptor subset.
- Unknown row-scalar columns use the existing unknown-column diagnostics.
- Non-integer descriptor columns use deterministic MyLite unsupported
  diagnostics.
- Signed integer literal overflow uses deterministic MyLite range diagnostics.
- `FROM_DAYS()` overflow uses warning `1441 / HY000`.
- `MAKETIME()` clipping uses warning `1292 / 22007`.
- Allocation failure uses the existing `MYLITE_NOMEM / HY001` diagnostic.
- Physical SQLite callback failures map back to MyLite runtime diagnostics
  through existing row-scalar error handling.

## Testing

The test suite must cover:

- Parser and lexer recognition, arity markers, and identifier fallback.
- No-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`.
- `NULL`, boolean, signed integer, boundary, overflow, and clipping cases.
- MySQL warning count and warning rows for `FROM_DAYS()` overflow and
  `MAKETIME()` clipping.
- Row-scalar projection over integer-family descriptor columns, including
  persisted close/reopen reads.
- Unsupported argument expressions, unsupported descriptor column types, unknown
  columns, and signed-64 literal overflow.
- The MySQL expectation script must verify the user-visible behavior above
  against MySQL 8.4.9.
