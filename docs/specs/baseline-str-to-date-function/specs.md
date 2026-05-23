# Baseline STR_TO_DATE Function

## Goal

Add a narrow, MySQL-runtime-verified `STR_TO_DATE(str, format)` slice for
scalar and single-table row-scalar projection:

```sql
SELECT STR_TO_DATE(value, format)
SELECT STR_TO_DATE(value, format) FROM table_name
DO STR_TO_DATE(value, format)
```

The slice covers common numeric date, time, and datetime parsing without
claiming the full `DATE_FORMAT()` format-language inverse, locale-sensitive
names, fractional seconds, arbitrary expression support, or DML assignment
coverage.

## Compatibility Authority

- Official MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html#function_str-to-date>
- Official MySQL 8.4 Reference Manual, built-in function parsing:
  <https://dev.mysql.com/doc/refman/8.4/en/functions.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_str_to_date_function_expectations.sh`.

The MyLite grammar and implementation are independently authored from official
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. Do not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the baseline expectations:

- `STR_TO_DATE(str, format)` returns `NULL` with no warning if either argument
  is `NULL`, after normal name resolution for referenced columns, including
  identifiers nested inside otherwise short-circuited expressions.
- Wrong arity raises `1582 / 42000`.
- The result is rendered as a date when only date parts are present, a time
  when only time parts are present, and a datetime when both date and time
  parts are present.
- Supported numeric date and time fields accept one or two digits for month,
  day, hour, minute, and second fields where MySQL accepts them.
- `%Y` accepts one to four digits; one- and two-digit `%Y` input follows
  MySQL's two-digit year mapping, while three- and four-digit input is rendered
  as that year. `%y` always uses the two-digit mapping.
- `%h`, `%I`, and `%l` parse 12-hour input; `12` maps to hour `00` unless
  followed by a PM marker.
- `%p` is valid only after a matched 12-hour field in this slice; orphaned or
  preceding meridiem markers return `NULL` with warning `1411`.
- Extra characters after a successfully matched value are ignored and produce
  warning `1292 / 22007` with the value-specific truncated date/time/datetime
  message.
- Format mismatch, invalid fields, invalid combined calendar dates, and
  disallowed zero-date results return `NULL` and produce warning
  `1411 / HY000`.
- With `sql_mode = ''`, zero date parts are allowed and unspecified date or
  time fields render as zero parts. With `NO_ZERO_DATE`, any zero year, month,
  or day in a date result returns `NULL` with warning `1411`. With
  `NO_ZERO_IN_DATE`, zero month or day parts return `NULL` with warning `1411`
  unless the entire date is `0000-00-00`; zero years with nonzero month and day
  are accepted.
- With `ALLOW_INVALID_DATES`, date results whose year, month, and day fields
  are individually in range but do not form a real calendar date are returned as
  text instead of `NULL`; `NO_ZERO_DATE` and `NO_ZERO_IN_DATE` restrictions
  still apply to zero parts before this relaxed calendar validation.
- `STRICT_TRANS_TABLES` alone does not reject zero parts for this scalar
  function.

## Supported Surface

MyLite supports:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` using the existing row envelope: one
  descriptor-backed table source, optional existing `WHERE`, ordering, and
  limit support;
- exactly two arguments;
- `str` as a string literal, `NULL`, or a descriptor-backed nonbinary string or
  baseline `TEXT` family column, plus MySQL-compatible `NULL` short-circuiting
  when the `format` argument is literal `NULL`;
- `format` as a string literal or `NULL`, plus MySQL-compatible `NULL`
  short-circuiting when the `str` argument is literal `NULL`;
- literal format characters that must match input literally;
- format specifiers `%Y`, `%y`, `%m`, `%c`, `%d`, `%e`, `%H`, `%k`, `%h`,
  `%I`, `%l`, `%i`, `%s`, `%S`, `%T`, `%r`, `%p`, and `%%`;
- contiguous numeric parsing for adjacent numeric specifiers such as
  `%Y%m%d`, but not MySQL's broader separator-skipping behavior for every
  adjacent-specifier shape;
- result strings through existing public result APIs.

## Deferred Surface

This slice does not support:

- DML assignments, defaults, generated columns, check constraints, predicates,
  grouping expressions, aggregate arguments, joins beyond the existing
  row-scalar envelope, CTEs, views, stored programs, or arbitrary expression
  parents;
- non-string first arguments except `NULL`, temporal columns, binary strings,
  JSON, numeric columns, `ENUM`, `SET`, spatial values, or user variables;
- row-backed or nonliteral format arguments;
- locale-sensitive month or weekday names (`%M`, `%b`, `%W`, `%a`),
  day-of-year, week/year-week, ordinal day suffixes, fractional seconds
  (`%f`), time-zone offsets, or any other format specifier outside the
  supported set;
- relaxed compact-input separator skipping beyond the documented contiguous
  numeric subset;
- numeric context, protocol-grade temporal metadata, or fractional precision;
- MySQL's full historical edge-case behavior for every invalid or incomplete
  format combination.

## Grammar

MyLite extends the existing expression grammar with function-specific AST nodes:

```lemon
expression(A) ::= STR_TO_DATE(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_STR_TO_DATE_FUNCTION, B, C, R);
}
expression(A) ::= STR_TO_DATE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_STR_TO_DATE_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= STR_TO_DATE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_STR_TO_DATE_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    STR_TO_DATE(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_STR_TO_DATE_ARGUMENT_COUNT_ERROR, D, R);
}
identifier(A) ::= STR_TO_DATE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

