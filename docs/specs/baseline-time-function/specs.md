# Baseline TIME Function

## Summary

This phase adds a narrow MySQL-compatible `TIME(expr)` projection slice. The
function extracts a time string from an admitted `TIME`, `DATE`, `DATETIME`, or
`TIMESTAMP` value without creating a general temporal coercion engine.

Supported contexts:

- no-source scalar `SELECT`;
- `SELECT ... FROM DUAL`;
- `DO`;
- single-table row-scalar `SELECT` projection over descriptor columns.

The slice does not add `TIME()` in predicates, ordering, grouping, DML
assignments, defaults, generated columns, or arbitrary nested expressions.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing related MyLite specs:
  - `docs/specs/baseline-temporal-extract-functions/specs.md`
  - `docs/specs/baseline-time-type/specs.md`
  - `docs/specs/baseline-datetime-type/specs.md`
  - `docs/specs/baseline-timestamp-type/specs.md`
  - `docs/specs/baseline-current-date-time-functions/specs.md`
- Official MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_time_function_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 behavior, public SQLite
APIs, and existing MyLite code. It does not copy MySQL, MariaDB, Percona,
SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

MySQL documents `TIME(expr)` as extracting the time portion of a time or
datetime expression and returning it as a string. Runtime probes for this
baseline establish:

- `TIME('2003-12-31 01:02:03')` returns `01:02:03`.
- `TIME('01:02:03')` returns `01:02:03`.
- `TIME('-13:29:17')` preserves the sign and returns `-13:29:17`.
- Double-quoted string literals follow the current default string-literal
  treatment; `TIME("-13:29:17")` returns `-13:29:17`.
- `TIME('-272:59:59')` returns `-272:59:59`.
- `TIME('272:59:59')` returns `272:59:59`.
- `TIME(NULL)` returns `NULL`.
- `TIME()` and `TIME(a, b)` are syntax errors.
- Whitespace between `TIME` and `(` is accepted.
- `TIME(DATE_column)` returns `00:00:00`.
- `TIME(DATETIME_column)` and `TIME(TIMESTAMP_column)` return the stored time
  portion.
- `TIME(TIME_column)` returns the stored time text, including a leading minus
  sign for negative times.
- Invalid string input such as `TIME('not-a-date')` returns `NULL` with warning
  `1292 / 22007` and message beginning `Truncated incorrect time value:`.
- MySQL accepts broader inputs, including compact numeric temporal values,
  date-only strings with warning-producing time coercion, fractional seconds,
  and string descriptor columns with metadata-sensitive fractional display.
  Those require broader temporal coercion and are intentionally deferred.

## Ownership Boundary

- Public API: no ABI changes. `mylite_execute()` and existing result APIs
  expose rows, warnings, diagnostics, and affected-row conventions.
- Statement context: no new session state. SQL mode is not widened beyond the
  existing canonical temporal storage/conversion subset.
- Lexer/parser/AST: the existing `TIME` token is admitted as a one-argument
  expression function while remaining usable as a column type keyword and as an
  identifier where MyLite already allows it.
- Analyzer/planner/runtime: supported literal arguments are decoded during
  scalar planning; descriptor column arguments are resolved from MyLite catalog
  descriptors. The runtime lowers row-backed calls to a MyLite-owned SQLite
  scalar function.
- Catalog: no descriptor, descriptor version, catalog generation, or SQLite
  schema-generation mutation.
- Storage/VFS: no `.mylite` preamble, file-format, VFS, or SQLite payload
  changes.
- SQLite: this is a MyLite wrapper/function change using public
  `sqlite3_create_function_v2()` registration. No SQLite fork patch is needed.

## Supported SQL

No-source and `DUAL` projection:

```sql
SELECT TIME(time_value)
SELECT TIME(time_value) FROM DUAL
```

`DO` execution:

```sql
DO TIME(time_value)
```

Single-table row-scalar projection:

```sql
SELECT TIME(column_name) FROM table_name [WHERE predicate] [ORDER BY column_name] [LIMIT n]
```

`time_value` is one of:

- `NULL`;
- a single- or double-quoted string literal in one of these exact forms:
  - `HH:MM:SS`;
  - `-HH:MM:SS`;
  - `HHH:MM:SS`;
  - `-HHH:MM:SS`;
  - `YYYY-MM-DD HH:MM:SS`, including admitted zero-date forms with a time part;
- a row-scalar descriptor column of type `DATE`, `TIME`, `DATETIME`, or
  `TIMESTAMP`.

`DATE` descriptor input returns `00:00:00`. `DATETIME` and `TIMESTAMP`
descriptor inputs return their time portion. `TIME` descriptor input returns
the stored canonical time text.

