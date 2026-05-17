# Baseline DATE_SUB SECOND Aliases

## Goal

Extend the existing `DATE_ADD(... INTERVAL ... SECOND)` scalar slice to the
matching subtraction and alias forms:

```sql
DATE_SUB(date_or_datetime_string, INTERVAL interval_value SECOND)
ADDDATE(date_or_datetime_string, INTERVAL interval_value SECOND)
SUBDATE(date_or_datetime_string, INTERVAL interval_value SECOND)
```

This is a narrow temporal arithmetic compatibility slice. It covers common
application SQL that uses MySQL's interval function spellings while preserving
the current limits: scalar no-source/`DUAL`/`DO` only, whole-second intervals,
canonical date/datetime strings, and MyLite-owned evaluation.

## Sources

- Official MySQL 8.4 date and time function documentation:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Official MySQL 8.4 expression and temporal interval documentation:
  <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- Official MySQL 8.4 function-name parsing rules:
  <https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_date_sub_second_aliases_expectations.sh`.

The MyLite grammar and implementation are independently authored from the
official documentation and MySQL 8.4.9 runtime observations. Do not copy MySQL
grammar or implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes against the local MySQL 8.4.9 container establish these
expectations for this slice:

- `DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 SECOND)` returns
  `2008-01-02 13:29:16` with zero warnings.
- `DATE_SUB(... INTERVAL -1 SECOND)` and `SUBDATE(... INTERVAL -1 SECOND)`
  add one second. `ADDDATE(... INTERVAL -1 SECOND)` subtracts one second.
- `ADDDATE(date, INTERVAL value SECOND)` behaves like the existing supported
  `DATE_ADD()` interval form. The non-interval `ADDDATE(date, days)` shorthand
  exists in MySQL but remains deferred.
- `SUBDATE(date, INTERVAL value SECOND)` behaves like `DATE_SUB()`. Other
  `SUBDATE()` forms remain deferred.
- `DATE_SUB`, `ADDDATE`, and `SUBDATE` all reject whitespace between the
  function name and `(` in default SQL mode and accept that whitespace with
  `IGNORE_SPACE`.
- Identifier behavior differs by function name. `DATE_SUB` follows the same
  whitespace-sensitive built-in identifier rule as `DATE_ADD`: in default SQL
  mode `CREATE TABLE date_sub(id INT)` is a syntax error while
  `CREATE TABLE date_sub (id INT)` is accepted, and with `IGNORE_SPACE`
  unquoted `date_sub` is reserved. `ADDDATE` and `SUBDATE` remain usable as
  ordinary unquoted identifiers in the tested table-name positions, including
  under `IGNORE_SPACE`.
- A `NULL` date argument or `NULL` interval argument returns `NULL`.
- Date-only input is interpreted as midnight, so subtracting one second from
  `2008-01-02` returns `2008-01-01 23:59:59`.
- Successful `SELECT` reports `ROW_COUNT() = -1`; successful `DO` reports
  `ROW_COUNT() = 0`.

## Supported Surface

MyLite supports this exact subset:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO` scalar expressions;
- one function call per scalar expression, with ordinary expression
  parentheses allowed by the existing expression grammar;
- default SQL mode requires no whitespace between `DATE_SUB`, `ADDDATE`, or
  `SUBDATE` and `(`. The existing `IGNORE_SPACE` SQL mode permits whitespace
  for these calls;
- first argument limited to:
  - a single- or double-quoted canonical date string `YYYY-MM-DD`;
  - a single- or double-quoted canonical datetime string
    `YYYY-MM-DD HH:MM:SS`;
  - `NULL`;
- date and datetime inputs must be complete, valid Gregorian values in the
  current MyLite temporal storage baseline range `1000-01-01` through
  `9999-12-31 23:59:59`;
- second argument must use the literal interval syntax
  `INTERVAL interval_value SECOND`;
- `interval_value` is limited to a decimal integer literal with optional unary
  `+` or `-`, or `NULL`;
- integer interval values must fit the signed 64-bit range;
- `ADDDATE` adds the interval, matching `DATE_ADD`;
- `DATE_SUB` and `SUBDATE` subtract the interval. Negative intervals therefore
  add seconds;
- outputs are datetime text. Date-only input is interpreted as midnight before
  arithmetic;
- results must remain in the current MyLite temporal storage baseline range
  `1000-01-01 00:00:00` through `9999-12-31 23:59:59`;
- successful supported expressions produce no warnings and do not mutate
  catalog state, descriptors, physical tables, or the `.mylite` preamble.

`DATE_SUB` is a whitespace-sensitive built-in identifier for this slice. It
remains usable as an unquoted identifier in default SQL mode where MySQL allows
it, but not when immediately followed by `(` in table-definition syntax and not
when `IGNORE_SPACE` is active. `ADDDATE` and `SUBDATE` are parsed as explicit
function calls in expression positions, but remain ordinary identifiers in the
tested table-name positions. `SECOND` remains nonreserved. `INTERVAL` remains
reserved.

## Deferred Surface

This slice intentionally does not support:

- interval units other than `SECOND`;
- plural or alternate unit spellings;
- `ADDDATE(date, days)` or non-interval `SUBDATE` forms;
- `TIMESTAMPADD()`, `ADDTIME()`, `SUBTIME()`, or infix
  `date + INTERVAL ...` / `date - INTERVAL ...` temporal arithmetic;
- interval expressions, string intervals, decimal/fractional intervals,
  floating-point intervals, function or column interval operands, parameters,
  user variables, or subqueries;
- relaxed temporal input strings, incomplete dates, zero dates, fractional
  seconds, time-zone conversion, `TIMESTAMP` session time-zone semantics, or
  warning-returning invalid temporal conversions;
- table-backed projection, predicates, ordering, grouping, joins, DML
  assignments, generated columns, defaults, casts, or function nesting beyond
  the existing scalar-expression envelope;
- binary/protocol-grade temporal metadata. The public result currently exposes
  text/`NULL` scalar values.

## Grammar

The MySQL grammar accepts broader temporal interval expressions. MyLite admits
only the independently authored subset below:

```lemon
expression ::= DATE_SUB LPAREN expression COMMA INTERVAL expression SECOND RPAREN.
expression ::= ADDDATE LPAREN expression COMMA INTERVAL expression SECOND RPAREN.
expression ::= SUBDATE LPAREN expression COMMA INTERVAL expression SECOND RPAREN.
```

The runtime, not the grammar, enforces that the first expression is a supported
date/datetime string or `NULL`, and that the interval expression is a supported
signed integer literal or `NULL`.

The parser enforces MySQL's whitespace-sensitive built-in function rule for the
three call forms. `DATE_SUB` also participates in the existing
`IGNORE_SPACE`-sensitive identifier handling:

```lemon
identifier ::= DATE_SUB.
identifier ::= ADDDATE.
identifier ::= SUBDATE.
identifier ::= SECOND.
```

With `IGNORE_SPACE`, unquoted `DATE_SUB` is rejected as an identifier and
quoted `` `date_sub` `` remains available. `ADDDATE`, `SUBDATE`, and `SECOND`
remain available as unquoted identifiers under `IGNORE_SPACE` for the tested
positions.

## Runtime Semantics

### Evaluation

1. Evaluate the first argument with MyLite-owned rules for this feature only.
   `NULL` short-circuits to a `NULL` result.
2. Decode a string literal using the current statement SQL mode. This reuses
   the same string literal decoding path as existing row values and scalar
   string functions, including `NO_BACKSLASH_ESCAPES` and `ANSI_QUOTES`
   behavior.
3. Validate either canonical date shape or canonical datetime shape. Date-only
   inputs use `00:00:00` before arithmetic.
4. Evaluate the interval expression. `NULL` returns `NULL`; signed integer
   literals are parsed in signed 64-bit range.
5. Apply whole-second arithmetic. `ADDDATE` uses the interval as written;
   `DATE_SUB` and `SUBDATE` negate the interval before applying it. If that
   negation cannot be represented in the supported internal range, fail with
   the function-specific result-range diagnostic.
6. Return `YYYY-MM-DD HH:MM:SS` text when the result is within the supported
   scalar result range.

### Diagnostics

Supported in-range expressions return successfully with `warning_count == 0`.
Unsupported or out-of-scope forms fail deterministically through the existing
MyLite unsupported-diagnostic path unless the existing parser rejects them
earlier.

Required diagnostics use the invoked function name:

- syntax errors: existing parser syntax diagnostics;
- wrong function shape: existing parser syntax diagnostics for forms that do
  not match `FUNCTION(expr, INTERVAL expr SECOND)`;
- first argument is not string/`NULL`:
  `FUNCTION() supports only date or datetime string literals and NULL`;
- string input contains embedded NUL:
  `FUNCTION() date literals do not support NUL bytes`;
- invalid or unsupported input date/datetime string:
  `FUNCTION() supports only canonical YYYY-MM-DD or YYYY-MM-DD HH:MM:SS values`;
- interval value is not integer/`NULL`:
  `FUNCTION() INTERVAL SECOND supports only signed integer literals and NULL`;
- interval integer is outside signed 64-bit range:
  `FUNCTION() INTERVAL SECOND literals must fit the signed 64-bit range`;
- result year cannot be represented in this baseline:
  `FUNCTION() result is outside the supported datetime range`;
- allocation failure: existing `MYLITE_NOMEM` diagnostic behavior;
- public API misuse: no public API changes.

## Architecture

- Public API: unchanged. Successful `SELECT` and `DO` use existing
  `mylite_execute()` and result APIs.
- Statement context: unchanged except that parser/lexer SQL-mode flags
  determine double quotes, backslash escapes, and `IGNORE_SPACE` function-name
  parsing.
- Lexer/parser/AST: add explicit tokens and AST node kinds for the three new
  spellings. Do not introduce general function-call parsing.
- Analyzer/planner/runtime: no catalog resolution and no table planning are
  needed for this no-source scalar slice. The evaluator reuses the current
  MyLite-owned string decoding, temporal validation, signed integer parsing,
  and date arithmetic.
- Catalog: untouched. These functions do not read or mutate descriptors,
  descriptor caches, catalog generation, or `sqlite_schema_generation`.
- Result builder: returns a single text/`NULL` scalar cell through existing
  scalar-result paths. Column labels come from the original expression span or
  explicit alias.
- Storage/VFS/file format: untouched. The functions are pure scalar evaluation
  and must not modify SQLite payload or the MyLite preamble.
- SQLite: no public SQLite API hook and no SQLite fork patch are required.

## Tests

Add fast C coverage under `packages/libmylite/tests/`:

- parser accepts supported `DATE_SUB`, `ADDDATE`, and `SUBDATE`
  `INTERVAL ... SECOND` forms and preserves spans;
- default SQL mode rejects whitespace before `(` for all three function names,
  while `IGNORE_SPACE` accepts it;
- default SQL mode rejects `CREATE TABLE date_sub(id INT)` but accepts
  `CREATE TABLE date_sub (id INT)`;
- `IGNORE_SPACE` rejects unquoted `DATE_SUB` as an identifier and accepts
  quoted `` `date_sub` ``;
- `ADDDATE`, `SUBDATE`, and `SECOND` remain usable as unquoted identifiers in
  tested table-name or column-name positions;
- no-source, `FROM DUAL`, aliases, `DO`, `NULL`, signed intervals, date-only
  input, leap-day rollover, and independent handle behavior;
- current SQL mode behavior for double quotes and backslashes;
- deterministic unsupported diagnostics for bad first arguments, unsupported
  interval operands, out-of-range interval integers, invalid date text,
  unsupported units, and result overflow/underflow;
- file preamble remains unchanged across scalar evaluation.

Add a MySQL 8.4.9 expectation artifact:

```sh
./packages/libmylite/tests/mysql_baseline_date_sub_second_aliases_expectations.sh
```

## Compatibility Updates

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/functions-temporal.md`
- `docs/compatibility/sql-query-expressions.md`
- `docs/compatibility/type-system-literals-conversion.md`
- `docs/specs/baseline-date-add-second/specs.md`
