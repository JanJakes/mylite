# Baseline Time Second Conversion Functions

## Summary

This phase adds a narrow MySQL-compatible temporal conversion pair:

```sql
TIME_TO_SEC(time_value)
SEC_TO_TIME(second_value)
```

The supported slice covers no-source scalar `SELECT`, `SELECT ... FROM DUAL`,
`DO`, and single-table row-scalar `SELECT` projection contexts. It does not add
a general temporal coercion engine, function predicates, expression ordering,
DML assignments, generated defaults, or arbitrary nested expression planning.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing related MyLite specs:
  - `docs/specs/baseline-temporal-extract-functions/specs.md`
  - `docs/specs/baseline-time-function/specs.md`
  - `docs/specs/baseline-time-type/specs.md`
  - `docs/specs/baseline-datetime-type/specs.md`
  - `docs/specs/baseline-timestamp-type/specs.md`
  - `docs/specs/baseline-row-scalar-expression-projection/specs.md`
- Official MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_time_second_conversion_functions_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 behavior, public SQLite
APIs, and existing MyLite code. It does not copy MySQL, MariaDB, Percona,
SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `TIME_TO_SEC('22:23:00')` returns `80580`.
- `TIME_TO_SEC('00:39:38')` returns `2378`.
- `TIME_TO_SEC('-00:39:38')` returns `-2378`.
- `TIME_TO_SEC('100:00:00')` returns `360000`.
- `TIME_TO_SEC(NULL)` returns `NULL`.
- `TIME_TO_SEC('2003-12-31 01:02:03')` ignores the date part and returns
  `3723`.
- `TIME_TO_SEC('2003-12-31 24:00:00')` returns `NULL` and appends warning
  `1292 / 22007` with text beginning `Truncated incorrect time value:`.
- `TIME_TO_SEC()` and `TIME_TO_SEC(a, b)` fail with native-function argument
  count error `1582 / 42000`.
- `SEC_TO_TIME(2378)` returns `00:39:38`.
- `SEC_TO_TIME(-2378)` returns `-00:39:38`.
- `SEC_TO_TIME(0)` returns `00:00:00`.
- `SEC_TO_TIME(NULL)` returns `NULL`.
- `SEC_TO_TIME(TRUE)` returns `00:00:01`; `SEC_TO_TIME(FALSE)` returns
  `00:00:00`.
- `SEC_TO_TIME(3020399)` returns `838:59:59`.
- `SEC_TO_TIME(3020400)` returns `838:59:59` and appends warning
  `1292 / 22007` with text beginning `Truncated incorrect time value:`.
- `SEC_TO_TIME(-3020400)` returns `-838:59:59` with the same warning class.
- `SEC_TO_TIME()` and `SEC_TO_TIME(a, b)` fail with native-function argument
  count error `1582 / 42000`.
- Whitespace between each function name and `(` is accepted.
- Successful supported calls produce no warnings. A preceding scalar `SELECT`
  makes `ROW_COUNT()` return `-1`; a supported `DO` makes it return `0`.
- MySQL accepts broader values such as decimal seconds, numeric temporal
  coercions, date-only time coercions, string seconds for `SEC_TO_TIME()`, and
  descriptor-specific string/number coercions. This slice defers those forms
  because they require the broader MySQL temporal and decimal conversion model.

## Ownership Boundary

- Public API: no ABI changes. `mylite_execute()` and existing result APIs
  expose row values, diagnostics, warning counts, and row-count conventions.
- Statement context: no new session state. The existing warning diagnostics
  area records truncation warnings for supported clipped or invalid values.
- Lexer/parser/AST: the two function names become one-argument expression
  functions while remaining usable as identifiers where MyLite admits ordinary
  nonreserved keyword identifiers.
- Analyzer/planner/runtime: supported scalar literals are converted by
  MyLite-owned conversion helpers. Descriptor-column arguments are resolved
  from MyLite catalog descriptors, then lowered to MyLite-owned SQLite scalar
  callbacks for row-backed projection.
- Catalog: no descriptor, descriptor version, catalog generation, or SQLite
  schema-generation mutation.
- Storage/VFS: no `.mylite` preamble, file-format, VFS, or SQLite payload
  changes.
- SQLite: row-backed scans, filtering, ordering, and limiting remain delegated
  to SQLite. MyLite uses public `sqlite3_create_function_v2()` scalar callbacks
  only for per-row conversion. No SQLite fork patch is needed.

## Supported SQL

No-source and `DUAL` projection:

