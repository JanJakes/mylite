# IPv4 network address functions

## Scope

This feature implements the MySQL scalar IPv4 utility functions:

- `INET_ATON(str)`
- `INET_NTOA(expr)`

The functions are available anywhere MyLite currently evaluates supported
scalar built-ins: no-table `SELECT`, one-table `SELECT` projection, `WHERE`,
`ORDER BY`, and the existing single-table `UPDATE` and `DELETE` expression
paths.

IPv6 functions (`INET6_ATON`, `INET6_NTOA`, `IS_IPV4`, `IS_IPV4_COMPAT`,
`IS_IPV4_MAPPED`, `IS_IPV6`) are out of scope.

## Sources

- MySQL 8.4 Reference Manual, Built-In Function and Operator Reference:
  https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html
- MySQL 8.4 Reference Manual, Miscellaneous Functions:
  https://dev.mysql.com/doc/refman/8.4/en/miscellaneous-functions.html
- Observed MySQL 8.4.9 runtime behavior in Docker container
  `mylite-mysql-849`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar, documentation prose,
or implementation sources.

## MySQL 8.4.9 Behavior

`INET_ATON(str)` interprets a valid IPv4 address string as a 32-bit unsigned
integer in network byte order and returns `NULL` for `NULL` input. It accepts
one-, two-, three-, and four-part forms:

| Expression | Result |
| --- | --- |
| `INET_ATON('127.0.0.1')` | `2130706433` |
| `INET_ATON('10.0.5.9')` | `167773449` |
| `INET_ATON('10.0.5')` | `167772165` |
| `INET_ATON('10.0')` | `167772160` |
| `INET_ATON('10')` | `10` |
| `INET_ATON(10)` | `10` |
| `INET_ATON(1.2)` | `16777218` |
| `INET_ATON('127.1')` | `2130706433` |
| `INET_ATON('0.1')` | `1` |
| `INET_ATON('1..2')` | `16777218` |
| `INET_ATON('1...2')` | `16777218` |
| `INET_ATON('.1')` | `1` |
| `INET_ATON('1.2..3')` | `16908291` |
| `INET_ATON('255.255.255.255')` | `4294967295` |
| `INET_ATON(NULL)` | `NULL` |

Observed MySQL 8.4.9 accepts short forms but still requires every part to be in
the one-byte octet range `[0, 255]`. Missing middle or leading components in
dotted forms count as zero when the total dotted component count is four or
fewer and the final component is present. For example, `'10.5'` and `'10.0.5'`
both represent `10.0.0.5`, while `'1...2'` represents `1.0.0.2`. Numeric
arguments are first converted to their string form for this parser; invalid
numeric arguments use a single-quoted warning value, while invalid string
arguments display the nested quoted string value.

Invalid `INET_ATON` strings return `NULL` and append warning 1411:

| Expression | Warning message shape |
| --- | --- |
| `INET_ATON('')` | `Incorrect string value: '''' for function inet_aton` |
| `INET_ATON('127.0.0.1x')` | includes the invalid argument text |
| `INET_ATON('127.0.0.256')` | includes the invalid argument text |
| `INET_ATON('1.2.3.4.5')` | includes the invalid argument text |
| `INET_ATON(' 127.0.0.1')` | leading space is invalid |
| `INET_ATON('127.0.0.1 ')` | trailing space is invalid |
| `INET_ATON('-1')` | signs are invalid |
| `INET_ATON('256')` | one-part values above 255 are invalid |
| `INET_ATON(256)` | numeric one-part values above 255 are invalid |
| `INET_ATON('2130706433')` | large one-part strings are invalid |
| `INET_ATON(2130706433)` | large numeric one-part values are invalid |
| `INET_ATON('1.')` | trailing empty component is invalid |
| `INET_ATON('1....2')` | more than four dotted components is invalid |
| `INET_ATON('....1')` | more than four dotted components is invalid |
| `INET_ATON('1.2...3')` | more than four dotted components is invalid |
| `INET_ATON('...')` | all-empty dotted input is invalid |

`INET_NTOA(expr)` converts a numeric IPv4 value in `[0, 4294967295]` to a
dotted-quad string in the connection character set. `NULL` input returns `NULL`.

