# Baseline DATE_FORMAT Function

## Goal

Add the narrow `DATE_FORMAT()` slice needed by common application SQL such as:

```sql
SELECT DATE_FORMAT(option_value, '%H.%i') = 0.42
FROM options;
```

This feature extends MyLite's scalar and row-scalar projection path. It is not
a general temporal expression engine, locale subsystem, relaxed temporal parser,
or full expression comparison implementation.

## Sources

- Official MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Official MySQL 8.4 Reference Manual, function-name parsing and resolution:
  <https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html>
- Existing temporal, row-scalar, and comparison designs:
  - `docs/specs/baseline-date-add-second/specs.md`
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
  - `docs/specs/baseline-scalar-comparison-projection/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_date_format_function_expectations.sh`.

The MyLite grammar and implementation are independently authored from official
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. Do not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `DATE_FORMAT(date, format)` formats the first argument according to the
  string format argument.
- If either argument is `NULL`, the result is `NULL`.
- Invalid temporal string input returns `NULL` and records warning
  `1292 / 22007` with message `Incorrect datetime value: 'value'`.
- `DATE_FORMAT()` requires exactly two arguments. Wrong arities fail with
  `1582 / 42000`.
- `DATE_FORMAT('2008-01-02', '%Y-%m-%d %H:%i:%s')` treats a date-only value as
  midnight and returns `2008-01-02 00:00:00`.
- Supported MySQL format tokens verified for this slice include year, month,
  day, hour, minute, second, microsecond, AM/PM, 12-hour and 24-hour time,
  weekday/month names, ordinal day, day-of-year, literal percent, trailing
  percent, and unknown nonreserved percent sequences.
- MySQL also supports week/year format tokens. MyLite defers those because they
  require the broader week-numbering semantics that belong with `WEEK()`,
  `YEARWEEK()`, and related functions.
- `DATE_FORMAT` is not in MySQL's whitespace-sensitive built-in function list:
  `DATE_FORMAT ('2008-01-02', '%Y')` is accepted in default SQL mode and under
  `IGNORE_SPACE`. Unquoted `date_format` remains usable as a table name in
  nonexpression contexts.
- Table-backed `DATE_FORMAT(column, literal_format)` evaluates once per row and
  preserves the existing single-table row envelope for `WHERE`, `ORDER BY`, and
  `LIMIT`.
- `DATE_FORMAT(value, '%H.%i') = 0.42` returns `1`, `0`, or `NULL` as a numeric
  comparison. Invalid temporal values still return `NULL` and append the
  temporal warning.
- Successful supported statements produce no warnings except for invalid
  temporal inputs. Successful `SELECT` makes a following `ROW_COUNT()` return
  `-1`; successful `DO` makes it return `0`.

## Supported Surface

MyLite supports:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` using the existing row envelope:
  `FROM table [AS alias]`, optional existing `WHERE`, single descriptor-column
  `ORDER BY`, and existing `LIMIT`;
- `DATE_FORMAT(value, format)` with exactly two arguments;
- `value` as:
  - `NULL`;
  - a single- or double-quoted string literal containing canonical
    `YYYY-MM-DD` or `YYYY-MM-DD HH:MM:SS`;
  - a descriptor column in a table-backed row-scalar `SELECT` whose current
    descriptor family is `DATE`, `DATETIME`, `TIMESTAMP`, `CHAR`, `VARCHAR`, or
    baseline `TEXT`;
- `format` as:
  - `NULL`;
  - a single- or double-quoted string literal without embedded `NUL`;
- valid date/datetime values in the current storage baseline range
  `1000-01-01` through `9999-12-31 23:59:59`;
- format tokens:
  - `%Y`, `%y`, `%m`, `%c`, `%d`, `%e`, `%H`, `%k`, `%h`, `%I`, `%l`, `%i`,
    `%S`, `%s`, `%T`, `%r`, `%p`, `%f`;
  - `%a`, `%W`, `%b`, `%M`, `%D`, `%j`, `%w`;
  - `%%`, a trailing `%`, and unknown non-format percent sequences such as
    `%q` as MySQL-style literal output;
- top-level row-scalar numeric equality of the exact form
  `DATE_FORMAT(value, '%H.%i') = numeric_literal` or
  `numeric_literal = DATE_FORMAT(value, '%H.%i')`, where `numeric_literal` is a
  decimal integer or fixed decimal literal with optional unary sign;
- output text/`NULL` values through existing result APIs;
- warning count `0` for supported valid in-range forms.

The admitted row-scalar numeric equality is deliberately narrow. It exists for
the common `DATE_FORMAT(..., '%H.%i') = 0.42` pattern and does not establish
general table-backed expression comparison.

## Deferred Surface

This slice intentionally does not support:

- `TIME` descriptor values or time-only string inputs. MySQL's `DATE_FORMAT()`
  behavior for `TIME` values depends on current-date coercion that needs a
  separate design;
- week and week-year format tokens `%U`, `%u`, `%V`, `%v`, `%X`, and `%x`;
- fractional temporal inputs beyond returning `%f` as `000000`;
- locale, language, character set, collation, or time-zone effects;
- relaxed temporal strings, incomplete dates, zero dates, invalid-date SQL mode
  effects, or warning-compatible coercion beyond the explicit invalid string
  warning above;
- format columns, format expressions, variables, parameters, subqueries,
  `GET_FORMAT()`, `TIME_FORMAT()`, `STR_TO_DATE()`, or arbitrary nesting;
- table-backed expression projection outside flat `DATE_FORMAT()` and the
  exact top-level numeric equality shape described above;
- use in `WHERE` beyond the later `baseline-date-format-predicates` equality
  slice, `ORDER BY`, `GROUP BY`, `HAVING`, DML assignments, defaults, generated
  columns, indexes, constraints, joins, CTEs, or arbitrary SQLite pass-through.

## Grammar

MyLite adds this parser production:

```lemon
expression(A) ::= DATE_FORMAT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R).
```

Wrong arities produce an argument-count AST node so runtime can return MySQL's
native-function parameter-count diagnostic:

```lemon
expression(A) ::= DATE_FORMAT(T) LPAREN RPAREN(R).
expression(A) ::= DATE_FORMAT(T) LPAREN expression(B) RPAREN(R).
expression(A) ::=
    DATE_FORMAT(T) LPAREN expression(B) COMMA expression(C)
    COMMA function_argument_list(D) RPAREN(R).