```sql
SELECT TIME_TO_SEC(time_value)
SELECT SEC_TO_TIME(second_value)
SELECT TIME_TO_SEC(time_value), SEC_TO_TIME(second_value) FROM DUAL
```

`DO` execution:

```sql
DO TIME_TO_SEC(time_value), SEC_TO_TIME(second_value)
```

Single-table row-scalar projection:

```sql
SELECT TIME_TO_SEC(column_name), SEC_TO_TIME(column_name)
FROM table_name [WHERE predicate] [ORDER BY column_name [ASC | DESC]] [LIMIT row_count]
```

The existing single-table row-scalar envelope supplies target table resolution,
predicate resolution, descriptor-column projection, `ORDER BY`, and `LIMIT`
behavior.

### `TIME_TO_SEC()` Inputs

`time_value` is one of:

- `NULL`;
- a single- or double-quoted string literal in one of these exact forms:
  - `HH:MM:SS`;
  - `-HH:MM:SS`;
  - `HHH:MM:SS`;
  - `-HHH:MM:SS`;
  - `YYYY-MM-DD HH:MM:SS`, including admitted zero-date forms with a time part;
- a row-scalar descriptor column of type `DATE`, `TIME`, `DATETIME`,
  `TIMESTAMP`, `CHAR`, `VARCHAR`, or baseline `TEXT`.

`DATE` descriptor input returns `0` for non-`NULL` values. `TIME`, `DATETIME`,
and `TIMESTAMP` descriptor inputs use their stored time portion. String-family
descriptor inputs are parsed through the same canonical string subset as scalar
string literals.

### `SEC_TO_TIME()` Inputs

`second_value` is one of:

- `NULL`;
- a decimal integer literal with optional unary `+` or `-` in the signed 64-bit
  range;
- `TRUE` or `FALSE`;
- a row-scalar descriptor column from an integer-family descriptor whose stored
  physical value is in the signed 64-bit range.

Integer-family descriptors currently include `TINYINT`, `SMALLINT`,
`MEDIUMINT`, `INT`, `INTEGER`, `BIGINT`, and their supported `UNSIGNED` forms
when the stored value fits MyLite's current signed 64-bit physical integer
storage.

## Deferred Surface

This slice intentionally does not support:

- decimal/floating seconds, fractional-second return text, string seconds for
  `SEC_TO_TIME()`, hex/bit literals, parameters, variables, subqueries, or
  arbitrary expressions;
- numeric temporal literals for `TIME_TO_SEC()`, compact numeric temporal
  strings, date-only string coercion, relaxed temporal delimiters, fractional
  seconds, time-zone suffixes, trailing garbage, or broad SQL-mode-sensitive
  temporal coercion;
- `TIME_TO_SEC()` or `SEC_TO_TIME()` inside `WHERE`, `ORDER BY`, `GROUP BY`,
  `HAVING`, DML assignments, defaults, generated columns, constraints, joins,
  CTEs, or SQLite pass-through;
- use as aggregate, window, stored, user-defined, or loadable functions;
- protocol metadata parity beyond existing result-object conventions.

## Grammar

MyLite adds these independently authored Lemon-shape productions:

```lemon
expression(A) ::= TIME_TO_SEC(T) LPAREN expression(V) RPAREN(R).
expression(A) ::= SEC_TO_TIME(T) LPAREN expression(V) RPAREN(R).
```

Unsupported arities are represented explicitly so the runtime can report the
verified native-function argument-count diagnostic:

```lemon
expression(A) ::= TIME_TO_SEC(T) LPAREN RPAREN(R).
expression(A) ::= TIME_TO_SEC(T) LPAREN expression(V) COMMA function_argument_list(L) RPAREN(R).
expression(A) ::= SEC_TO_TIME(T) LPAREN RPAREN(R).
expression(A) ::= SEC_TO_TIME(T) LPAREN expression(V) COMMA function_argument_list(L) RPAREN(R).
```

The function names remain admitted as ordinary keyword identifiers where the
existing grammar accepts nonreserved keyword identifiers:

```lemon
identifier(A) ::= TIME_TO_SEC(T).
identifier(A) ::= SEC_TO_TIME(T).
```

Semantic narrowing is enforced by the analyzer/runtime:

