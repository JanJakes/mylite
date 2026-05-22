# Baseline TIMESTAMPDIFF Function

## Goal

Add a narrow `TIMESTAMPDIFF(unit, expr1, expr2)` slice for common temporal
projection:

```sql
SELECT TIMESTAMPDIFF(DAY, created_at, updated_at) FROM posts;
```

This phase extends MyLite's no-source scalar, `DUAL`, `DO`, and single-table
row-scalar `SELECT` paths. It is not a general temporal expression engine and
does not add expression predicates, expression ordering, grouping, generated
columns, defaults, DML assignment expressions, fractional temporal arithmetic,
or relaxed temporal coercion.

## Sources

- Official MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Official MySQL 8.4 Reference Manual, function-name parsing and resolution:
  <https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html>
- Existing MyLite temporal and row-scalar designs:
  - `docs/specs/baseline-datediff-function/specs.md`
  - `docs/specs/baseline-date-format-function/specs.md`
  - `docs/specs/baseline-temporal-extract-functions/specs.md`
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_timestampdiff_function_expectations.sh`.

This specification is independently authored from official documentation,
observed MySQL 8.4.9 behavior, public SQLite APIs, and existing MyLite code. It
does not copy MySQL, MariaDB, Percona, SQLite implementation internals, or
restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `TIMESTAMPDIFF(unit, expr1, expr2)` returns `expr2 - expr1` as an integer in
  the requested unit.
- MySQL accepts `YEAR`, `QUARTER`, `MONTH`, `WEEK`, `DAY`, `HOUR`, `MINUTE`,
  `SECOND`, and bare `MICROSECOND` units. It also accepts `SQL_TSI_` aliases
  for `YEAR`, `QUARTER`, `MONTH`, `WEEK`, `DAY`, `HOUR`, `MINUTE`, and
  `SECOND` in MySQL 8.4.9. `SQL_TSI_MICROSECOND` is a syntax error in the
  observed runtime.
- A date-only value is treated as a datetime at `00:00:00`.
- `YEAR`, `QUARTER`, and `MONTH` count whole calendar boundaries. For example,
  `TIMESTAMPDIFF(YEAR, '2003-02-02', '2004-02-01')` returns `0`,
  `TIMESTAMPDIFF(YEAR, '2003-02-02', '2004-02-02')` returns `1`,
  `TIMESTAMPDIFF(MONTH, '2003-01-31', '2003-02-28')` returns `0`, and
  `TIMESTAMPDIFF(MONTH, '2003-01-31', '2003-03-01')` returns `1`.
- `WEEK`, `DAY`, `HOUR`, `MINUTE`, and `SECOND` count whole elapsed units and
  truncate toward zero for partial units, including negative partial units.
- `NULL` temporal arguments return `NULL`.
- Argument validation is left-to-right and stops when a `NULL` argument is
  encountered. `TIMESTAMPDIFF(DAY, NULL, 'bad')` returns `NULL` without a
  warning, while `TIMESTAMPDIFF(DAY, 'bad', NULL)` returns `NULL` with warning
  `1292 / 22007`.
- Invalid non-temporal strings, full-zero dates such as `'0000-00-00'`, and
  partial-zero dates such as `'2001-11-00'` return `NULL` and append warning
  `1292 / 22007` with text beginning `Incorrect datetime value:`.
- Year-zero dates with nonzero month and day, such as `'0000-01-02'`, are
  accepted for day and second arithmetic.
- Stored `DATE`, `DATETIME`, and `TIMESTAMP` descriptor values that are
  full-zero or partial-zero return `NULL` without adding a fresh warning in the
  observed row-backed projection path; string descriptor values containing
  invalid temporal text warn like string literals.
- `TIMESTAMPDIFF` accepts whitespace before `(` in default SQL mode and is not
  one of MySQL's whitespace-sensitive special function names.
- `TIMESTAMPDIFF` is usable as an unquoted table identifier outside
  function-call contexts.
- Wrong argument counts, quoted units, invalid units, and composite interval
  units such as `DAY_HOUR` fail with parse diagnostics (`1064 / 42000`) in
  MySQL 8.4.9.
- Successful supported statements produce no warnings. A successful scalar
  `SELECT` makes a following `ROW_COUNT()` return `-1`; a successful `DO`
  makes it return `0`.

## Supported Surface

MyLite supports:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` using the existing row envelope:
  `FROM table [AS alias]`, optional existing `WHERE`, single descriptor-column
  `ORDER BY`, and existing `LIMIT`;
- `TIMESTAMPDIFF(unit, value1, value2)` with exactly one unit and two temporal
  arguments;
- supported runtime units:
  - `YEAR`, `SQL_TSI_YEAR`;
  - `QUARTER`, `SQL_TSI_QUARTER`;
  - `MONTH`, `SQL_TSI_MONTH`;
  - `WEEK`, `SQL_TSI_WEEK`;
  - `DAY`, `SQL_TSI_DAY`;
  - `HOUR`, `SQL_TSI_HOUR`;
  - `MINUTE`, `SQL_TSI_MINUTE`;
  - `SECOND`, `SQL_TSI_SECOND`;
- bare `MICROSECOND` syntax, rejected deterministically as unsupported until
  fractional temporal parsing is implemented;
- each temporal value as:
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
limiting. MyLite does not materialize the source row set to compute
`TIMESTAMPDIFF()`.

## Deferred Surface

This slice intentionally does not support:

- runtime `MICROSECOND` results, fractional seconds, or
  `SQL_TSI_MICROSECOND`;
- numeric temporal literals, booleans, bit/hex literals, decimal or float
  values, parameters, variables, subqueries, or arbitrary expressions as
  temporal arguments;
- compact numeric temporal text, two-digit years, locale or time-zone
  coercion, relaxed temporal strings, or broader SQL-mode-sensitive temporal
  parsing;
- `TIME` descriptor arguments or time-only strings;
- composite interval units such as `YEAR_MONTH`, `DAY_HOUR`, or
  `SECOND_MICROSECOND`;
- use in `WHERE`, `ORDER BY`, `GROUP BY`, `HAVING`, DML assignments, defaults,
  generated columns, indexes, constraints, joins, CTEs, or arbitrary SQLite
  pass-through;
- broader temporal functions such as `TIMEDIFF()`, `TIMESTAMPADD()`,
  `TO_DAYS()`, `TO_SECONDS()`, or full `EXTRACT()` parity.

## Grammar

MyLite adds this parser production:

```lemon
expression(A) ::=
    TIMESTAMPDIFF(T) LPAREN timestampdiff_unit(U) COMMA expression(B)
    COMMA expression(C) RPAREN(R).
```

`timestampdiff_unit` is deliberately narrower than general `INTERVAL` units:

```lemon
timestampdiff_unit(A) ::= YEAR(T).
timestampdiff_unit(A) ::= SQL_TSI_YEAR(T).
timestampdiff_unit(A) ::= QUARTER(T).
timestampdiff_unit(A) ::= SQL_TSI_QUARTER(T).
timestampdiff_unit(A) ::= MONTH(T).
timestampdiff_unit(A) ::= SQL_TSI_MONTH(T).
timestampdiff_unit(A) ::= WEEK(T).
timestampdiff_unit(A) ::= SQL_TSI_WEEK(T).
timestampdiff_unit(A) ::= DAY(T).
timestampdiff_unit(A) ::= SQL_TSI_DAY(T).
timestampdiff_unit(A) ::= HOUR(T).
timestampdiff_unit(A) ::= SQL_TSI_HOUR(T).
timestampdiff_unit(A) ::= MINUTE(T).
timestampdiff_unit(A) ::= SQL_TSI_MINUTE(T).
timestampdiff_unit(A) ::= SECOND(T).
timestampdiff_unit(A) ::= SQL_TSI_SECOND(T).
timestampdiff_unit(A) ::= MICROSECOND(T).
```

Wrong arities, quoted units, unknown units, `SQL_TSI_MICROSECOND`, and
composite units are not admitted by this grammar. They therefore produce the
existing parse-error diagnostics, matching the MySQL 8.4.9 category for those
forms.

`TIMESTAMPDIFF` is admitted as an identifier where MyLite admits ordinary
nonreserved identifiers:

```lemon
identifier(A) ::= TIMESTAMPDIFF(T).
```

Analyzer/runtime acceptance is narrower:

```lemon
timestampdiff_expr(A) ::=
    TIMESTAMPDIFF LPAREN supported_timestampdiff_unit(B) COMMA
    timestampdiff_value(C) COMMA timestampdiff_value(D) RPAREN.

timestampdiff_value(A) ::= NULL(T).
timestampdiff_value(A) ::= string_literal(T).
timestampdiff_value(A) ::= descriptor_date_column(C).
timestampdiff_value(A) ::= descriptor_datetime_column(C).
timestampdiff_value(A) ::= descriptor_timestamp_column(C).
timestampdiff_value(A) ::= descriptor_string_column(C).
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning:

1. Detect no-source/`DUAL` scalar expressions and row-scalar projection
   attempts containing `TIMESTAMPDIFF()`.
2. Resolve row sources through the existing selected/default schema policy.
3. Resolve descriptor column arguments through MyLite catalog descriptors, not
   SQLite schema text.
4. Decode string literal arguments using the current statement SQL mode,
   including `ANSI_QUOTES` and `NO_BACKSLASH_ESCAPES`.
5. Reject unsupported argument kinds and the deferred `MICROSECOND` runtime
   unit before generated SQLite SQL exists.
6. Generate SQLite projection SQL over stable physical table names and quoted
   physical column names. String literals and internal discriminators are bound
   parameters.
7. Use a MyLite-owned SQLite scalar function for table-backed row execution.

Evaluation:

1. Evaluate the first temporal argument. If it is SQL `NULL`, return `NULL`
   immediately.
2. If the first argument is non-`NULL` and invalid, append warning
   `1292 / 22007`, `Incorrect datetime value: 'value'`, and return `NULL`
   without evaluating the second argument.
3. Evaluate the second temporal argument. If it is SQL `NULL`, return `NULL`.
4. If the second argument is non-`NULL` and invalid, append warning
   `1292 / 22007`, `Incorrect datetime value: 'value'`, and return `NULL`.
5. Date-only values use midnight. Datetime and timestamp values use their date
   and time portions.
6. Full-zero and partial-zero dates are invalid for `TIMESTAMPDIFF()`.
   Year-zero complete dates are valid, except invalid calendar days such as
   `'0000-02-29'`.
7. Valid arguments are converted to internal datetime values using the verified
   Gregorian behavior.
8. The signed difference is `right - left`.
9. `SECOND`, `MINUTE`, `HOUR`, `DAY`, and `WEEK` results use whole elapsed
   seconds divided by the unit size, truncated toward zero.
10. `MONTH` counts whole month boundaries, adjusted by day and time-of-day.
11. `QUARTER` is the whole-month result divided by three, truncated toward
    zero.
12. `YEAR` is the whole-month result divided by twelve, truncated toward zero.

The row-backed generated SQL shape is:

```sql
_mylite_timestampdiff(unit, left_value, left_kind, right_value, right_kind)
```

`unit`, `left_kind`, and `right_kind` are bound MyLite-internal discriminators
such as `day`, `string`, `date`, `datetime`, or `timestamp`. They are not
user-visible.

## Ownership Boundaries

- Public API: unchanged. Successful statements use existing `mylite_execute()`
  and result APIs.
- Statement context: unchanged. Diagnostics and row-count behavior remain
  statement-owned.
- Lexer/parser/AST: add `TIMESTAMPDIFF` token handling, AST node kinds, and
  special-unit grammar. Parser source spans remain result-label authority.
- Analyzer/planner: resolve descriptor arguments and reject unsupported shapes.
  It creates bound planned values and internal function calls; it does not ask
  SQLite metadata which columns exist.
- Catalog: read-only descriptor authority. `TIMESTAMPDIFF()` must not mutate
  descriptor rows, descriptor versions, descriptor caches, catalog generation,
  or `sqlite_schema_generation`.
- Result builder: returns signed integer text or `NULL`, using aliases or
  source spans for labels.
- Storage/VFS/file format: unchanged. `.mylite` preamble and shifted SQLite
  payload invariants are preserved.
- SQLite: use the public scalar-function registration API for
  `_mylite_timestampdiff`. No SQLite fork patch is required.

## Diagnostics

Required diagnostics:

- wrong arity, quoted unit, unknown unit, `SQL_TSI_MICROSECOND`, or composite
  unit: parse error `1064 / 42000`;
- deferred bare `MICROSECOND`: deterministic MyLite unsupported-feature error,
  `TIMESTAMPDIFF() does not yet support MICROSECOND`;
- unsupported argument kind in no-source/`DUAL`/`DO`: deterministic
  MyLite unsupported-feature error;
- unresolved identifier in no-source scalar context: MySQL-style unknown-column
  diagnostic;
- unknown table-backed column: existing descriptor-resolution unknown-column
  diagnostic;
- unsupported descriptor family such as `TIME`, integer, decimal, approximate,
  binary string, `BIT`, `ENUM`, `SET`, `JSON`, or spatial:
  deterministic MyLite unsupported-feature error;
- embedded `NUL` in decoded string literal: deterministic MyLite
  unsupported-feature error;
- invalid string temporal value: warning `1292 / 22007`,
  `Incorrect datetime value: 'value'`, result `NULL`;
- allocation failure: `MYLITE_NOMEM` with the existing public diagnostic path;
- physical SQLite callback misuse or unexpected SQLite failure: existing
  MyLite/SQLite runtime error path.

Successful supported in-range statements produce `warning_count == 0`.

## Tests

Add:

- MySQL expectation script
  `packages/libmylite/tests/mysql_baseline_timestampdiff_function_expectations.sh`;
- parser coverage in `packages/libmylite/tests/parser_test.c`;
- runtime C test
  `packages/libmylite/tests/runtime_timestampdiff_function_test.c`;
- CMake target and dotted CTest entry.

Coverage must include:

- scalar no-source, `DUAL`, and `DO` execution;
- result labels, aliases, and whitespace before `(`;
- successful `YEAR`, `QUARTER`, `MONTH`, `WEEK`, `DAY`, `HOUR`, `MINUTE`, and
  `SECOND` values;
- matching `SQL_TSI_` aliases for the supported units;
- positive and negative differences;
- calendar-boundary truncation for `YEAR`, `QUARTER`, and `MONTH`;
- partial-unit truncation toward zero for elapsed units;
- date-only values as midnight;
- `NULL` short-circuit behavior and invalid temporal warnings;
- full-zero, partial-zero, and year-zero complete dates;
- table-backed row-scalar projection over descriptor `DATE`, `DATETIME`,
  `TIMESTAMP`, and string-family columns;
- reopen persistence for row-backed input values;
- unsupported `MICROSECOND`, `TIME` arguments, numeric/boolean arguments,
  arbitrary expressions, nested functions, unknown columns, and embedded
  `NUL` literals;
- parse rejection for wrong arity, quoted/unknown/composite units, and
  `SQL_TSI_MICROSECOND`;
- row-count and warning-count behavior;
- existing temporal, parser, row-scalar, runtime lifecycle, file-backed, and
  full check workflows.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`, changing `TIMESTAMPDIFF()` from unsupported to the
  precise limited supported surface;
- `docs/compatibility/functions-temporal.md`, with the same limited wording.

Do not claim support for `TIMESTAMPADD()`, `TIMEDIFF()`, `MICROSECOND`
runtime results, fractional seconds, relaxed temporal strings, `TIME` input,
predicates, DML assignments, defaults, generated columns, arbitrary
expressions, or full temporal expression evaluation.

## Performance And Storage

No catalog, storage, file-format, or SQLite fork changes are required.
No-source scalar evaluation runs directly in MyLite. Row-scalar projection is
lowered to a SQLite scalar function call over the physical table scan, so
filtering, ordering, limiting, and row iteration stay in SQLite rather than in
an eager MyLite materialization layer.
