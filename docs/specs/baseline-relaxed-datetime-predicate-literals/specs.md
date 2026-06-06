# Baseline Relaxed DATETIME Predicate Literals

## Summary

This slice admits a narrow MySQL-compatible `DATETIME` predicate literal shape
used by WordPress calendar queries:

```sql
datetime_column < '2025-2-01'
datetime_column > '2025-2-28 23:59:59'
```

MyLite normalizes these decoded string literals to canonical
`YYYY-MM-DD HH:MM:SS` before binding predicate parameters. Date-only forms
compare as midnight. This does not add broad relaxed temporal parsing,
alternate delimiters, fractional seconds, two-digit years, numeric temporal
literals, trailing garbage, or temporal literal introducers.

## Compatibility Authority

Observed against MySQL 8.4.9:

- `DATETIME` predicates accept `YYYY-M-D`, `YYYY-M-DD`, `YYYY-MM-D`, and
  `YYYY-MM-DD` date-only strings.
- Missing time compares as `00:00:00`.
- The same date forms with one space or `T` plus canonical two-digit
  `HH:MM:SS` compare as that exact datetime.
- These admitted forms record no warnings.
- Existing SQL-mode checks for zero dates, partial-zero dates, and invalid
  calendar dates still apply after normalization.

The runtime probes are captured in
`packages/libmylite/tests/mysql_baseline_datetime_type_expectations.sh`.

## Scope

Supported decoded string forms for `DATETIME` descriptor predicates:

```text
YYYY-M-D
YYYY-M-DD
YYYY-MM-D
YYYY-MM-DD
YYYY-M-D HH:MM:SS
YYYY-M-DD HH:MM:SS
YYYY-MM-D HH:MM:SS
YYYY-MM-DD HH:MM:SS
YYYY-M-DTHH:MM:SS
YYYY-M-DDTHH:MM:SS
YYYY-MM-DTHH:MM:SS
YYYY-MM-DDTHH:MM:SS
```

The year must have exactly four digits. Month and day may have one or two
digits. Time, when present, must use two digits for hour, minute, and second.

## Implementation

No grammar changes are required. Existing string-literal predicate operands are
decoded by the predicate planner. For `DATETIME` columns, MyLite first tries the
existing ISO/offset/trailing-`Z` paths, then tries this relaxed date-only/date
time normalization before the canonical fallback.

The normalized text is still validated by the existing
`predicate_datetime_text_admitted()` SQL-mode path and then bound as a prepared
statement parameter. SQLite still executes the predicate; MyLite does not
materialize rows or evaluate the comparison in memory.

## Tests

- MySQL expectation coverage in
  `mysql_baseline_datetime_type_expectations.sh`.
- Runtime coverage in `runtime_datetime_type_test.c` for relaxed date-only
  equality and WordPress-shaped range predicates.
