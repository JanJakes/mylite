# Baseline Time Zone System Variable

## Status

This feature adds the first MyLite-owned `time_zone` and
`system_time_zone` system-variable slice for common client bootstrap and
deterministic current-time behavior.

The slice is deliberately narrow. MyLite exposes a fixed embedded system time
zone of `UTC`, exposes a fixed global `time_zone` value of `SYSTEM`, and lets a
connection set its own session `time_zone` to `SYSTEM`, `UTC`, `DEFAULT`, or a
signed UTC offset. Current date/time/timestamp functions and generated current
temporal values use the session time zone for visible wall-clock values.

Full MySQL `TIMESTAMP` column time-zone conversion is not part of this slice.
Existing TIMESTAMP rows remain stored and read as MyLite's current canonical
text values; changing `time_zone` after storing rows does not reinterpret those
rows until a later feature implements MySQL's UTC storage/display conversion.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Current date/time functions:
  `docs/specs/baseline-current-date-time-functions/specs.md`
- Current timestamp defaults:
  `docs/specs/baseline-current-timestamp-defaults/specs.md`
- Baseline TIMESTAMP type: `docs/specs/baseline-timestamp-type/specs.md`
- Baseline SQL mode session state:
  `docs/specs/baseline-sql-mode-session-state/specs.md`
- MySQL 8.4 Reference Manual, MySQL Server Time Zone Support:
  https://dev.mysql.com/doc/refman/8.4/en/time-zone-support.html
- MySQL 8.4 Reference Manual, Dynamic System Variables:
  https://dev.mysql.com/doc/refman/8.4/en/dynamic-system-variables.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes were run against local container `mylite-mysql-849` using
`mysql:8.4.9`. The expectation script
`packages/libmylite/tests/mysql_baseline_time_zone_system_variable_expectations.sh`
records these observations.

Observed system-variable defaults in the test container:

```text
@@GLOBAL.time_zone = SYSTEM
@@SESSION.time_zone = SYSTEM
@@system_time_zone = UTC
```

Observed supported session assignment forms:

```sql
SET time_zone = '+00:00'
SET time_zone = '+5:30'
SET SESSION time_zone = '-6:00'
SET LOCAL time_zone = '+14:00'
SET @@time_zone = '+00:00'
SET @@SESSION.time_zone = '-13:59'
SET @@LOCAL.time_zone = '-0:00'
SET @@session.Time_Zone = '+0:00'
SET time_zone = DEFAULT
SET time_zone = 'SYSTEM'
SET time_zone = SYSTEM
SET time_zone = 'UTC'
SET time_zone = UTC
```

Successful assignments report `ROW_COUNT() = 0` and `@@warning_count = 0`.

Offset readback is canonical:

- one-digit hours gain a leading zero;
- `-00:00`, `-0:00`, `-00:0`, and similar negative-zero forms read back as
  `+00:00`;
- valid offsets are in the inclusive range `-13:59` through `+14:00`.

Invalid offsets such as `+14:01`, `-14:00`, `+00:60`, `+0`, `00:00`,
`+00:00:00`, the empty string, and strings with leading whitespace fail with
`1298 / HY000` and a message containing `Unknown or incorrect time zone`.

Non-string numeric assignments such as `SET time_zone = 0` fail with
`1232 / 42000` and a message containing
`Incorrect argument type to variable 'time_zone'`. `SET time_zone = NULL` fails
with `1231 / 42000` and a message containing
`Variable 'time_zone' can't be set to the value of 'NULL'`.

The session time zone affects visible current-time functions. With
`SET timestamp = 1700000000`, MySQL returned:

```text
time_zone  NOW()
+00:00     2023-11-14 22:13:20
+02:30     2023-11-15 00:43:20
-06:00     2023-11-14 16:13:20
UTC        2023-11-14 22:13:20
```

MySQL also converts `TIMESTAMP` column storage and retrieval through UTC. That
larger behavior remains outside this MyLite slice.

Observed global behavior:

- `@@GLOBAL.time_zone` reads the global value.
- `SET GLOBAL time_zone = '+03:00'` changes the global value when privileges
  allow it, but does not change the current session value.
- A later `SET time_zone = DEFAULT` copies the current global value into the
  session value.

MyLite does not implement mutable global system-variable state in this slice.
It keeps `@@GLOBAL.time_zone` fixed at `SYSTEM`, so `SET time_zone = DEFAULT`
always resets the session to `SYSTEM`.

## Scope

This feature adds:

- scalar reads for `@@time_zone`, `@@SESSION.time_zone`, `@@LOCAL.time_zone`,
  and `@@GLOBAL.time_zone`;
- scalar reads for global-only `@@system_time_zone` and
  `@@GLOBAL.system_time_zone`;
- `SHOW VARIABLES LIKE 'time_zone'`,
  `SHOW SESSION VARIABLES LIKE 'time_zone'`, and
  `SHOW GLOBAL VARIABLES LIKE 'time_zone'`;
