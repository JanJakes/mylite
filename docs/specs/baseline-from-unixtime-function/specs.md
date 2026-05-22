# Baseline FROM_UNIXTIME Function

## Summary

This phase adds a narrow MySQL-compatible `FROM_UNIXTIME()` slice:

```sql
FROM_UNIXTIME(unix_timestamp)
```

The supported surface covers no-source scalar `SELECT`, `SELECT ... FROM DUAL`,
`DO`, and single-table row-scalar `SELECT` projection over supported integer
seconds. It pairs with the existing `UNIX_TIMESTAMP()` and session
`time_zone` slices without adding a general temporal coercion engine, formatted
two-argument `FROM_UNIXTIME()`, decimal/fractional seconds, or arbitrary
expression evaluation.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Existing MyLite specs:
  - `docs/specs/baseline-unix-timestamp-function/specs.md`
  - `docs/specs/baseline-time-zone-system-variable/specs.md`
  - `docs/specs/baseline-current-date-time-functions/specs.md`
  - `docs/specs/baseline-row-scalar-expression-projection/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_from_unixtime_function_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 behavior, public SQLite
APIs, and existing MyLite code. It does not copy MySQL, MariaDB, Percona,
SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `FROM_UNIXTIME(0)` returns `1970-01-01 00:00:00` in `+00:00`.
- `FROM_UNIXTIME(1)` returns `1970-01-01 00:00:01` in `+00:00`.
- `FROM_UNIXTIME(951782400)` returns `2000-02-29 00:00:00` in `+00:00`.
- `FROM_UNIXTIME(1447430881)` returns `2015-11-13 16:08:01` in `+00:00`.
- `FROM_UNIXTIME(NULL)` returns `NULL`.
- `FROM_UNIXTIME(TRUE)` returns `1970-01-01 00:00:01`; `FROM_UNIXTIME(FALSE)`
  returns `1970-01-01 00:00:00`.
- Session `time_zone` offsets affect the visible result. With
  `time_zone = '+02:30'`, `FROM_UNIXTIME(0)` returns
  `1970-01-01 02:30:00`; with `time_zone = '-06:00'`, it returns
  `1969-12-31 18:00:00`.
- `FROM_UNIXTIME(-1)` returns `NULL` with no warning.
- `FROM_UNIXTIME(32536771199)` returns `3001-01-18 23:59:59` in `+00:00`.
- `FROM_UNIXTIME(32536771200)` returns `NULL` with no warning on the local
  64-bit MySQL 8.4.9 runtime.
- Successful supported calls produce no warnings. A preceding scalar `SELECT`
  makes `ROW_COUNT()` return `-1`; a supported `DO` makes it return `0`.
- `FROM_UNIXTIME()` and `FROM_UNIXTIME(a, b, c)` fail with native-function
  argument-count error `1582 / 42000`.
- MySQL also accepts `FROM_UNIXTIME(unix_timestamp, format)`, decimal and float
  seconds, and numeric strings. This slice defers those forms because they
  require broader result typing, decimal/fractional temporal formatting, and
  numeric-string coercion.

## Ownership Boundary

- Public API: unchanged. `mylite_execute()` and existing result APIs expose
  scalar rows, `DO` non-row results, diagnostics, warning counts, and row-count
  conventions.
- Session and statement context: session `time_zone` supplies the offset used
  for visible datetime rendering. No new durable or statement-scoped public
  state is added.
- Lexer/parser/AST: `FROM_UNIXTIME` becomes a function token, with arity nodes
  for native argument-count errors. The token remains admitted as an ordinary
  keyword identifier where MyLite already permits that pattern.
- Analyzer/planner/runtime: admitted integer/boolean/`NULL` arguments are
  converted by MyLite-owned code. Descriptor-column arguments are resolved from
  MyLite catalog descriptors, not SQLite metadata.
- Catalog: read-only. `FROM_UNIXTIME()` does not mutate schemas, table or
  column descriptors, descriptor versions, descriptor caches, catalog
  generation, or `sqlite_schema_generation`.
- Result builder: returns visible datetime text or `NULL` using existing result
  labels and ownership conventions.
- Storage/VFS/file format: unchanged. `.mylite` preamble and shifted SQLite
  payload invariants are preserved.
- SQLite: row-backed scans, filtering, ordering, and limiting remain delegated
  to SQLite. MyLite uses a public SQLite scalar callback for per-row conversion.
  No SQLite fork patch is required.

## Supported SQL

No-source and `DUAL` projection:

```sql
SELECT FROM_UNIXTIME(unix_timestamp)
SELECT FROM_UNIXTIME(unix_timestamp) FROM DUAL
```

`DO` execution:

```sql
DO FROM_UNIXTIME(unix_timestamp)
```

Single-table row-scalar projection:

```sql
SELECT FROM_UNIXTIME(column_name)
FROM table_name [WHERE predicate] [ORDER BY column_name [ASC | DESC]] [LIMIT row_count]
```

The existing single-table row-scalar envelope supplies target table resolution,
predicate resolution, descriptor-column projection, ordering, and limiting.

`unix_timestamp` is one of:

- `NULL`;
- a decimal integer literal in the signed 64-bit range with optional unary `+`
  or `-`;
- `TRUE` or `FALSE`;
- a row-scalar descriptor column from an integer-family descriptor whose stored
  physical value is in MyLite's current signed 64-bit `INTEGER` storage range.

## Deferred Surface

This slice intentionally does not support:

- `FROM_UNIXTIME(unix_timestamp, format)`;
- decimal, fixed, or approximate seconds and fractional-second result text;
- string seconds, hex/bit literals, parameters, variables, subqueries, or
  arbitrary expressions;
- descriptor `DECIMAL`, approximate, string, temporal, `YEAR`, binary string,
  `BIT`, `ENUM`, `SET`, JSON, or spatial columns as arguments;
- use inside `WHERE`, `ORDER BY`, `GROUP BY`, `HAVING`, DML assignments,
  defaults, generated columns, indexes, constraints, joins, CTEs, or SQLite
  pass-through;
- named time zones, daylight-saving behavior, time-zone tables, or protocol
  metadata parity.

## Grammar

MyLite adds these independently authored Lemon-shape productions:

```lemon
expression(A) ::= FROM_UNIXTIME(T) LPAREN expression(V) RPAREN(R).
expression(A) ::= FROM_UNIXTIME(T) LPAREN expression(V) COMMA expression(F) RPAREN(R).
```

Unsupported arities are represented explicitly:

```lemon
expression(A) ::= FROM_UNIXTIME(T) LPAREN RPAREN(R).
expression(A) ::=
    FROM_UNIXTIME(T) LPAREN expression(V) COMMA expression(F) COMMA
    function_argument_list(E) RPAREN(R).
