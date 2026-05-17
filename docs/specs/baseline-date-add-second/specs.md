# Baseline DATE_ADD SECOND

## Goal

Add the first narrow temporal arithmetic scalar function slice:

```sql
DATE_ADD(date_or_datetime_string, INTERVAL interval_value SECOND)
```

This slice exists to cover common application SQL such as:

```sql
SELECT DATE_ADD("2008-01-02 13:29:17", INTERVAL 1 SECOND) AS output
```

It is not a general temporal-expression implementation. It establishes the
MyLite-owned `INTERVAL ... SECOND` parser shape and scalar evaluator path while
leaving broader date/time functions, table-backed temporal expression
projection, fractional seconds, and month/year calendar arithmetic for later
features.

## Sources

- Official MySQL 8.4 Reference Manual, expression and temporal interval
  documentation:
  <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- Official MySQL 8.4 built-in function catalog:
  <https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html>
- Official MySQL 8.4 function-name parsing rules:
  <https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_date_add_second_expectations.sh`.

The MyLite grammar and implementation are independently authored from the
official documentation and MySQL 8.4.9 runtime observations. Do not copy MySQL
grammar or implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes against the local MySQL 8.4.9 container establish these
expectations for this slice:

- `DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND)` returns
  `2008-01-02 13:29:18` with zero warnings.
- Double-quoted strings are accepted in default SQL mode. With `ANSI_QUOTES`,
  the same token is an identifier and the expression fails with an unknown
  column diagnostic.
- `DATE_ADD` is a whitespace-sensitive built-in function name. With default
  SQL mode, `DATE_ADD(...)` is accepted but `DATE_ADD (...)` is a syntax error.
  In identifier positions, unquoted `date_add` is accepted unless it is
  immediately followed by `(`, so `CREATE TABLE date_add (id INT)` is accepted
  and `CREATE TABLE date_add(id INT)` is a syntax error. With `IGNORE_SPACE`,
  both function-call forms are accepted, and unquoted `date_add` is reserved in
  nonexpression identifier positions.
- `INTERVAL +1 SECOND`, `INTERVAL -1 SECOND`, `INTERVAL 0 SECOND`, and
  `INTERVAL NULL SECOND` are accepted; a `NULL` date or interval returns
  `NULL`.
- `DATE_ADD('2008-01-02', INTERVAL 1 SECOND)` returns
  `2008-01-02 00:00:01`; because `SECOND` includes a time part, the result is
  a datetime string.
- Successful `SELECT` reports `ROW_COUNT() = -1`; successful `DO` reports
  `ROW_COUNT() = 0`.
- Other interval units, interval expressions such as `1+1`, string interval
  values, invalid date strings with warning/`NULL` behavior,
  lower-than-`1000` result years, and overflow warning behavior are valid MySQL
  behavior but remain out of scope for this baseline slice.

## Supported Surface

MyLite supports this exact subset:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO` scalar expressions;
- one `DATE_ADD()` call per scalar expression, with ordinary expression
  parentheses allowed by the existing expression grammar;
- default SQL mode requires no whitespace between `DATE_ADD` and `(`. The
  existing `IGNORE_SPACE` SQL mode permits whitespace there for this function;
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
- outputs are datetime text. Date-only input is interpreted as midnight before
  adding seconds;
- results must remain in the current MyLite temporal storage baseline range
  `1000-01-01 00:00:00` through `9999-12-31 23:59:59`;
- successful supported expressions produce no warnings and do not mutate
  catalog state, descriptors, physical tables, or the `.mylite` preamble.

`DATE_ADD` and `SECOND` remain usable as unquoted identifiers in contexts where
MySQL treats them as nonreserved. Without `IGNORE_SPACE`, unquoted `DATE_ADD`
as a `CREATE TABLE` target is rejected only when the identifier is immediately
followed by the table-definition `(`, matching MySQL's function-name parsing
rule. When `IGNORE_SPACE` is active, unquoted `DATE_ADD` is reserved and must
be quoted in identifier positions. `SECOND` remains nonreserved. `INTERVAL`
remains reserved.

## Deferred Surface

This slice intentionally does not support:

- `TIMESTAMPADD()` or infix `date + INTERVAL ...` / `date - INTERVAL ...`
  temporal arithmetic;
- interval units other than `SECOND`;
- plural or alternate unit spellings;
- interval expressions, string intervals, decimal/fractional intervals,
  floating-point intervals, function or column interval operands, parameters,
  user variables, or subqueries;
- relaxed temporal input strings, incomplete dates, zero dates, fractional
  seconds, time-zone conversion, `TIMESTAMP` session time-zone semantics, or
  warning-returning invalid temporal conversions;
- table-backed `DATE_ADD()` projection, predicates, ordering, grouping, joins,
  DML assignments, generated columns, defaults, casts, or function nesting
  beyond the existing scalar-expression envelope;
- binary/protocol-grade temporal metadata. The public result currently exposes
  text/`NULL` scalar values.

## Grammar

The MySQL grammar accepts a broad temporal interval expression. MyLite admits
only the independently authored subset below:

```lemon
expression ::= DATE_ADD LPAREN expression COMMA INTERVAL expression SECOND RPAREN.
```

The runtime, not the grammar, enforces that the first expression is a supported
date/datetime string or `NULL`, and that the interval expression is a supported
signed integer literal or `NULL`.

The parser enforces MySQL's whitespace-sensitive built-in function rule for
`DATE_ADD`: without `IGNORE_SPACE`, the `DATE_ADD` token and `LPAREN` token
must be adjacent; with `IGNORE_SPACE`, intervening whitespace is accepted.

Identifier grammar admits `DATE_ADD` and `SECOND` as unquoted identifiers:

```lemon
identifier ::= DATE_ADD.
identifier ::= SECOND.
```

`INTERVAL` is not admitted as an identifier. With `IGNORE_SPACE`, unquoted
`DATE_ADD` is rejected as an identifier and quoted `` `date_add` `` remains
available. Without `IGNORE_SPACE`, the parser also rejects a `CREATE TABLE`
target whose final unquoted identifier component is an affected function name
immediately followed by the table-definition `(`.

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
5. Add whole seconds using MyLite-owned Gregorian date arithmetic.
6. Return `YYYY-MM-DD HH:MM:SS` text when the result is within the supported
   scalar result range.

### Diagnostics

Supported in-range expressions return successfully with `warning_count == 0`.
Unsupported or out-of-scope forms fail deterministically through the existing
MyLite unsupported-diagnostic path unless the existing parser rejects them
earlier.

Required diagnostics:

- syntax errors: existing parser syntax diagnostics;
- wrong `DATE_ADD()` shape: existing parser syntax diagnostics for forms that
  do not match `DATE_ADD(expr, INTERVAL expr SECOND)`;
- first argument is not string/`NULL`: `DATE_ADD() supports only date or
  datetime string literals and NULL`;
- string input contains embedded NUL: `DATE_ADD() date literals do not support
  NUL bytes`;
- invalid or unsupported input date/datetime string:
  `DATE_ADD() supports only canonical YYYY-MM-DD or YYYY-MM-DD HH:MM:SS values`;
- interval value is not integer/`NULL`:
  `DATE_ADD() INTERVAL SECOND supports only signed integer literals and NULL`;
- interval integer is outside signed 64-bit range:
  `DATE_ADD() INTERVAL SECOND literals must fit the signed 64-bit range`;
- result year cannot be represented in this baseline:
  `DATE_ADD() result is outside the supported datetime range`;
- allocation failure: existing `MYLITE_NOMEM` diagnostic behavior;
- public API misuse: no public API changes.

## Architecture

- Public API: unchanged. Successful `SELECT` and `DO` use existing
  `mylite_execute()` and result APIs.
- Statement context: unchanged except that the parser receives the existing
  SQL-mode parser/lexer flags, so double quotes, backslash escapes, and
  `IGNORE_SPACE` function-name parsing match the session state already
  implemented.
- Lexer/parser/AST: add explicit tokens and one `DATE_ADD` AST node kind. Do
  not introduce general function-call parsing.
- Analyzer/planner/runtime: no catalog resolution and no table planning are
  needed for this no-source scalar slice. The evaluator uses MyLite-owned
  string decoding, temporal validation, signed integer parsing, and date
  arithmetic.
- Catalog: untouched. `DATE_ADD()` does not read or mutate descriptors,
  descriptor caches, catalog generation, or `sqlite_schema_generation`.
- Result builder: returns a single text/`NULL` scalar cell through existing
  scalar-result paths. Column labels come from the original expression span or
  explicit alias.
- Storage/VFS/file format: untouched. The function is pure scalar evaluation
  and must not modify SQLite payload or the MyLite preamble.
- SQLite: no public SQLite API hook and no SQLite fork patch are required.

## Tests

Add fast C coverage under `packages/libmylite/tests/`:

- parser accepts supported `DATE_ADD(... INTERVAL ... SECOND)` forms and
  preserves spans;
- default SQL mode rejects `DATE_ADD (...)`, while `IGNORE_SPACE` accepts it;
- default SQL mode rejects `CREATE TABLE date_add(id INT)` but accepts
  `CREATE TABLE date_add (id INT)`;
- `DATE_ADD` and `SECOND` remain usable as unquoted identifiers where MySQL
  keeps them nonreserved;
- `IGNORE_SPACE` rejects unquoted `DATE_ADD` as an identifier and accepts quoted
  `` `date_add` ``;
- `IGNORE_SPACE` still accepts `SECOND` as an unquoted identifier;
- no-source, `FROM DUAL`, aliases, `DO`, `NULL`, signed intervals, date-only
  input, leap-day rollover, and independent handle behavior;
- current SQL mode behavior for double quotes and backslashes;
- deterministic unsupported diagnostics for bad first arguments, unsupported
  interval operands, out-of-range interval integers, invalid date text,
  unsupported units, and result overflow;
- file preamble remains unchanged across scalar evaluation.

Add a MySQL 8.4.9 expectation artifact:

```sh
./packages/libmylite/tests/mysql_baseline_date_add_second_expectations.sh
```

## Compatibility Updates

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/functions-temporal.md`
- `docs/compatibility/sql-query-expressions.md`
- `docs/compatibility/type-system-literals-conversion.md`

Use limited wording. Do not claim full `DATE_ADD()`, full intervals, temporal
expression arithmetic, table-backed temporal expressions, fractional seconds,
or warning-compatible invalid-date behavior. The later
`baseline-date-sub-second-aliases` slice covers the matching limited
`DATE_SUB()`, `ADDDATE()`, and `SUBDATE()` interval-second forms.
