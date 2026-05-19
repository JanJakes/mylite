# Baseline DATE ISO Predicate Strings

## Summary

This feature widens the descriptor-backed `DATE` predicate subset. MyLite
already accepts canonical `YYYY-MM-DD` string literals for supported
`SELECT`, `UPDATE`, and `DELETE` predicates. This slice adds datetime-shaped
string predicates for `DATE` descriptor columns:

- `YYYY-MM-DD HH:MM:SS`;
- `YYYY-MM-DDTHH:MM:SS`;
- the same forms with numeric `+HH:MM` / `-HH:MM` offsets;
- the same forms with one trailing `Z` or `z` suffix, with a warning.

It does not add relaxed date parsing generally. Row values, defaults,
assignments, scalar expressions, casts, temporal literal introducers,
fractional seconds, two-digit years, numeric temporal values, and broader
trailing-garbage truncation remain separate work.

## Compatibility Authority

Primary references:

- MySQL 8.4 Reference Manual, date and time literals:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-literals.html
- MySQL 8.4 Reference Manual, date and time types:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-types.html
- MySQL 8.4.9 runtime probes captured by
  `packages/libmylite/tests/mysql_baseline_date_iso_predicate_strings_expectations.sh`

The MySQL manual documents date and datetime string literal formats, the
`T` separator between date and time parts, and numeric offset requirements for
datetime-like literal text. Observed MySQL 8.4.9 behavior establishes the
`DATE` predicate behavior for this slice:

- `DATE` column predicates accept datetime-shaped string literals.
- Equality to a same-day midnight literal matches the stored date; equality to
  a same-day non-midnight literal does not.
- Range comparisons compare the stored date as midnight on that date. For
  example, `d < '2016-01-15T23:59:59'` includes `2016-01-15`.
- `T` and space separators are accepted with no warnings.
- Numeric offsets with two-digit hour and minute fields are accepted with no
  warnings, but `DATE` predicate comparison does not shift the literal by the
  session time zone; MySQL compares the literal's local date/time fields.
- Offset forms with one-digit hours, `-00:00`, or values outside the supported
  `-14:00` through `+14:00` envelope fail with `1525 / HY000` and message
  shape `Incorrect DATE value: '<literal>'`.
- A single trailing `Z` or `z` is accepted by truncating that final suffix and
  recording warning `1292 / 22007` with message shape
  `Incorrect date value: '<literal>' for column '<column>' at row 1`.
- MySQL duplicates trailing-suffix warnings for `BETWEEN` during row
  evaluation. MyLite records deterministic conversion warnings once per
  admitted suffix literal, matching the existing `DATETIME`/`TIMESTAMP`
  trailing-`Z` predicate policy.

## Ownership Boundary

- Public API: no public ABI changes. `mylite_execute()` and the existing result
  APIs expose rows, affected rows, diagnostics, and warnings.
- Statement context: the existing SQL mode and time-zone state remain owned by
  the session. This feature observes SQL mode for canonical temporal validity
  and validates offset shape, but `DATE` predicates do not apply time-zone
  shifting.
- Lexer/parser/AST: no new tokens or nodes are needed. Existing string literal
  predicate operands carry the SQL surface.
- Analyzer/planner/runtime: descriptor resolution remains authoritative. The
  predicate planner decodes and validates `DATE` predicate literals before
  SQLite SQL is generated, chooses whether a `DATE` column must be compared as
  midnight datetime text, and stores prepared-statement values.
- Catalog: no descriptors, descriptor versions, catalog rows, catalog
  generations, or SQLite schema-generation counters are mutated.
- Storage/VFS: no `.mylite` preamble, file format, VFS, or SQLite payload
  changes.
- SQLite execution: this is a MyLite wrapper/translation change. SQLite remains
  the physical row store and predicate executor. No SQLite date parser and no
  SQLite fork patch are used.

## Syntax

No grammar expansion is required. The existing predicate grammar already admits
string literals in descriptor-backed value positions.

Independently authored MyLite Lemon-shape snippet:

```lemon
predicate_value ::= string_literal.
predicate_value ::= NULL.

comparison_predicate ::= column_reference comparison_operator predicate_value.
between_predicate ::= column_reference opt_not BETWEEN predicate_value AND predicate_value.
in_predicate ::= column_reference opt_not IN LPAREN predicate_value_list RPAREN.
predicate_value_list ::= predicate_value.
predicate_value_list ::= predicate_value_list COMMA predicate_value.
```

Semantic narrowing for this slice:

- the resolved descriptor column must be `DATE`;
- non-`NULL` literal operands must decode to one of the admitted
  second-precision forms below;
- no embedded `NUL` bytes are admitted;
- no parameters, functions, column-to-expression comparisons, subqueries,
  temporal introducers, numeric temporal values, fractional values, or general
  expressions are added by this slice.

## Literal Conversion

Canonical `YYYY-MM-DD` operands keep the existing `DATE` predicate behavior.

Datetime-shaped operands are admitted in these exact forms:

```text
YYYY-MM-DD HH:MM:SS
YYYY-MM-DDTHH:MM:SS
YYYY-MM-DD HH:MM:SS+HH:MM
YYYY-MM-DDTHH:MM:SS+HH:MM
YYYY-MM-DD HH:MM:SS-HH:MM
YYYY-MM-DDTHH:MM:SS-HH:MM
YYYY-MM-DD HH:MM:SSZ
YYYY-MM-DDTHH:MM:SSZ
YYYY-MM-DD HH:MM:SSz
YYYY-MM-DDTHH:MM:SSz
```