- `SHOW VARIABLES LIKE 'system_time_zone'` and
  `SHOW GLOBAL VARIABLES LIKE 'system_time_zone'`;
- session-local `SET time_zone = value`,
  `SET SESSION time_zone = value`, `SET LOCAL time_zone = value`,
  `SET @@time_zone = value`, `SET @@SESSION.time_zone = value`, and
  `SET @@LOCAL.time_zone = value`;
- `DEFAULT`, string, and the MySQL-observed unquoted `SYSTEM` / `UTC` target
  values;
- accepted string values `SYSTEM`, `UTC`, and signed UTC offsets of the form
  `+H:MM`, `+HH:MM`, `-H:MM`, or `-HH:MM`;
- offset range validation, readback normalization, and MySQL-shaped
  diagnostics for the supported invalid values;
- session-time-zone-aware materialization for `CURDATE()` / `CURRENT_DATE`,
  `CURTIME()` / `CURRENT_TIME`, `NOW()` / `CURRENT_TIMESTAMP`, and existing
  generated current date/time/timestamp defaults and assignments;
- independent per-handle session time-zone state, reset on close/reopen;
- no catalog, descriptor, SQLite schema, or file-format mutation from
  `time_zone` assignments.

## Non-Goals

This feature must not implement:

- mutable global `time_zone`, persisted variables, startup options, privilege
  checks, or server-wide state shared between embedded handles;
- named time zones other than the fixed `UTC` alias, time-zone tables,
  daylight-saving rules, leap seconds, or `mysql.time_zone*` metadata rows;
- `SYSTEM` as host-local wall-clock behavior other than MyLite's fixed embedded
  UTC baseline;
- `CONVERT_TZ()`, `UTC_DATE()`, `UTC_TIME()`, `UTC_TIMESTAMP()`, temporal
  offset literals, time-zone-aware temporal arithmetic, or local calendar
  libraries;
- full `TIMESTAMP` column storage/retrieval conversion between session time
  zones and UTC;
- global/session privilege semantics, Performance Schema variable tables, or
  protocol session-state tracking;
- SQLite fork patches or SQLite date/time functions.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` owns call validation,
  statement-context setup, result ownership, diagnostics replacement, and
  failure cleanup.
- Session state owns the current handle-local `time_zone` text and a parsed
  offset in minutes for current-time materialization. It is not durable catalog
  state.
- Lexer/parser own syntax admission for generic `SET` system-variable values,
  scalar `@@...` expressions, and `SHOW VARIABLES`; they do not validate time
  zone ranges.
- Runtime execution owns target resolution, value decoding, canonicalization,
  scope checks, diagnostics, and mutation of session state.
- Current-time helpers own converting the active statement Unix timestamp into
  MyLite's visible date/time strings according to the current session offset.
- Catalog descriptors remain authoritative for table metadata. `SET time_zone`
  does not mutate schemas, tables, columns, defaults, indexes, descriptor
  versions, catalog generation, or SQLite schema generation.
- SQLite owns physical row storage. This feature keeps time-zone logic in
  MyLite code and does not rely on SQLite date/time SQL functions or optional
  compile-time extensions.
- Storage and VFS own the `.mylite` preamble and shifted SQLite payload
  boundary. Time-zone session assignments do not write the file.

## Supported Grammar

The generic MyLite `SET` grammar is extended only enough to admit the observed
unquoted time-zone names. Runtime still decides which targets may use them.

```lemon
set_statement ::= SET set_system_variable_target EQUAL set_system_variable_value.

set_system_variable_target ::= identifier.
set_system_variable_target ::= SESSION identifier.
set_system_variable_target ::= LOCAL identifier.
set_system_variable_target ::= GLOBAL identifier.
set_system_variable_target ::= SYSTEM_VARIABLE.

