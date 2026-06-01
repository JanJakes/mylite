# Baseline Scalar Period, Time Zone, and Weight Functions

## Scope

This slice adds a limited MySQL 8.4.9-compatible baseline for:

- `PERIOD_ADD(period, months)`
- `PERIOD_DIFF(period1, period2)`
- `CONVERT_TZ(datetime_value, from_tz, to_tz)`
- `WEIGHT_STRING(value)`
- `WEIGHT_STRING(value AS BINARY(length))`

The compatibility authority is official MySQL 8.4 documentation for date/time
and string functions plus MySQL 8.4.9 runtime probes captured in
`packages/libmylite/tests/mysql_baseline_scalar_period_timezone_weight_expectations.sh`.

## MySQL-Observed Behavior

`PERIOD_ADD()` and `PERIOD_DIFF()` use year-month period values in `YYMM` or
`YYYYMM` form. Two-digit years map like MySQL's period functions: `00` through
`69` mean 2000 through 2069, and `70` through `99` mean 1970 through 1999.
Invalid periods such as `0` or a month outside `1..12` raise error
`1210/HY000` with an incorrect-arguments message. `NULL` inputs return `NULL`.

`CONVERT_TZ()` returns a converted `YYYY-MM-DD HH:MM:SS` value for valid
datetime text and valid offset time zones such as `+00:00` and `-05:30`.
`NULL` inputs return `NULL`. Invalid datetime text returns `NULL` and appends
warning `1292`. Invalid time zone names or offsets return `NULL` without a new
warning in the observed fixed-offset probes.

`WEIGHT_STRING()` returns binary bytes. With `AS BINARY(length)`, the result is
truncated or right-padded with zero bytes to the requested byte length. MySQL's
full `AS CHAR`, collation-weight, and flag behavior depends on collation weight
tables and remains outside this slice.

## MyLite Semantics

The period functions are implemented for `NULL`, boolean, signed integer
literals, and signed-integer descriptor columns. String, decimal, floating, and
warning-producing coercions are intentionally deferred.

`CONVERT_TZ()` is implemented for canonical `YYYY-MM-DD HH:MM:SS` inputs and
literal or descriptor-backed fixed-offset time zone strings in `+HH:MM` or
`-HH:MM` form. Named time zones remain placeholder behavior until time zone
tables and `mysql_tzinfo_to_sql` loading exist. Leap seconds are not modeled.

`WEIGHT_STRING()` is implemented as binary byte identity for admitted scalar
values and descriptor-backed byte/text values. `AS BINARY(length)` supports
unsigned integer literal lengths. Full collation weights, `AS CHAR`, `LEVEL`,
`ASC` / `DESC` / `REVERSE` flags, and nonbinary collation parity are deferred.

The implementation uses public SQLite scalar function registration for
row-backed execution and MyLite wrapper/direct evaluation for no-source,
`DUAL`, and `DO` scalar evaluation. No SQLite fork hook or storage-format change
is needed.

## Grammar

The MyLite Lemon grammar extension is independently authored:

```lemon
expression ::= PERIOD_ADD LPAREN expression COMMA expression RPAREN.
expression ::= PERIOD_DIFF LPAREN expression COMMA expression RPAREN.
expression ::= CONVERT_TZ LPAREN expression COMMA expression COMMA expression RPAREN.
expression ::= WEIGHT_STRING LPAREN expression RPAREN.
expression ::= WEIGHT_STRING LPAREN expression AS BINARY LPAREN INTEGER RPAREN RPAREN.
```

Argument-count error forms are accepted for the native function names and
reported as MySQL `1582/42000` where the parser can unambiguously recognize the
function call.

## Tests

The MySQL expectation script verifies runtime-observed values, `NULL`
propagation, invalid-period errors, fixed-offset conversion, invalid datetime
warnings, binary padding/truncation shape, and deferred MySQL-accepted surfaces.

The MyLite runtime test covers:

- no-source, `FROM DUAL`, and `DO` scalar execution;
- table-backed scalar projection over supported descriptor columns;
- fixed-offset `CONVERT_TZ()` results and invalid inputs;
- `HEX(WEIGHT_STRING(...))` and `HEX(WEIGHT_STRING(... AS BINARY(N)))`;
- native argument-count diagnostics and period invalid-argument diagnostics.

## Compatibility Status

The related `COMPATIBILITY.md` rows move from `❌` to `🟡` because the functions
are now accepted and executable for a documented baseline subset, but several
MySQL coercion, collation, named-zone, and optional-syntax behaviors remain
unimplemented.