These snippets describe MyLite's supported grammar. They are not copied from
MySQL grammar text.

## Runtime Semantics

Planning and execution:

1. Detect supported top-level scalar and row-scalar calls during the existing
   expression analysis.
2. Preserve native-function arity diagnostics before generic unsupported-form
   diagnostics.
3. Decode admitted SQL string literals through MyLite string-literal decoding,
   respecting the current `NO_BACKSLASH_ESCAPES` mode.
4. Resolve row-backed `str` column arguments through MyLite descriptors, never
   SQLite schema text.
5. Validate the literal `format` against the admitted specifier set before
   execution.
6. Parse the input from the beginning of the string according to the format.
   Literal format bytes must match input bytes. Numeric specifiers consume the
   documented width range for the supported subset.
7. Build the result kind from the format parts: date, time, or datetime.
8. Fill unspecified parts with zero. Apply MySQL-compatible two-digit year
   mapping for `%y` and one- or two-digit `%Y`.
9. Return `NULL` with warning `1411 / HY000` for mismatches, invalid field
   ranges, invalid combined calendar dates unless `ALLOW_INVALID_DATES` is
   active, and zero date parts according to MySQL's `NO_ZERO_DATE` and
   `NO_ZERO_IN_DATE` scalar-function behavior.
10. Return the parsed value and append warning `1292 / 22007` when the format
    matched but the input has trailing extra characters.
11. Generate row-scalar SQLite SQL only from descriptor-planned expressions,
    quoted identifiers, numbered parameters, and the registered MyLite scalar
    function:

```sql
_mylite_str_to_date(value_sql, format_sql)
```

The generated SQL stays in the physical row path. MyLite does not materialize
source rows in memory to evaluate `STR_TO_DATE()`.

## Ownership Boundaries

- Public API: unchanged. Callers use `mylite_execute()` and existing result
  accessors.
- Statement context: unchanged. Successful `SELECT` and `DO` preserve existing
  row-count, warning-count, and diagnostics snapshot behavior.
- Lexer/parser/AST: adds `STR_TO_DATE` token and AST nodes. Parser source spans
  remain authoritative for default result labels.
- Analyzer/planner: owns supported-shape validation, descriptor column
  resolution, literal-format validation, and generated SQL construction.
- Runtime helper module: owns `STR_TO_DATE()` parsing, warning staging, and
  SQLite callback registration.
- Catalog: read-only descriptor authority. No descriptors, catalog generation,
  or SQLite schema generation are mutated.
- Result builder: exposes result text through existing result object
  conventions.
- Storage/VFS/file format: unchanged. `.mylite` preamble and shifted SQLite
  payload invariants are preserved.
- SQLite: uses public scalar function registration only. No SQLite fork patch
  is required.

## Diagnostics

Required diagnostics:

- parser syntax errors through existing parse diagnostics;
- wrong argument count: `1582 / 42000` native-function parameter-count errors
  for `STR_TO_DATE`;
- unsupported scalar value shapes:
  `STR_TO_DATE() supports only string and NULL values`;
- unsupported format-argument shapes:
  `STR_TO_DATE() supports only string format literals and NULL`;
- unsupported row-backed column types:
  `STR_TO_DATE() supports only nonbinary string columns`;
- unknown descriptor column arguments through existing unknown-column
  diagnostics in field-list context;
- unsupported format specifier:
  `STR_TO_DATE() supports only baseline date and time format specifiers`;
- invalid input, invalid fields, invalid calendar dates, and disallowed zero
  dates: `Warning 1411 / HY000 Incorrect datetime value: '<value>' for function str_to_date`;
- trailing extra input after a successful match:
  `Warning 1292 / 22007 Truncated incorrect date/time/datetime value: '<value>'`;
- allocation failure through existing `MYLITE_NOMEM` / `HY001` behavior;
- physical SQLite callback failures as statement execution failures through the
  existing physical SQLite error path.

## Tests

Fast C tests must cover:

- scalar date, datetime, and time returns for `%Y-%m-%d`,
  `%Y-%m-%d %H:%i:%s`, `%H:%i:%s`, `%T`, `%h:%i %p`, and `%r`;
- one-digit and two-digit date fields, `%y`, and one-/two-/three-/four-digit
  `%Y` behavior;
- literal prefix matching and mismatch warnings;
- trailing extra input warnings;
- `NULL` arguments, including unsupported-value/format short-circuit cases
  after normal name resolution, including nested identifier references;
- zero-date behavior under `sql_mode = ''`, `NO_ZERO_DATE`, `NO_ZERO_IN_DATE`,
  and `ALLOW_INVALID_DATES`;
- invalid month/day and invalid combined calendar dates;
- orphaned and preceding `%p` meridiem markers;
- row-scalar projection over descriptor-backed `VARCHAR` and `TEXT` columns,
  including warnings and `NULL` rows;
- wrong arity diagnostics;
- unknown column and unsupported descriptor diagnostics;
- `DO` status and warning-count behavior;
- independent handles and file preamble preservation for row-backed reads;
- MySQL 8.4.9 expectation script covering the same user-visible results.

Compatibility docs must mark `STR_TO_DATE()` as limited support and must not
claim general expression, DML, predicate, locale, fractional, or protocol
metadata coverage.
