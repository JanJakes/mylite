# Baseline UNIX_TIMESTAMP Function

## Goal

Add a narrow `UNIX_TIMESTAMP()` slice for common application temporal
projection:

```sql
SELECT UNIX_TIMESTAMP()
SELECT UNIX_TIMESTAMP(created_at) FROM posts
```

This phase extends MyLite's scalar and row-scalar projection paths. It is not a
general temporal coercion engine, fractional temporal implementation,
time-zone-table subsystem, or expression rewrite for predicates, defaults, or
DML assignment values.

## Sources

- Official MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Official MySQL 8.4 Reference Manual, date and time literals:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-literals.html>
- Official MySQL 8.4 Reference Manual, function-name parsing and resolution:
  <https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html>
- Existing MyLite temporal and row-scalar designs:
  - `docs/specs/baseline-current-date-time-functions/specs.md`
  - `docs/specs/baseline-time-zone-system-variable/specs.md`
  - `docs/specs/baseline-temporal-extract-functions/specs.md`
  - `docs/specs/baseline-date-format-function/specs.md`
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_unix_timestamp_function_expectations.sh`.

The MyLite grammar and implementation are independently authored from official
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. Do not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `UNIX_TIMESTAMP()` returns the current statement Unix epoch seconds. It
  observes `SET timestamp` and is not shifted by `@@time_zone`.
- `UNIX_TIMESTAMP(date)` treats string, `DATE`, and `DATETIME` arguments as
  session-local temporal values and converts them to UTC epoch seconds using
  the current session `time_zone` offset.
- A `TIMESTAMP` column argument returns the stored internal timestamp directly.
  In MySQL, changing the session time zone changes the displayed `TIMESTAMP`
  text, but `UNIX_TIMESTAMP(ts_col)` keeps returning the same internal epoch.
- `NULL` returns `NULL`.
- Complete temporal arguments before `'1970-01-01 00:00:01'` UTC return `0`
  without warnings. Observed 64-bit MySQL 8.4.9 on the local `aarch64` runtime
  accepts values through `3001-01-18 23:59:59` UTC and returns `0` for later
  values without warnings.
- Invalid non-temporal strings such as `'bad'` return `0.000000` and append
  warning `1292 / 22007`, `Incorrect datetime value: 'bad'`.
- Full zero dates such as `'0000-00-00'` return `0` and append warning
  `1292 / 22007`, while partial-zero forms observed in this phase return `0`
  without warnings.
- Successful supported statements produce no warnings. A successful scalar
  `SELECT` makes a following `ROW_COUNT()` return `-1`; a successful `DO`
  makes it return `0`.
- MySQL also accepts compact numeric temporal arguments and fractional-second
  arguments. MyLite defers those forms because they require broader temporal
  numeric coercion and decimal result typing.

## Supported Surface

MyLite supports:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` using the existing row envelope:
  `FROM table [AS alias]`, optional existing `WHERE`, single descriptor-column
  `ORDER BY`, and existing `LIMIT`;
- `UNIX_TIMESTAMP()` with no arguments;
- `UNIX_TIMESTAMP(value)` with exactly one argument;
- `value` as:
  - `NULL`;
  - a single- or double-quoted string literal containing canonical
    `YYYY-MM-DD` or `YYYY-MM-DD HH:MM:SS`;
  - a full zero-date string for MySQL-compatible `0` plus warning behavior;
  - a descriptor column in a table-backed row-scalar `SELECT` whose descriptor
    family is `DATE`, `DATETIME`, or `TIMESTAMP`;
- valid complete date/datetime values in the current storage baseline range,
  with the Unix timestamp result clipped to MySQL's observed runtime range;
- output integer text/`NULL` values through existing result APIs for supported
  in-range values;
- MySQL-compatible warning count and `SHOW WARNINGS` entries for the admitted
  invalid string and full-zero-date cases.

The row-backed descriptor argument scope deliberately excludes string-family
columns in this phase. MySQL's display shape for `UNIX_TIMESTAMP(varchar_col)`
uses decimal-looking output even for whole-second values, while string literals
without fractional seconds return integer-looking output. MyLite will add that
surface with broader expression typing rather than returning subtly wrong
metadata or display values.

