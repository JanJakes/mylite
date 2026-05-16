# Baseline ISO Temporal Predicate Literals

## Summary

This feature widens the existing descriptor-backed temporal predicate literal
subset for `DATETIME` and `TIMESTAMP` columns. MyLite already accepts canonical
`YYYY-MM-DD HH:MM:SS` string literals in supported `SELECT`, `UPDATE`, and
`DELETE` predicates. This slice adds:

- `YYYY-MM-DDTHH:MM:SS` string predicates, normalized to the existing canonical
  space separator;
- `YYYY-MM-DD HH:MM:SS+HH:MM` and `YYYY-MM-DDTHH:MM:SS+HH:MM` predicates;
- the corresponding negative offset forms with `-HH:MM`;
- the same conversion for comparison predicates, `<=>`, `BETWEEN`, `IN`, and
  `NOT` variants already admitted by the current descriptor predicate planner.

It does not add general temporal conversion. Row values, defaults, generated
defaults, scalar expressions, function arguments outside existing surfaces,
fractional seconds, relaxed delimiters, two-digit years, numeric temporal
literals, ODBC temporal literals, typed temporal literals, and trailing `Z`
warning/truncation behavior remain separate work.

## Compatibility Authority

Primary references:

- MySQL 8.4 Reference Manual, date and time literals:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-literals.html
- MySQL 8.4.9 runtime probes recorded by
  `packages/libmylite/tests/mysql_baseline_iso_temporal_predicate_literals_expectations.sh`

The MySQL manual states that `T` may separate the date and time parts of a
datetime string and that `DATETIME` and `TIMESTAMP` values may carry appended
numeric time-zone offsets. It also documents important offset constraints:
offset hour values below 10 require a leading zero, `-00:00` is rejected, and
named zones are not admitted in temporal literal offsets.

Observed MySQL 8.4.9 behavior used by this slice:

- `DATETIME` and `TIMESTAMP` predicates with a `T` separator and no offset are
  accepted with no warnings.
- Offset predicates using `+00:00`, `+01:00`, `-01:00`, `+14:00`, and `-14:00`
  are accepted with no warnings when the resulting value is in range.
- Offset predicates using a one-digit hour such as `+1:00`, `-00:00`, or
  `+14:01` fail with `1525 / HY000`; the final diagnostic text names the
  target temporal type as `Incorrect DATETIME value` or `Incorrect TIMESTAMP
  value`.
- For `DATETIME`, a numeric offset literal is converted to the current session
  time zone's wall-clock value before comparison.
- For `TIMESTAMP`, a numeric offset literal compares to the UTC instant. MyLite
  currently stores and returns timestamp descriptor text in its fixed UTC
  baseline, so this slice converts explicit-offset timestamp predicates to that
  fixed UTC storage text. Full session time-zone conversion for timestamp
  storage/readback remains deferred by the baseline time-zone and timestamp
  specs.
- A trailing `Z` is not treated by MySQL 8.4.9 as a clean UTC designator in
  this string-predicate context. `SELECT` predicates truncate and warn with
  `1292`, while strict mutating statements can error. MyLite intentionally
  defers that warning/error behavior to a separate diagnostics-focused slice
  rather than silently normalizing `Z` to UTC.

## Ownership Boundary

- Public API: no public ABI changes. Results, affected rows, warnings, and
  diagnostics continue through the existing `mylite_execute()` and result APIs.
- Lexer/parser/AST: no new tokens or AST nodes are required. The admitted
  inputs are ordinary SQL string literals in already-supported predicate value
  positions.
- Analyzer/planner/runtime: descriptor resolution remains authoritative. The
  predicate planner converts admitted temporal string literals before SQLite SQL
  is generated and before values are bound.
- Catalog: no descriptor rows, table descriptors, catalog generations, or
  schema text are mutated by temporal predicate conversion.
- Storage/VFS: no file-format, preamble, VFS, or SQLite payload changes.
- SQLite execution: SQLite remains a physical row store and predicate executor.
  MyLite owns temporal literal validation and canonicalization, then binds the
  converted text with prepared-statement parameters. No SQLite date parser and
  no SQLite fork patch are used.

## Syntax

No grammar expansion is needed. The existing predicate literal grammar continues
to admit string literals in descriptor-backed predicate value positions.

Independently authored MyLite Lemon-shape snippet:

```text
predicate_value ::= string_literal.
predicate_value ::= NULL.

comparison_predicate ::= column_reference comparison_operator predicate_value.
between_predicate ::= column_reference opt_not BETWEEN predicate_value AND predicate_value.
in_predicate ::= column_reference opt_not IN LP predicate_value_list RP.
```

Semantic narrowing for this slice:

- the target descriptor must be `DATETIME` or `TIMESTAMP`;
- the literal must decode to one of the admitted second-precision forms;
- no embedded `NUL` bytes are admitted;
- no parameters, expressions, column references, functions, subqueries, typed
  temporal literals, numeric values, or fractional values are admitted as the
  predicate right operand by this slice.

## Conversion Rules

### T Separator

