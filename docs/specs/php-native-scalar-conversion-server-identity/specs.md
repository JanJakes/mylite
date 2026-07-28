# PHP Native Scalar Conversion And Server Identity

## Status

This feature specifies PHP native-protocol scalar conversion and PDO version
identity across PDO MyLite and the mysqli replacement. It closes review
findings API-03 and API-04 without changing SQL type semantics or mysqli's
direct text-protocol result policy.

## Sources

- MyLite architecture and engineering policy:
  `README.md`,
  `docs/architecture/engineering-standards.md`
- Existing result metadata:
  `docs/compatibility/error-warning-result-semantics.md`,
  `docs/specs/baseline-result-column-metadata/specs.md`
- Existing SQL-visible server identity:
  `docs/specs/baseline-mysql-server-version-identity/specs.md`
- PHP PDO MySQL attributes:
  https://www.php.net/manual/en/ref.pdo-mysql.php,
  https://www.php.net/manual/en/pdo.constants.php
- PHP mysqli prepared statements:
  https://www.php.net/manual/en/mysqli.quickstart.prepared-statements.php
- PHP mysqli integer and float native option:
  https://www.php.net/manual/en/mysqli.options.php

This specification is independently authored from project documentation,
official PHP API documentation, observed MySQL 8.4.9 behavior, and existing
MyLite source code. It does not copy MySQL, PHP, mysqlnd, MariaDB, Percona, or
other implementation internals.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_php_native_scalar_conversion_server_identity_expectations.sh`
records the runtime probes for this feature against MySQL 8.4.9 and PHP's
mysqlnd-backed PDO MySQL and mysqli extensions.

For native prepared results with `PDO::ATTR_STRINGIFY_FETCHES` disabled and for
mysqli prepared `get_result()` and bound-result fetches:

- signed integral values from `PHP_INT_MIN` through `PHP_INT_MAX` are PHP
  integers;
- unsigned integral and `BIT` values are PHP integers when representable by a
  PHP integer;
- unsigned integral and `BIT` values above `PHP_INT_MAX` are decimal strings;
- `FLOAT` and `DOUBLE` values are PHP doubles;
- `DECIMAL`, text, and binary values are PHP strings;
- SQL `NULL` is PHP `null`;
- binary strings retain their exact length and embedded NUL bytes.

With `PDO::ATTR_STRINGIFY_FETCHES` enabled, integral, `BIT`, `FLOAT`, and
`DOUBLE` values are strings. `DECIMAL`, text, and binary values remain strings,
and SQL `NULL` remains PHP `null`.

Default direct mysqli query results use text-protocol scalar policy and expose
non-NULL values as strings. This feature does not apply native prepared
conversion to those results.

`PDO::ATTR_SERVER_VERSION` returns `8.4.9` and equals `SELECT VERSION()`.
`PDO::ATTR_CLIENT_VERSION` identifies the client library independently and is
not the server identity.

## Scope

The implementation must:

- convert PDO result cells from native metadata and length-aware value bytes;
- convert prepared mysqli `get_result()`, `execute_query()`, and bound-result
  cells by the same metadata-driven policy;
- support signed and unsigned TINY, SHORT, LONG, INT24, and LONGLONG metadata,
  `BIT`, `FLOAT`, and `DOUBLE`;
- preserve DECIMAL/NEWDECIMAL, overflowing integers, text, binary, temporal,
  JSON, geometry, and unknown types as length-aware strings;
- preserve SQL `NULL` as PHP `null`;
- honor PDO's current `ATTR_STRINGIFY_FETCHES` value for every fetch, including
  changes made after statement preparation;
- leave direct mysqli result conversion unchanged;
- expose one native server-version accessor backed by the same constant as
  SQL-visible `VERSION()` and `@@version`;
- report `mylite_version()` as PDO's client version and the compatibility
  server version as PDO's server version.

## Non-Goals

This feature does not add:

- unsigned table storage above MyLite's current signed 64-bit physical
  envelope;
- new SQL types, casts, expression metadata, or numeric formatting rules;
- native numeric conversion in the core `mylite` PHP extension;
- `MYSQLI_OPT_INT_AND_FLOAT_NATIVE` conversion for direct text-protocol
  results;
- a configurable server identity or wire-protocol handshake;
- 32-bit PHP qualification in this release. The conversion contract is still
  defined as values representable by the active platform's `zend_long`.

## Public Native ABI

The installed native header adds:

```c
const char *mylite_server_version(void);
```

`mylite_server_version()` returns a process-lifetime borrowed pointer to the
MySQL compatibility server identity used by SQL-visible `VERSION()` and
`@@version`. `mylite_version()` remains the MyLite package/library version.
The new function is additive at the existing ABI generation.

## Conversion Policy

Conversion uses result-column type and flags, not inspection of arbitrary text
columns.

| Native metadata | PHP value with native conversion |
| --- | --- |
| TINY, SHORT, LONG, INT24, LONGLONG, YEAR | `int` when the decimal value is representable by `zend_long`; otherwise the original decimal `string` |
| BIT | `int` when its unsigned big-endian value is at most `ZEND_LONG_MAX`; otherwise an unsigned decimal `string` |
| FLOAT, DOUBLE | `float` parsed from MyLite's canonical numeric bytes |
| DECIMAL, NEWDECIMAL | Original `string`, including scale and trailing zeroes |
| NULL value | `null`, regardless of column metadata |
| All other types | Original length-aware `string` |

The unsigned metadata flag does not force a PHP integer when the value exceeds
`ZEND_LONG_MAX`. No conversion may use a floating-point intermediate for an
exact integer.

With PDO stringify enabled, successfully converted integral, `BIT`, and
approximate numeric values are converted to their PHP string representation.
Exact numeric strings that cannot be represented as PHP integers remain
unchanged.

## mysqli Protocol Boundary

Prepared statement `bind_result()`, `get_result()`, and `execute_query()` use
native conversion. Buffered and unbuffered direct `query()`,
`real_query()`/`store_result()`, and `real_query()`/`use_result()` retain their
existing string/NULL behavior.

The conversion happens when a cell is copied into a zval. Buffered prepared
results therefore own typed zvals, while unbuffered bound-result fetches
replace bound zvals with the current row's typed values.

## PDO Identity

`PDO::ATTR_CLIENT_VERSION` returns `mylite_version()`.
`PDO::ATTR_SERVER_VERSION` returns `mylite_server_version()`. The server value
must exactly equal the first column of:

```sql
SELECT VERSION();
```

Application harnesses must not override either attribute or the SQL-visible
row to make this equality pass.

## Error And Lifetime Semantics

- Conversion does not alter connection or statement diagnostics.
- SQL `NULL` is checked before metadata conversion.
- All string construction is length-aware and preserves embedded NUL bytes.
- A noncanonical integral value that cannot be parsed safely remains a string.
- Native version pointers are immutable and valid for the process lifetime.
- PHP zvals own their converted strings and remain valid under the existing
  result and statement lifetime rules.

## Test Matrix

| Layer | Required coverage |
| --- | --- |
| MySQL 8.4.9 fixture | signed minimum; representable unsigned maximum; unsigned overflow expression; BIT; FLOAT/DOUBLE; DECIMAL; binary/text/NULL; PDO stringify; direct and prepared mysqli; PDO identity equality |
| PDO MyLite | native and stringified associative/column fetches; runtime stringify toggle; binary NUL; server/client distinction; `VERSION()` equality |
| mysqli MyLite | direct strings unchanged; prepared `get_result()`; `execute_query()`; `bind_result()`; BIT and unsigned overflow; binary NUL |
| Native C API | package/server identity distinction and SQL-visible equality |
| Qualification | Release/Debug tests, ASan/UBSan extension tests, ABI checks, formatters, static analysis, artifact-size gates |