```

The function name remains admitted as a keyword identifier where appropriate:

```lemon
identifier(A) ::= FROM_UNIXTIME(T).
```

Semantic narrowing is enforced by the analyzer/runtime:

```lemon
from_unixtime_expr ::= FROM_UNIXTIME LPAREN supported_epoch_seconds RPAREN.

supported_epoch_seconds ::= NULL.
supported_epoch_seconds ::= signed_integer_literal.
supported_epoch_seconds ::= boolean_literal.
supported_epoch_seconds ::= descriptor_integer_column.
```

The two-argument grammar is parsed so MyLite can return a deterministic
unsupported-format diagnostic instead of a syntax error. It remains outside the
supported execution surface.

## Conversion And Result Semantics

- `NULL` returns `NULL`.
- `FALSE` is `0`; `TRUE` is `1`.
- In-range integer seconds are interpreted as UTC Unix epoch seconds, shifted by
  the current session fixed-offset `time_zone`, and formatted as
  `YYYY-MM-DD HH:MM:SS`.
- Negative seconds return `NULL` with no warning.
- Seconds greater than `32536771199` return `NULL` with no warning on the
  current MySQL 8.4.9-compatible 64-bit baseline.
- Supported in-range calls append no warnings.
- Successful scalar `SELECT` follows existing result-set conventions and makes
  subsequent `ROW_COUNT()` report `-1`.
- Successful `DO` returns no row result set and makes subsequent `ROW_COUNT()`
  report `0`.

## Physical SQLite Handling

For row-backed `FROM_UNIXTIME()`, MyLite generates:

```sql
_mylite_from_unixtime(<descriptor_or_bound_integer>)
```

The callback reads an SQLite integer or `NULL`, applies MyLite-owned range and
session-time-zone conversion, and returns text or `NULL`. Generated SQL uses
quoted physical descriptor columns and stable physical table names. User SQL
text and user literals are never interpolated into generated SQLite SQL.

## Diagnostics

Errors:

- `1582 / 42000`, `Incorrect parameter count in the call to native function
  'FROM_UNIXTIME'` for zero-argument or three-or-more-argument calls.
- Deterministic MyLite unsupported-feature diagnostics for the deferred
  two-argument format form, string/decimal/float/hex/bit arguments, table-
  qualified expression arguments, nested expressions, variables, parameters,
  subqueries, or unsupported row-scalar envelopes.
- Existing descriptor diagnostics for unknown tables, unknown columns, missing
  default schema, reserved names, and unsupported object kinds.
- Existing allocation and physical SQLite failure diagnostics.

Supported calls do not append warnings.

## Verification

- Add a MySQL 8.4.9 expectation script covering scalar, `DUAL`, `DO`, time-zone
  offsets, range boundaries, row-backed descriptor projection, arity, and
  deferred accepted MySQL forms.
- Add parser tests for function parsing, whitespace, aliases, identifier use,
  one/two/wrong arity nodes.
- Add C runtime tests for scalar values, row-backed values, `DO`, row count,
  warning count, reopen persistence, unsupported arguments, and diagnostics.
- Run focused parser/runtime tests, the MySQL expectation script, and
  `cmake --workflow --preset check` before committing.