```

`DATE_FORMAT` is admitted as an identifier where MyLite admits ordinary
identifiers:

```lemon
identifier(A) ::= DATE_FORMAT(T).
```

It is not a whitespace-sensitive function name for this slice.

Analyzer/runtime acceptance is narrower:

```lemon
date_format_expr(A) ::= DATE_FORMAT LPAREN date_format_value COMMA date_format_format RPAREN.

date_format_value(A) ::= descriptor_date_column(B).
date_format_value(A) ::= descriptor_datetime_column(B).
date_format_value(A) ::= descriptor_timestamp_column(B).
date_format_value(A) ::= descriptor_string_column(B).
date_format_value(A) ::= string_literal(T).
date_format_value(A) ::= NULL(T).

date_format_format(A) ::= string_literal(T).
date_format_format(A) ::= NULL(T).

date_format_numeric_equal(A) ::=
    DATE_FORMAT LPAREN date_format_value COMMA hour_minute_decimal_format RPAREN
    EQUAL numeric_literal(C).
date_format_numeric_equal(A) ::=
    numeric_literal(B) EQUAL
    DATE_FORMAT LPAREN date_format_value COMMA hour_minute_decimal_format RPAREN.

hour_minute_decimal_format(A) ::= string_literal_with_decoded_value_percent_H_dot_percent_i(T).
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning:

1. Detect no-source/`DUAL` scalar expressions and row-scalar projection
   attempts that contain a top-level or parenthesized `DATE_FORMAT()` call, or
   the exact top-level numeric equality shape admitted above.
2. Resolve row sources through the existing selected/default schema policy.
3. Resolve descriptor column arguments through MyLite catalog descriptors, not
   SQLite schema text.
4. Decode string literal arguments using the current statement SQL mode,
   including `ANSI_QUOTES` and `NO_BACKSLASH_ESCAPES`.
5. Validate the format literal for this slice. Known deferred week tokens fail
   deterministically rather than returning wrong values. The numeric equality
   form additionally requires the decoded format literal to be exactly
   `%H.%i`, because other formatted strings need MySQL-compatible numeric
   coercion and truncation warnings.
6. Generate SQLite projection SQL over stable physical table names and quoted
   physical column names. String literals and comparison literals are bound
   parameters.
7. Use a MyLite-owned SQLite scalar function for table-backed row execution so
   SQLite can keep scanning, filtering, ordering, and limiting without MyLite
   materializing source rows.

Scalar evaluation:

1. `NULL` value or `NULL` format returns `NULL`.
2. Date-only values use midnight.
3. Valid datetime values are formatted according to the admitted token set.
4. Invalid non-`NULL` temporal text returns `NULL` and appends warning
   `1292 / 22007`, `Incorrect datetime value: 'value'`.
5. Unknown non-format percent sequences such as `%q` output the character after
   `%`. A trailing `%` outputs `%`.

The row-backed generated SQL shape for `DATE_FORMAT()` is:

```sql
_mylite_date_format(value_expr, format_expr, input_kind_expr)
```

