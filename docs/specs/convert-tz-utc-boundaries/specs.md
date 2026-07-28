# `CONVERT_TZ()` UTC Boundaries

## Status

Specified; implementation and release qualification are pending.

## Summary

MyLite currently applies the difference between two fixed UTC offsets to every
canonical datetime between years 1 and 9999. MySQL 8.4.9 first normalizes the
input from the source zone to UTC and converts it only when that UTC instant
falls within the platform-supported `CONVERT_TZ()` interval. On the pinned
64-bit runtime, that interval is:

- `1970-01-01 00:00:01.000000` UTC, inclusive;
- `3001-01-18 23:59:59.999999` UTC, inclusive.

When source-zone normalization falls outside that interval, MySQL returns the
original datetime value unchanged. It does not apply the target offset, return
`NULL`, or append a warning. Target-zone conversion may produce a displayed
value outside the UTC interval.

This feature gives the existing fixed-offset MyLite subset the same boundary
behavior and adds canonical fractional-second inputs with one through six
digits. Named zones, relaxed datetime syntax, coercion, and result-metadata
parity remain separately tracked gaps.

## Sources

- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MyLite baseline temporal specification:
  `docs/specs/baseline-scalar-period-timezone-weight-functions/specs.md`
- MyLite fixed-offset implementation:
  `packages/libmylite/src/runtime/mylite_convert_tz.c`
- Follow-up review finding `SEM-02`:
  `docs/architecture/review-2026-07-followup-remediation-plan.md`
- MySQL 8.4 date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- MySQL 8.4 date, datetime, and timestamp types:
  <https://dev.mysql.com/doc/refman/8.4/en/datetime.html>
- MySQL 8.4 time-zone support:
  <https://dev.mysql.com/doc/refman/8.4/en/time-zone-support.html>
- MySQL 8.4.9 x86-64 observations pinned by
  `packages/libmylite/tests/mysql_convert_tz_utc_boundaries_expectations.sh`.

The specification is independently authored from public documentation,
observed MySQL behavior, and MyLite source. It does not use MySQL server
implementation source.

## Scope

The feature applies to the existing `CONVERT_TZ(datetime_value, from_tz,
to_tz)` execution surfaces:

- no-source and `FROM DUAL` scalar projection;
- `DO`;
- descriptor-backed single-table projection, predicate, and ordering;
- supported single-table `UPDATE` and duplicate-key update expressions;
- the internal SQLite scalar used by row-backed execution.

The admitted values remain deliberately narrow:

- a canonical datetime string in `YYYY-MM-DD HH:MM:SS` form, optionally
  followed by `.` and one through six ASCII fractional digits;
- fixed offsets in exactly `+HH:MM` or `-HH:MM` form;
- `NULL`, with the existing propagation behavior.

This work does not add named time zones, time-zone table loading, leap-second
tables, relaxed datetime spelling, numeric datetime coercion, or additional
expression contexts.

## MySQL 8.4.9 Observations

The pinned runtime reports `8.4.9` on `x86_64`. Its observed UTC interval and
boundary behavior are:

| Source-local input | Source zone | Normalized UTC | Target zone | Result |
| --- | --- | --- | --- | --- |
| `1970-01-01 00:00:00.999999` | `+00:00` | Below minimum | `+01:00` | Original input |
| `1970-01-01 00:00:01.000000` | `+00:00` | Minimum | `+01:00` | `1970-01-01 01:00:01.000000` |
| `1969-12-31 10:01:01.000000` | `-13:59` | Minimum | `+14:00` | `1970-01-01 14:00:01.000000` |
| `1970-01-01 14:00:01.000000` | `+14:00` | Minimum | `-13:59` | `1969-12-31 10:01:01.000000` |
| `3001-01-18 23:59:59.999999` | `+00:00` | Maximum | `+01:00` | `3001-01-19 00:59:59.999999` |
| `3001-01-18 10:00:59.999999` | `-13:59` | Maximum | `+14:00` | `3001-01-19 13:59:59.999999` |
| `3001-01-19 13:59:59.999999` | `+14:00` | Maximum | `-13:59` | `3001-01-18 10:00:59.999999` |
| `3001-01-19 00:00:00.000000` | `+00:00` | Above maximum | `+01:00` | Original input |

These probes establish that range testing occurs after applying the source
offset and before applying the target offset. A valid target result may fall
before the UTC minimum or after the UTC maximum. No warning is appended for
either out-of-range no-conversion case.

The runtime preserves the input fractional precision for canonical string
arguments: `.1`, `.12`, and `.123456` remain one, two, and six digits in the
result. Fractions participate in the lower and upper comparisons. Gregorian
leap-day crossings preserve the fraction and use ordinary proleptic-Gregorian
calendar arithmetic.