set_system_variable_value ::= DEFAULT.
set_system_variable_value ::= STRING.
set_system_variable_value ::= SYSTEM.
set_system_variable_value ::= UTC.
set_system_variable_value ::= INTEGER.
set_system_variable_value ::= PLUS INTEGER.
set_system_variable_value ::= MINUS INTEGER.
set_system_variable_value ::= TRUE.
set_system_variable_value ::= FALSE.
set_system_variable_value ::= ON.
set_system_variable_value ::= OFF.
```

Only `time_zone` may consume `SYSTEM` or `UTC` values in this feature.
Other mutable system-variable slices keep their current validation behavior.

## Semantics

### Defaults and Scope

MyLite initializes every handle with:

```text
time_zone = SYSTEM
system_time_zone = UTC
global time_zone = SYSTEM
```

`SYSTEM` resolves to offset `+00:00` for current-time calculations because
MyLite's embedded system time zone is fixed to UTC in this slice.

`SET time_zone = DEFAULT` and scoped session/local equivalents reset the
session `time_zone` to the fixed global value `SYSTEM`.

`SET GLOBAL time_zone = ...` remains unsupported with the existing deterministic
MyLite unsupported-global-assignment diagnostic. `@@GLOBAL.time_zone` still
reads as `SYSTEM`.

`system_time_zone` is read-only and global-only. Session/local reads fail with
the existing global-only diagnostic shape. `SET system_time_zone = ...` fails
with the read-only system-variable diagnostic.

### Value Conversion

String and unquoted names are ASCII case-insensitive.

Accepted values:

- `SYSTEM`, read back as `SYSTEM`, effective offset `0`;
- `UTC`, read back as `UTC`, effective offset `0`;
- signed offsets with one or two hour digits, one colon, and one or two minute
  digits.

Offset normalization:

- output is always `+HH:MM` or `-HH:MM`;
- hour and minute readback use two digits;
- every negative-zero spelling normalizes to `+00:00`;
- the effective offset is minutes east of UTC.

Valid range:

```text
-13:59 <= offset <= +14:00
```

Invalid offset strings and unsupported names fail with `1298 / HY000` and
`Unknown or incorrect time zone: '<value>'`.

Unsupported value types fail deterministically:

- numeric values use `1232 / 42000` with
  `Incorrect argument type to variable 'time_zone'`;
- `NULL` uses `1231 / 42000` with
  `Variable 'time_zone' can't be set to the value of 'NULL'`;
- arbitrary expressions, parameters, subqueries, and functions remain outside
  the generic `SET` grammar and keep existing syntax or unsupported diagnostics.

### Current-Time Values

At statement start, MyLite already captures one active statement Unix timestamp.
This feature formats that timestamp as:

```text
utc_epoch_seconds + session_time_zone_offset_seconds
```

The shifted epoch is converted with UTC calendar rules to produce the visible
session wall-clock values used by:

- `CURDATE()` / `CURRENT_DATE`;
- `CURTIME()` / `CURRENT_TIME`;
- `NOW()` / `CURRENT_TIMESTAMP`;
- current date/time/timestamp DML assignments and defaults already supported by
  previous slices.

`@@timestamp` itself remains the Unix timestamp value and is not shifted by
`time_zone`.

Because TIMESTAMP row conversion is deferred, stored TIMESTAMP descriptor values
are not reinterpreted at read time after a later `SET time_zone`. This must be
documented and tested as a known MyLite gap rather than silently implied to be
MySQL-compatible.

## Diagnostics

Required diagnostics:

- unknown system variable: existing `1193 / HY000`;
- unsupported quoted system-variable scope: existing MyLite unsupported
  diagnostic;
- unsupported `SET GLOBAL time_zone`: existing MyLite unsupported-global
  diagnostic;
- read-only `system_time_zone`: existing read-only system-variable diagnostic;
- session/local `system_time_zone`: existing global-only system-variable
  diagnostic;
- unknown or unsupported time-zone string/name: `1298 / HY000`;
- numeric time-zone assignment: `1232 / 42000`;
- `NULL` time-zone assignment: `1231 / 42000`;
- allocation failure: existing MyLite `MYLITE_NOMEM` path with diagnostics;
- public API misuse: unchanged.

Supported assignments produce no warnings.

## Tests

Add MySQL-runtime expectation coverage for:

- MySQL 8.4.9 version check;
- default `@@GLOBAL.time_zone`, `@@SESSION.time_zone`,
  `@@system_time_zone`, `SHOW VARIABLES`, and `SHOW GLOBAL VARIABLES`;
- each supported assignment spelling listed in this spec;
- offset canonicalization, zero-offset normalization, range endpoints, `UTC`,
  and `SYSTEM`;
- invalid offsets, unsupported names, numeric values, `NULL`, and unsupported
  global mutation behavior;
- current-time function shifts under deterministic `SET timestamp`;
- MySQL's wider TIMESTAMP row conversion behavior as explicitly deferred.

Add MyLite runtime tests for:

- scalar readback, `SHOW VARIABLES`, and `SHOW GLOBAL VARIABLES`;
- supported session/local assignment forms and readback normalization;
- invalid value diagnostics;
- read-only/global-only `system_time_zone` behavior;
- deterministic current-date/time/timestamp function shifts with
  `SET timestamp`;
- current temporal defaults/assignments using the session time-zone offset in
  the current materialized-text baseline;
- independent handles and close/reopen reset;
- file preamble preservation after `SET time_zone` and current temporal DML;
- unchanged public API surface and existing misuse behavior.

Run:

```sh
packages/libmylite/tests/mysql_baseline_time_zone_system_variable_expectations.sh
cmake --build --preset dev
ctest --preset dev -R 'libmylite\.(runtime\.(time_zone_system_variable|current_date_time_functions|current_timestamp_defaults|timestamp_type|show_variables|open_memory)|parser)$' --output-on-failure
cmake --workflow --preset check
```

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/runtime-system-variables.md`
- `docs/compatibility/runtime-session-sql-modes.md`

Do not claim full time-zone tables, global mutation, named-zone support beyond
`UTC`, or TIMESTAMP storage/retrieval conversion.
