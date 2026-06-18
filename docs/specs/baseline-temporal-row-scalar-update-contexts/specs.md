# Baseline Temporal Row-Scalar UPDATE Contexts

## Goal

This slice closes compatible non-key single-table `UPDATE` assignment context
gaps for existing temporal row-scalar functions. It does not add new temporal
function semantics, operand domains, relaxed parsing, fractional precision,
time-zone behavior, defaults, generated-column support, grouping support, or
key-target assignment support.

Covered functions:

- `DATE()`, `TIME()`, `DATE_FORMAT()`, `TIME_FORMAT()`, and `STR_TO_DATE()`;
- `DATEDIFF()`, `TIMEDIFF()`, `TIMESTAMP()`, `TIMESTAMPADD()`, and
  `TIMESTAMPDIFF()`;
- `UNIX_TIMESTAMP()`, `SEC_TO_TIME()`, `FROM_UNIXTIME()`, `FROM_DAYS()`,
  `MAKEDATE()`, `MAKETIME()`, `TIME_TO_SEC()`, `TO_DAYS()`, and
  `TO_SECONDS()`;
- `DAYOFMONTH()` / `DAY()`, `DAYOFWEEK()`, `DAYOFYEAR()`, `DAYNAME()`,
  `LAST_DAY()`, `HOUR()`, `MINUTE()`, `SECOND()`, `MICROSECOND()`,
  `MONTH()`, `MONTHNAME()`, `QUARTER()`, `WEEK()`, `WEEKDAY()`,
  `WEEKOFYEAR()`, `YEAR()`, `YEARWEEK()`, and `EXTRACT()`.

Already supported temporal assignment surfaces such as current date/time
functions, `SYSDATE()`, `ADDTIME()`, `SUBTIME()`, date interval updates,
`CONVERT_TZ()`, `PERIOD_ADD()`, and `PERIOD_DIFF()` are preserved.

## Compatibility Authority

Normative behavior comes from official MySQL 8.4 documentation and MySQL 8.4.9
runtime observations:

- <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/update.html>

The exact expected rows and per-statement changed-row counts are captured in
`packages/libmylite/tests/mysql_baseline_temporal_row_scalar_update_contexts_expectations.sh`
and verified against MySQL 8.4.9.

## Semantics

MyLite may plan a covered function as a row-scalar single-table `UPDATE`
assignment when the existing assignment rules accept the target: the target
must be a compatible non-key column, must not be `AUTO_INCREMENT`, and must use
the existing target conversion path.

Function values, `NULL` behavior, warnings, diagnostics, SQL modes, temporal
range handling, time-zone handling, and conversion behavior are delegated to
the existing row-scalar temporal function implementations. This slice
intentionally stays inside their current documented operand subset.

Joined and multi-table `UPDATE` statements remain outside this slice.

## Syntax

The parser retry layer may replace a direct covered function call in a
single-table `UPDATE` assignment value with a row-scalar placeholder when the
argument list contains a row operand. The same placeholder admission applies to
the existing compatible duplicate-key update assignment path. The original AST
is retained for runtime planning.

Intended admission shape:

```lemon
update_value(A) ::= row_scalar_temporal_function_call(B). {
    A = B;
}
```

For duplicate-key assignments, the retry layer uses an integer placeholder
accepted by the existing `insert_value` grammar and replaces that placeholder
with the retained original AST after parsing. This keeps ordinary identifier
copy assignments outside this slice.

## SQLite Integration

This slice uses MyLite-side parsing/planning and existing SQLite scalar UDFs.
It does not require a SQLite fork hook.

## Test Plan

- Add a MySQL 8.4.9 expectation script covering successful one-row
  assignments, per-update changed-row counts, final rows, and final diagnostic
  counts.
- Add a C runtime test with the same assignments and a joined-update rejection
  regression proving joined and multi-table updates remain outside the slice.
- Add a duplicate-key regression for a covered temporal row-scalar assignment
  whose function arguments consume multiple SQLite parameters.
- Run the new test, focused temporal/parser retry/update tests, MySQL
  expectation scripts, diff checks, clang-tidy for touched C files, and the
  full `cmake --workflow --preset check` gate.

## Compatibility Impact

Temporal rows should no longer list compatible non-key single-table `UPDATE`
assignments as missing for the covered functions. Rows remain yellow where
independent gaps still exist, including relaxed temporal parsing, broader units
or argument domains, numeric coercion, fractional seconds, time-zone fidelity,
grouping/default/generated-column support, unsupported predicates, and full
expression metadata.