```lemon
time_to_sec_expr ::= TIME_TO_SEC LPAREN supported_time_to_sec_value RPAREN.
sec_to_time_expr ::= SEC_TO_TIME LPAREN supported_sec_to_time_value RPAREN.

supported_time_to_sec_value ::= NULL.
supported_time_to_sec_value ::= canonical_time_string.
supported_time_to_sec_value ::= canonical_datetime_string.
supported_time_to_sec_value ::= descriptor_date_column.
supported_time_to_sec_value ::= descriptor_time_column.
supported_time_to_sec_value ::= descriptor_datetime_column.
supported_time_to_sec_value ::= descriptor_timestamp_column.
supported_time_to_sec_value ::= descriptor_nonbinary_string_column.

supported_sec_to_time_value ::= NULL.
supported_sec_to_time_value ::= signed_integer_literal.
supported_sec_to_time_value ::= boolean_literal.
supported_sec_to_time_value ::= descriptor_integer_column.
```

## Conversion And Result Semantics

`TIME_TO_SEC()`:

- `NULL` returns `NULL`.
- Canonical time string input returns `hour * 3600 + minute * 60 + second`,
  negated when the input has a leading `-`.
- Canonical datetime string input ignores the date part and uses the time
  suffix. The time suffix must be valid for a datetime value, so hours outside
  `00..23` are invalid even though standalone `TIME` strings may use larger
  hours.
- Descriptor `DATE` input returns `0` for non-`NULL` rows.
- Invalid or currently unsupported string input returns `NULL` and appends
  warning `1292 / 22007` with message prefix
  `Truncated incorrect time value:`.
- The result is a signed integer or `NULL`.

`SEC_TO_TIME()`:

- `NULL` returns `NULL`.
- `FALSE` is `0`; `TRUE` is `1`.
- In-range integer seconds are formatted as `HH:MM:SS`, preserving a leading
  `-` for negative values. Hours have at least two digits and may have three
  digits for values at or near MySQL's maximum time range.
- Absolute values above `3020399` seconds are clipped to `838:59:59` or
  `-838:59:59` and append warning `1292 / 22007` with message prefix
  `Truncated incorrect time value:`.
- The result is text or `NULL`.

Successful supported calls that do not clip or encounter invalid temporal text
append no warnings. `DO` returns no row result set and uses existing `DO`
result conventions.

## Physical SQLite Handling

For row-backed `TIME_TO_SEC()`, MyLite generates a descriptor-built expression
of the form:

```sql
_mylite_temporal_extract(<descriptor_column>, ?, ?)
```

The second argument is bound to `time_to_sec`; the third argument is bound to
the descriptor input kind. The callback returns a SQLite integer or `NULL`.

For row-backed `SEC_TO_TIME()`, MyLite generates:

```sql
_mylite_sec_to_time(<descriptor_column>)
```

The callback reads the SQLite integer value, applies MyLite clipping and
warning behavior, and returns text or `NULL`.

Generated SQL uses quoted physical descriptor columns and stable physical table
names. User SQL text and user literals are never interpolated into generated
SQLite SQL. Scalar literal conversion happens before SQLite execution; row
filtering, sorting, and limiting stay with SQLite.

## Diagnostics

Supported warnings:

- `1292 / 22007`, warning, `Truncated incorrect time value: '<value>'` for
  invalid admitted `TIME_TO_SEC()` text inputs.
- `1292 / 22007`, warning, `Truncated incorrect time value: '<value>'` for
  clipped admitted `SEC_TO_TIME()` integer inputs.

Errors:

- `1582 / 42000`, `Incorrect parameter count in the call to native function
  '<name>'` for zero-argument or multi-argument calls.
- Deterministic MyLite unsupported-feature diagnostics for unsupported scalar
  argument kinds, unsupported descriptor argument kinds, table-qualified
  expression arguments, nested expressions, parameters, variables, subqueries,
  decimal/floating/hex/bit literals, or unsupported row-scalar envelopes.
- Existing descriptor diagnostics for unknown tables, unknown columns, missing
  default schema, reserved names, and unsupported object kinds.
- Existing allocation and physical SQLite failure diagnostics.

## Compatibility Documentation

`COMPATIBILITY.md` and `docs/compatibility/functions-temporal.md` mark
`TIME_TO_SEC()` and `SEC_TO_TIME()` as partially supported with the exact
limited contexts, arguments, warnings, and deferred coercions. The query
expression and literal/conversion docs are updated only for this admitted
projection surface.

## Verification

- Add a MySQL 8.4.9 expectation script covering scalar, `DUAL`, `DO`,
  descriptor-backed, warning, clipping, arity, and deferred accepted inputs.
- Add C parser tests for function parsing, whitespace, aliases, identifier use,
  and arity nodes.
- Add C runtime tests for scalar values, row-scalar values, warnings, row-count
  state, reopen persistence, unsupported arguments, and current envelope
  behavior.
- Run focused parser/runtime tests, the MySQL expectation script, and the full
  `cmake --workflow --preset check` workflow before committing.