| Expression | Result / warnings |
| --- | --- |
| `INET_NTOA(2130706433)` | `127.0.0.1` |
| `INET_NTOA(167773449)` | `10.0.5.9` |
| `INET_NTOA(4294967295)` | `255.255.255.255` |
| `INET_NTOA(NULL)` | `NULL` |
| `INET_NTOA('2130706433')` | `127.0.0.1` |
| `INET_NTOA('1.9')` | `0.0.0.1`, warning 1292 |
| `INET_NTOA(1.9)` | `0.0.0.2` |
| `INET_NTOA('x')` | `0.0.0.0`, warning 1292 |
| `INET_NTOA(' 1 ')` | `0.0.0.1` |
| `INET_NTOA(4294967296)` | `NULL`, warning 1411 |
| `INET_NTOA(-1)` | `NULL`, warning 1411 |
| `INET_NTOA('4294967296x')` | `NULL`, warnings 1292 and 1411 |

String-to-integer truncation warnings use code 1292 and message shape
`Truncated incorrect INTEGER value: '<text>'`. Out-of-range numeric values use
code 1411 and message shape `Incorrect integer value: '<value>' for function
inet_ntoa`; negative numeric literals are displayed by MySQL as `-(n)` in that
message.

## Metadata

Observed metadata:

| Connection charset | Expression | Type | Length | Decimals | Charset | Flags | Nullable |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `utf8mb4` | `INET_ATON('127.0.0.1')` | `LONGLONG` | `21` | `0` | `binary` / `63` | `UNSIGNED BINARY NUM` | yes |
| `latin1` | `INET_ATON('127.0.0.1')` | `LONGLONG` | `21` | `0` | `binary` / `63` | `UNSIGNED BINARY NUM` | yes |
| `utf8mb4` | `INET_NTOA(2130706433)` | `VAR_STRING` | `124` | `31` | connection collation | none | yes |
| `latin1` | `INET_NTOA(2130706433)` | `VAR_STRING` | `31` | `31` | connection collation | none | yes |

`INET_NTOA` length is 31 characters multiplied by the connection character-set
maximum byte length. `INET_ATON` is nullable even for non-null literal input.

## Grammar

The ordinary scalar function-call grammar is sufficient:

```lemon
function_call(A) ::= function_name(N) LP function_argument_list(ARGS) RP.
function_name(A) ::= INET_ATON.
function_name(A) ::= INET_NTOA.
```

Both functions require exactly one argument. Unsupported arity is rejected by
the existing scalar-function support check.

## Implementation Notes

- Add `INET_ATON` and `INET_NTOA` to the scalar-function registry.
- Parse IPv4 text directly in MyLite code. Do not use `inet_aton(3)` because it
  has platform-specific short-form behavior and whitespace tolerance.
- `INET_ATON` accepts only ASCII decimal digits and dots. Leading and middle
  empty dotted components are zero when the full input has one to four
  components and ends in a digit. Signs, whitespace, too many components,
  trailing empty components, all-empty inputs, and parts outside `[0, 255]` are
  invalid.
- `INET_NTOA` reuses MyLite's integer conversion behavior for strings so code
  1292 warnings match the current scalar conversion paths, then applies a
  function-specific unsigned 32-bit range check with code 1411.
- DML paths use the existing warning promotion rules. Under the current default
  strict-mode policy, warnings in `UPDATE` assignments/predicates and `DELETE`
  predicates become execution errors and must leave rows unchanged.

## Test Plan

- Parser acceptance for both functions and case-insensitive function names.
- Unsupported arity for zero and two arguments.
- No-table scalar results for normal addresses, short forms, maximum value,
  `NULL`, invalid strings, numeric strings, fractional numbers, and bounds.
- Warning code/message checks for invalid `INET_ATON`, invalid `INET_NTOA`
  bounds, and string truncation in `INET_NTOA`.
- Metadata checks under `SET NAMES utf8mb4` and `SET NAMES latin1`.
- One-table projection, `WHERE`, and `ORDER BY`.
- `UPDATE` assignment/predicate and `DELETE` predicate contexts.

## Compatibility Status

The first implementation slice supports MySQL 8.4.9-compatible behavior for the
covered scalar expression contexts. Deferred behavior is limited to exact native
error-code reporting for unsupported arity and any broader expression contexts
that do not yet evaluate scalar functions.