Canonical values such as years 1, 999, 1000, and 9999 are returned unchanged
when their normalized UTC instants are outside the interval. This behavior is
distinct from an invalid datetime: an invalid leap day still returns `NULL`
with warning `1292`.

## MyLite Semantics

For non-`NULL`, syntactically valid admitted inputs, MyLite must:

1. parse and validate the datetime, including zero to six fractional digits;
2. parse and validate both fixed offsets;
3. convert the source-local whole-second value to UTC by subtracting the
   source offset;
4. compare the UTC whole second and fraction against the inclusive x86-64
   MySQL 8.4.9 interval;
5. copy the original datetime text unchanged when the UTC instant is outside
   that interval;
6. otherwise add the target offset to UTC, convert back to a civil datetime,
   and format the result with the input fractional precision.

The architecture-independent MyLite implementation intentionally exposes the
pinned 64-bit MySQL contract on every supported host. A 32-bit MyLite build
must not silently substitute MySQL's narrower 32-bit interval.

Range preservation is not an error path. It returns a non-`NULL` result,
allocates only the result string, appends no warning, and leaves the connection
usable. The original string copy is length-aware and does not rely on caller
NUL termination.

## Fractional Seconds

The base datetime is 19 bytes. A fractional suffix is valid only when:

- byte 19 is `.`;
- between one and six following bytes exist;
- every following byte is an ASCII decimal digit.

The parsed fraction is stored as microseconds for comparison and as its
original precision for formatting. Missing lower-order digits are
right-padded only in the internal microsecond value; formatted results retain
the original number of digits. No rounding occurs.

Examples:

| Input fraction | Internal microseconds | Result suffix |
| --- | ---: | --- |
| none | 0 | none |
| `.1` | 100000 | `.1` |
| `.12` | 120000 | `.12` |
| `.000001` | 1 | `.000001` |
| `.999999` | 999999 | `.999999` |

Seven or more fractional digits remain outside the admitted subset and follow
the existing invalid-datetime `NULL` plus warning behavior.

## Errors, Warnings, And SQL Modes

Existing behavior remains:

- a `NULL` argument returns `NULL`;
- an invalid datetime returns `NULL` and appends warning `1292/HY000`;
- an invalid or unsupported zone returns `NULL` without a new warning;
- an incorrect argument count returns error `1582/42000`;
- allocation failure remains a MyLite allocation error with valid cleanup.

An out-of-range normalized UTC instant returns the original value without a
warning in strict and non-strict modes. The boundary itself is independent of
`sql_mode`, the session `time_zone`, and the host process time zone because
both admitted zones are explicit fixed offsets.

## Result Metadata

This feature changes value semantics only. The existing result descriptor is
left unchanged. Function- and argument-specific type, collation, length,
decimals, flags, nullability, and prepared-protocol metadata are addressed by
the separate `SEM-03` remediation and must not be claimed here.

## Resource And Ownership Contract

Parsing, range comparison, and civil-date conversion remain constant-time and
use fixed-size stack state. Exactly one result allocation is required for a
non-`NULL` result, whether converted or preserved. No time-zone database,
locale API, environment lookup, SQLite fork hook, or new dependency is
introduced.

The public `mylite_convert_tz_value()` ownership contract remains unchanged:
on success, `out_text` is either a caller-owned allocation or `NULL` when
`out_is_null` is true. Failure must not publish a partial allocation.

## Storage, ABI, And Compatibility

The change does not alter:

- the `.mylite` file format or catalog schema;
- transaction, VFS, or recovery behavior;
- the public ABI;
- SQLite integration or fork patches;
- named-zone or leap-second placeholder decisions.

The baseline `CONVERT_TZ()` compatibility row remains scoped to its documented
fixed-offset surface. A focused UTC-boundary row remains yellow until native
and MySQL differential tests pass all release gates.

## Test Plan

The MySQL 8.4.9 fixture must pin:

- the exact x86-64 runtime identity;
- one microsecond below and exactly at the lower bound;
- exactly at and one microsecond above the upper bound;
- both positive and negative extreme source offsets at both boundaries;
- target-offset results below and above the UTC interval;
- unchanged out-of-range values and zero warnings;
- one-, two-, and six-digit fractional precision;
- leap-day crossings in both directions;
- canonical distant years that remain unchanged.

The focused native runtime test must exercise the same matrix through direct
scalar execution and the internal row-backed SQLite function. It must also
cover `NULL`, invalid fractional input, warning preservation, non-NUL-
terminated input through `mylite_convert_tz_value()`, and allocation-failure
cleanup where the existing fault profile can target the result allocation.

Qualification must include Development, Debug-CI, Release, ASan/UBSan, and
deterministic allocator profiles; affected scalar and row-scalar runtime
suites; pinned temporal MySQL fixtures; formatting; full static analysis;
ABI/install-consumer checks; compatibility validation; and the production
size gate.