## Deferred Surface

This slice intentionally does not support:

- compact numeric temporal arguments such as `20151113102019`, two-digit year
  numeric coercion, booleans, bit/hex literals, decimal or float values,
  parameters, variables, subqueries, or arbitrary expressions as arguments;
- fractional-second input and decimal results;
- string-family descriptor columns, binary string columns, `TIME` columns,
  `YEAR` columns, or non-temporal descriptors as arguments;
- named time zones, time-zone tables, daylight-saving transitions, or `SYSTEM`
  time-zone behavior beyond the existing fixed-offset `@@time_zone` slice;
- use in `WHERE`, `ORDER BY`, `GROUP BY`, `HAVING`, DML assignments, defaults,
  generated columns, indexes, constraints, joins, CTEs, or arbitrary SQLite
  pass-through;
- `FROM_UNIXTIME()`, `UTC_TIMESTAMP()`, `SYSDATE()`, `TO_SECONDS()`, or broader
  temporal conversion functions.

## Grammar

MyLite adds these parser productions:

```lemon
expression(A) ::= UNIX_TIMESTAMP(T) LPAREN RPAREN(R).
expression(A) ::= UNIX_TIMESTAMP(T) LPAREN expression(V) RPAREN(R).
```

Wrong arities produce an argument-count AST node so runtime can return MySQL's
native-function parameter-count diagnostic:

```lemon
expression(A) ::=
    UNIX_TIMESTAMP(T) LPAREN expression(V) COMMA function_argument_list(E) RPAREN(R).
```

`UNIX_TIMESTAMP` is admitted as an identifier where MyLite admits ordinary
nonreserved identifiers:

```lemon
identifier(A) ::= UNIX_TIMESTAMP(T).
```

Analyzer/runtime acceptance is narrower:

```lemon
unix_timestamp_expr(A) ::= UNIX_TIMESTAMP LPAREN RPAREN.
unix_timestamp_expr(A) ::= UNIX_TIMESTAMP LPAREN unix_timestamp_value(V) RPAREN.

unix_timestamp_value(A) ::= NULL(T).
unix_timestamp_value(A) ::= string_literal(T).
unix_timestamp_value(A) ::= descriptor_date_column(C).
unix_timestamp_value(A) ::= descriptor_datetime_column(C).
unix_timestamp_value(A) ::= descriptor_timestamp_column(C).
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning:

1. Detect no-source/`DUAL` scalar expressions and row-scalar projection
   attempts containing `UNIX_TIMESTAMP()`.
2. Resolve row sources through the existing selected/default schema policy.
3. Resolve descriptor column arguments through MyLite catalog descriptors, not
   SQLite schema text.
4. Decode string literal arguments using the current statement SQL mode,
   including `ANSI_QUOTES` and `NO_BACKSLASH_ESCAPES`.
5. Reject unsupported argument kinds before generated SQLite SQL exists.
6. Generate SQLite projection SQL over stable physical table names and quoted
   physical column names. String literals and internal discriminators are bound
   parameters.
7. Use a MyLite-owned SQLite scalar function for table-backed row execution so
   SQLite can keep scanning, filtering, ordering, and limiting without MyLite
   materializing source rows.

Scalar evaluation:

1. No-argument `UNIX_TIMESTAMP()` formats the statement epoch from the existing
   statement context helper: session `timestamp` override first, then active
   statement time, then host time fallback.
2. `NULL` returns `NULL`.
3. `DATE` and `DATETIME` values are interpreted in the current session
   fixed-offset time zone and converted to UTC epoch seconds.
4. `TIMESTAMP` descriptor values are interpreted as MyLite's fixed-UTC
   timestamp storage value and are not adjusted by the current session offset.
5. Epoch results outside the observed MySQL range return `0`.
6. Invalid non-temporal string inputs return `0.000000` and append warning
   `1292 / 22007`, `Incorrect datetime value: 'value'`.
7. Full zero-date strings return `0` and append the same warning.

The row-backed generated SQL shape is:

```sql
_mylite_unix_timestamp(value_expr, input_kind_expr)
```

`input_kind_expr` is a bound MyLite-internal discriminator such as `string`,
`date`, `datetime`, or `timestamp`. It is not user-visible.

## Ownership Boundaries

- Public API: unchanged. Successful statements use existing `mylite_execute()`
  and result APIs.
- Statement context: owns the statement-time snapshot used by no-argument
  `UNIX_TIMESTAMP()`.
- Lexer/parser/AST: add `UNIX_TIMESTAMP` token handling, AST node kinds, and
  argument-count nodes. Parser source spans remain result-label authority.
- Analyzer/planner: resolve descriptor arguments and reject unsupported shapes.
  It creates bound planned values and internal function calls; it does not ask
  SQLite metadata which columns exist.
- Catalog: read-only descriptor authority. `UNIX_TIMESTAMP()` must not mutate
  descriptor rows, descriptor versions, descriptor caches, catalog generation,
  or `sqlite_schema_generation`.
- Result builder: returns integer text, decimal warning text for the admitted
  invalid string case, or `NULL`, using aliases or source spans for labels.
- Storage/VFS/file format: unchanged. `.mylite` preamble and shifted SQLite
  payload invariants are preserved.
- SQLite: use the public scalar-function registration API for
  `_mylite_unix_timestamp`. No SQLite fork patch is required.

## Diagnostics

Required diagnostics:

- wrong arity: `1582 / 42000`, `Incorrect parameter count in the call to
  native function 'UNIX_TIMESTAMP'`;
- invalid non-temporal string and full zero-date inputs: warning
  `1292 / 22007`, `Incorrect datetime value: 'value'`;
- unknown descriptor column: existing MySQL-compatible unknown-column
  diagnostic;
- unsupported argument literal/expression:
  `UNIX_TIMESTAMP() supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor columns, and NULL`;
- unsupported string-family descriptor column:
  `UNIX_TIMESTAMP() does not yet support string descriptor columns`;
- unsupported fractional input:
  `UNIX_TIMESTAMP() does not yet support fractional seconds`;
- unsupported NUL-containing literal:
  `UNIX_TIMESTAMP() literals do not support NUL bytes`;
- allocation failure: existing `MYLITE_NOMEM` diagnostic behavior;
- physical SQLite failure: existing wrapped SQLite diagnostics;
- public API misuse: no public API changes.

Supported in-range statements return `warning_count == 0`.

## Performance And Storage

No-source scalar evaluation formats one stack-buffer or owned scalar result per
output cell. Row-backed evaluation lowers to a SQLite scalar callback over the
generated projection expression, so SQLite remains responsible for table scan,
filter, sort, and limit execution. MyLite does not materialize source rows in
memory, does not add indexes, and does not require a SQLite fork patch.

## Tests

Add fast C tests under `packages/libmylite/tests/` and MySQL-runtime expectation
coverage for:

- no-argument `UNIX_TIMESTAMP()` with deterministic `SET timestamp`;
- fixed-offset `@@time_zone` behavior for string/date/datetime values;
- `NULL`, lower bound, upper observed MySQL range, out-of-range values,
  invalid strings, full zero dates, and partial-zero date return behavior;
- `FROM DUAL`, `DO`, labels, warning counts, `SHOW WARNINGS`, and `ROW_COUNT()`;
- row-scalar projection over descriptor `DATE`, `DATETIME`, and `TIMESTAMP`
  columns, including reopen persistence and existing `WHERE` / `ORDER BY` /
  `LIMIT` row envelope reuse;
- deterministic diagnostics for unknown columns, wrong arity, numeric compact
  literals, fractional input, string descriptor columns, non-temporal
  descriptors, parameters, subqueries, and nested expression arguments;
- unchanged catalog generation, SQLite schema generation, and `.mylite`
  preamble bytes for read-only projection.

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/functions-temporal.md`;
- `docs/compatibility/sql-query-expressions.md` only if the row-scalar surface
  needs an explicit cross-reference change;
- `docs/compatibility/type-system-literals-conversion.md` only for the exact
  admitted temporal string literal surface.