`YYYY-MM-DDTHH:MM:SS` is admitted wherever the current canonical datetime or
timestamp predicate literal is admitted. MyLite validates the resulting
date/time components with the same SQL-mode and range rules as canonical
predicate literals, then binds `YYYY-MM-DD HH:MM:SS`.

### Numeric Offsets

Admitted offset shape:

```text
YYYY-MM-DD HH:MM:SS+HH:MM
YYYY-MM-DDTHH:MM:SS+HH:MM
YYYY-MM-DD HH:MM:SS-HH:MM
YYYY-MM-DDTHH:MM:SS-HH:MM
```

The hour and minute fields must each have two digits. Supported offset range is
`-14:00` through `+14:00`, except `-00:00`, which is rejected. Offsets outside
that range, offset names, `SYSTEM`, `UTC`, bare `Z`, one-digit hour fields, and
malformed offsets are not admitted.

For `DATETIME`, MyLite interprets the literal's date/time part at the provided
offset and converts it to the active MyLite session offset before comparison.
For example, with `SET time_zone = '+02:00'`,
`'2016-01-14T22:00:00+00:00'` compares as
`'2016-01-15 00:00:00'`.

For `TIMESTAMP`, explicit-offset predicate literals are converted to fixed UTC
canonical text before comparison. This matches MyLite's current timestamp
storage invariant and the existing compatibility docs that defer full
session-time-zone timestamp read/write conversion.

Converted values must remain in the supported descriptor domain:

- `DATETIME`: valid nonzero years `1000` through `9999`, subject to existing
  zero-date SQL-mode behavior for canonical zero values;
- `TIMESTAMP`: `1970-01-01 00:00:01` through
  `2038-01-19 03:14:07`, plus existing admitted stored-zero predicate behavior
  when SQL mode permits it.

### Unsupported Forms

The following continue to fail deterministically:

- trailing `Z` or `z`;
- fractional seconds;
- relaxed date/time delimiters;
- superfluous whitespace;
- two-digit years;
- nondelimited temporal strings;
- numeric temporal literals;
- temporal keyword literals such as `TIMESTAMP '...'`;
- ODBC temporal literals;
- named time zones or `SYSTEM`/`UTC` suffixes;
- offset forms with one-digit hours such as `+1:00`;
- `-00:00`;
- offset values outside `-14:00` through `+14:00`;
- offset conversions whose resulting `DATETIME` or `TIMESTAMP` is outside the
  currently supported descriptor range.

## Diagnostics

Successful admitted predicates record no warnings.

Unsupported or invalid `DATETIME` predicate literals fail with the existing
MyLite predicate datetime diagnostic, compatible with the observed MySQL final
error for this slice: `1525 / HY000`, message containing
`Incorrect DATETIME value: '<literal>'`.

Unsupported or invalid `TIMESTAMP` predicate literals fail with `1525 / HY000`,
message containing `Incorrect TIMESTAMP value: '<literal>'`.

Trailing `Z` warning/truncation is not included in this slice because MySQL's
behavior is context-sensitive: clean `SELECT` predicates warn and continue,
while strict mutating predicates can error with row-context diagnostics. MyLite
must not silently treat `Z` as UTC until that behavior is implemented with
matching warning and strict-mode semantics.

## Physical SQLite Handling

MyLite continues to generate descriptor-built SQLite predicate SQL with
parameter placeholders. Converted temporal values are stored in planned values
and bound as text. SQLite sees only physical table names and bound canonical
strings; it does not parse temporal literals, inspect logical descriptor
metadata, or decide temporal compatibility.

No generated SQLite SQL interpolates user literal text. Identifier quoting,
stable physical table naming, and reserved-name collision checks remain owned by
the existing descriptor-driven statement paths.

## Performance

The conversion is a constant-time parser and civil-time shift per predicate
literal. It happens once during statement planning, not once per row. Predicate
execution remains inside SQLite using the existing physical query plan and
bound parameters. No row materialization is introduced.

## Tests

MySQL-runtime expectation script:

```sh
packages/libmylite/tests/mysql_baseline_iso_temporal_predicate_literals_expectations.sh
```

Fast C tests extend the existing datetime and timestamp runtime lifecycle
binaries, covering:

- `T` separator predicates;
- explicit offset predicates with `T` and space separators in comparison,
  `<=>`, `BETWEEN`, `NOT BETWEEN`, `IN`, and `NOT IN` positions;
- `DATETIME` session-offset conversion for explicit offsets;
- `TIMESTAMP` explicit-offset conversion to MyLite's fixed UTC baseline;
- `UPDATE` and `DELETE` through the shared predicate planner;
- invalid one-digit hour offsets, `-00:00`, out-of-range offsets, and trailing
  `Z` rejection in MyLite for this slice;
- absence of result rows for successful DML and zero warnings for admitted
  conversions;
- persistence and file-format tests already covered by the underlying datetime
  and timestamp lifecycle suites.

## Verification

Required before commit:

1. `packages/libmylite/tests/mysql_baseline_iso_temporal_predicate_literals_expectations.sh`
2. `cmake --build --preset dev`
3. `ctest --preset dev -R 'libmylite.runtime.(datetime_type|timestamp_type|time_zone_system_variable|sql_modes)' --output-on-failure`
4. `cmake --workflow --preset check`