For no-suffix forms, MyLite validates the normalized
`YYYY-MM-DD HH:MM:SS` text against the existing temporal SQL-mode rules and
binds that normalized text.

For numeric-offset forms, MyLite validates the offset spelling and range:

- hour and minute fields must each have two digits;
- `-00:00` is rejected;
- the absolute offset must not exceed `14:00`;
- named zones and `SYSTEM` are not admitted.

Unlike `DATETIME` predicates, `DATE` predicates do not apply a time-zone shift.
The first 19 characters are normalized to `YYYY-MM-DD HH:MM:SS`, validated,
and bound.

For single trailing `Z` / `z` forms, MyLite copies the first 19 characters,
normalizes `T` to a space, validates the result, appends warning
`1292 / 22007`, and binds the normalized text. The suffix is not treated as a
UTC designator.

## Predicate Semantics

When every non-`NULL` operand in a `DATE` predicate is canonical
`YYYY-MM-DD`, MyLite keeps the existing direct date-text comparison.

When any non-`NULL` operand in a comparison, `BETWEEN`, or literal `IN` list is
datetime-shaped, MyLite compares the stored `DATE` value as midnight datetime
text by generating a descriptor-built SQLite expression equivalent to:

```sql
date_column || ' 00:00:00'
```

All canonical `YYYY-MM-DD` operands in that same predicate are converted to
`YYYY-MM-DD 00:00:00` before binding. This preserves MySQL's mixed date and
datetime predicate behavior while still pushing the predicate into SQLite.

Examples:

- `d = '2016-01-15T00:00:00'` matches stored `2016-01-15`.
- `d = '2016-01-15T23:59:59'` does not match stored `2016-01-15`.
- `d < '2016-01-15T23:59:59'` includes stored `2016-01-15`.
- `d BETWEEN '2016-01-15T00:00:00' AND '2016-01-15T23:59:59'` includes stored
  `2016-01-15`.

`IS NULL`, `IS NOT NULL`, ordering, projection, assignment, and storage remain
unchanged.

## Diagnostics

Successful no-suffix and numeric-offset datetime-shaped `DATE` predicate
literals record no warnings.

Successful trailing-`Z` / `z` forms append:

```text
Warning 1292 22007 Incorrect date value: '<literal>' for column '<column>' at row 1
```

MyLite records one warning per converted suffix literal. It does not duplicate
warnings per evaluated row.

Invalid or unsupported `DATE` predicate strings fail with:

```text
1525 / HY000 Incorrect DATE value: '<literal>'
```

This includes malformed offsets such as `+1:00`, `-00:00`, `+14:01`,
fractional seconds, relaxed delimiters, superfluous whitespace, numeric
temporal strings, temporal literal introducers, named zones, and broader
trailing garbage such as `Z+00:00`. MySQL accepts some broader trailing
garbage by truncating with warning; MyLite deliberately leaves that broader
surface to the existing deferred trailing-garbage compatibility item.

## Physical SQLite Handling

MyLite continues to generate SQL from descriptors and stable physical table
names. User literal text is never interpolated into SQL. Converted predicate
values are bound with prepared-statement parameters.

For datetime-shaped `DATE` predicates, only the left-hand descriptor term
changes from the direct date column to a deterministic expression appending
midnight time text. The physical stored value remains canonical `YYYY-MM-DD`.
SQLite performs the comparison with bound text values. No row materialization,
temporary row cache, or client-side filtering is introduced.

## Performance

Literal decoding and conversion happen once during statement planning. SQLite
still evaluates the predicate over the physical table. The datetime-shaped
path adds a simple concatenation expression to the predicate term, which is the
smallest faithful implementation for this baseline because MyLite currently
stores `DATE` as canonical text without a parallel datetime key.

## Test Plan

MySQL-runtime expectation script:

```sh
packages/libmylite/tests/mysql_baseline_date_iso_predicate_strings_expectations.sh
```

Fast C coverage extends `runtime_date_type_test.c` and covers:

- equality to midnight and non-midnight datetime-shaped literals;
- `<`, `<=`, `>`, `>=`, `<=>`, `BETWEEN`, `NOT BETWEEN`, `IN`, and `NOT IN`
  predicate reuse;
- space and `T` separators;
- numeric offsets with no `DATE` time-zone shift;
- trailing `Z` and `z` warning count and warning text;
- `UPDATE` and `DELETE` through the shared predicate planner;
- deterministic rejection of malformed offsets and deferred trailing garbage;
- persistence, file-format preamble, ordering, and independent-handle coverage
  through the existing `DATE` lifecycle tests.

## Verification

Required before commit:

1. `MYLITE_MYSQL_BIN=/opt/homebrew/bin/mysql MYLITE_MYSQL_SOCKET=/tmp/mylite-mysql-849.jsgoZE/mysql.sock packages/libmylite/tests/mysql_baseline_date_iso_predicate_strings_expectations.sh`
2. `cmake --build --preset dev`
3. `ctest --preset dev -R 'libmylite.runtime.(date_type|datetime_type|update_lifecycle|delete_lifecycle)' --output-on-failure`
4. `cmake --workflow --preset check`