## Deferred Surface

This slice intentionally does not support:

- numeric temporal literals, boolean values, bit/hex literals, decimal or float
  values, variables, parameters, subqueries, or arbitrary expressions;
- fractional seconds;
- date-only string coercion such as `TIME('2008-01-02')`;
- compact temporal strings, relaxed delimiters, two-digit years, time-zone
  suffixes, trailing garbage, or broader SQL-mode-sensitive temporal parsing;
- string-family descriptor columns as `TIME()` input;
- out-of-range time clipping to `838:59:59`;
- `TIME()` inside predicates, ordering, grouping, DML assignments, defaults,
  generated columns, constraints, joins, CTEs, or SQLite pass-through;
- `TIME_FORMAT()`, `TIME_TO_SEC()`, `TIMEDIFF()`, `TIMESTAMP()`, or broader
  temporal functions.

## Grammar

MyLite adds this independently authored Lemon-shape production:

```lemon
expression(A) ::= TIME(T) LPAREN expression(V) RPAREN(R).
```

Unsupported arities use existing syntax-error behavior:

```lemon
expression ::= TIME LPAREN RPAREN.                         // syntax error
expression ::= TIME LPAREN expression COMMA expression RPAREN. // syntax error
```

`TIME` remains an admitted type keyword:

```lemon
time_type(A) ::= TIME(T).
```

and an admitted identifier where the existing grammar accepts nonreserved
keyword identifiers:

```lemon
identifier(A) ::= TIME(T).
```

Semantic narrowing is enforced by the analyzer/runtime:

```lemon
time_function_expr ::= TIME LPAREN supported_time_value RPAREN.

supported_time_value ::= NULL.
supported_time_value ::= canonical_time_string.
supported_time_value ::= canonical_datetime_string.
supported_time_value ::= descriptor_date_column.
supported_time_value ::= descriptor_time_column.
supported_time_value ::= descriptor_datetime_column.
supported_time_value ::= descriptor_timestamp_column.
```

## Conversion And Result Semantics

Scalar literal conversion:

- `NULL` returns `NULL` with no warning.
- Canonical `HH:MM:SS`, `-HH:MM:SS`, `HHH:MM:SS`, and `-HHH:MM:SS` literals
  return the same canonical time string.
- Canonical `YYYY-MM-DD HH:MM:SS` literals return the `HH:MM:SS` suffix.
- Invalid or currently unsupported string literals return `NULL` and append
  warning `1292 / 22007` using the existing MyLite temporal warning path.

Descriptor conversion:

- `DATE` descriptors return `00:00:00` for non-`NULL` values.
- `TIME` descriptors return the stored canonical value.
- `DATETIME` and `TIMESTAMP` descriptors return the stored time suffix.
- `NULL` descriptor values return `NULL`.

Successful supported calls append no warnings. `DO TIME(...)` returns no row
result set and uses existing `DO` result conventions.

## Physical SQLite Handling

For row-backed projection, MyLite generates a descriptor-built expression of
the form:

```sql
_mylite_temporal_extract(<descriptor_column>, ?, ?)
```

The first argument is the quoted physical descriptor column. The second and
third arguments are bound string parameters identifying the requested extract
kind and descriptor input kind. Literal values are also bound parameters; user
text is not interpolated into generated SQL.

SQLite executes the scan and invokes the MyLite function per returned row. This
does not materialize rows in MyLite and does not bypass SQLite filtering,
ordering, or limiting for the surrounding supported row-scalar `SELECT`.

## Diagnostics

- Unsupported nonliteral scalar arguments without a table source produce the
  existing deterministic unsupported temporal-extract diagnostic.
- Unknown row-backed columns produce the existing unknown-column diagnostic.
- Unsupported row-backed descriptor kinds produce a deterministic unsupported
  diagnostic for `TIME()` input.
- Allocation failure reports `MYLITE_NOMEM`.
- Physical SQLite failures are reported through the existing runtime error
  path.

## Test Plan

- MySQL 8.4.9 expectation script for scalar literals, `NULL`, whitespace,
  `DUAL`, `DO`, descriptor `DATE` / `TIME` / `DATETIME` / `TIMESTAMP` inputs,
  invalid warnings, syntax errors, and deferred accepted surfaces.
- Parser tests for `TIME(expr)` and continued `TIME` type parsing.
- Runtime tests for no-source, `DUAL`, `DO`, table-backed descriptor columns,
  filtering/order/limit envelope, invalid warnings, unsupported arguments, and
  reopen persistence.
- Compatibility docs update for the exact limited surface.
- Focused build/CTest plus full `cmake --workflow --preset check`.