`input_kind_expr` is a bound MyLite-internal discriminator such as `string`,
`date`, `datetime`, or `timestamp`. It is not user-visible.

The row-backed generated SQL shape for the admitted `%H.%i` numeric equality is:

```sql
CAST(_mylite_date_format(value_expr, format_expr, input_kind_expr) AS REAL)
    = CAST(? AS REAL)
```

or the same comparison with operands reversed. This is limited to top-level
projection because MyLite is not yet claiming general table-backed comparison
coercion.

## Ownership Boundaries

- Public API: unchanged. Successful statements use existing `mylite_execute()`
  and result APIs.
- Statement context: unchanged. Statement diagnostics and row-count behavior
  follow existing scalar and row-scalar `SELECT` / `DO` conventions.
- Lexer/parser/AST: add `DATE_FORMAT` token handling, AST node kinds, and
  argument-count nodes. Parser source spans remain result-label authority.
- Analyzer/planner: resolve descriptor arguments and reject unsupported shapes.
  It creates bound planned values and internal function calls; it does not ask
  SQLite metadata which columns exist.
- Catalog: read-only descriptor authority. `DATE_FORMAT()` must not mutate
  descriptor rows, descriptor versions, descriptor caches, catalog generation,
  or `sqlite_schema_generation`.
- Result builder: returns text/`NULL` for `DATE_FORMAT()` and `1`/`0`/`NULL`
  text for the admitted numeric equality, using aliases or source spans for
  labels.
- Storage/VFS/file format: unchanged. `.mylite` preamble and shifted SQLite
  payload invariants are preserved.
- SQLite: use the public scalar-function registration API for
  `_mylite_date_format`. No SQLite fork patch is required.

## Diagnostics

Required diagnostics:

- parser syntax errors through existing parse diagnostics;
- wrong argument count: MySQL-compatible `1582 / 42000`
  `Incorrect parameter count in the call to native function 'DATE_FORMAT'`;
- unknown row descriptor columns through MySQL-compatible unknown-column
  diagnostics in field-list context;
- unsupported value argument:
  `DATE_FORMAT() supports only string, DATE, DATETIME, TIMESTAMP, and NULL values`;
- unsupported `TIME` argument:
  `DATE_FORMAT() does not yet support TIME values`;
- unsupported format argument:
  `DATE_FORMAT() supports only string format literals and NULL`;
- embedded `NUL` in a string literal:
  `DATE_FORMAT() literals do not support NUL bytes`;
- deferred week token:
  `DATE_FORMAT() does not yet support week-based format specifiers`;
- invalid temporal input warning:
  `Warning 1292 / 22007 Incorrect datetime value: 'value'`;
- unsupported top-level comparison shape:
  deterministic MyLite unsupported-feature diagnostic explaining the exact
  admitted equality form;
- allocation failure through existing `MYLITE_NOMEM` behavior;
- physical SQLite failures through existing runtime diagnostics;
- public API misuse: no public API changes.

## Performance

No-source and `DUAL` scalar evaluation is MyLite-owned and proportional to
argument and format-string length. Row-backed execution remains close to
SQLite's optimal path for the current architecture: MyLite resolves descriptors,
builds a SQLite expression, binds values, and lets SQLite scan/filter/order and
limit rows. The MyLite scalar callback formats one row value at a time and does
not materialize the source row set.

## Tests

Add MySQL-runtime expectation coverage for:

- core token output over canonical datetime text;
- date-only input, `NULL` value, `NULL` format, warning count, and row count;
- weekday/month names, ordinal day, day-of-year, literal percent, trailing
  percent, and unknown non-format percent output;
- invalid string input returning `NULL` with warning `1292`;
- default and `IGNORE_SPACE` whitespace handling;
- unquoted `date_format` identifier behavior;
- table-backed `VARCHAR`, `DATE`, `DATETIME`, and `TIMESTAMP` values plus
  `NULL` values;
- the user-shaped numeric equality
  `DATE_FORMAT(option_value, '%H.%i') = 0.42`;
- MySQL's truncation-warning behavior for broader formatted-string numeric
  comparisons, which MyLite defers instead of accepting silently;
- wrong-arity diagnostics;
- MySQL-accepted but deferred `TIME`, week tokens, format columns, string/float
  nonliteral format arguments, and broader expression comparison forms.

Add fast C tests under `packages/libmylite/tests/`, preferably
`runtime_date_format_function`, plus parser and lexer coverage.

## Compatibility Updates

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/functions-temporal.md`
- `docs/compatibility/sql-query-expressions.md`
- `docs/compatibility/type-system-literals-conversion.md`

Use limited wording. Do not claim full `DATE_FORMAT()`, time-only formatting,
week specifiers, locale behavior, relaxed temporal coercion, general
table-backed expressions, expression predicates, expression ordering, or full
comparison coercion.
