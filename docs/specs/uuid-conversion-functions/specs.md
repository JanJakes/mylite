# UUID validation and conversion functions

## Scope

This feature implements the deterministic UUID scalar functions:

- `IS_UUID(string_uuid)`
- `UUID_TO_BIN(string_uuid)`
- `UUID_TO_BIN(string_uuid, swap_flag)`
- `BIN_TO_UUID(binary_uuid)`
- `BIN_TO_UUID(binary_uuid, swap_flag)`

`UUID()` and `UUID_SHORT()` generation are specified separately in
`docs/specs/uuid-function/specs.md` and
`docs/specs/uuid-short-function/specs.md`.

The functions are available wherever MyLite currently evaluates supported
scalar built-ins: no-table `SELECT`, one-table `SELECT` projection, `WHERE`,
`ORDER BY`, and the supported single-table `UPDATE` and `DELETE` expression
paths. Insert expression paths remain deferred with the broader scalar-function
gap.

## Sources

- MySQL 8.4 Reference Manual, Built-In Function and Operator Reference:
  https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html
- MySQL 8.4 Reference Manual, Miscellaneous Functions:
  https://dev.mysql.com/doc/refman/8.4/en/miscellaneous-functions.html
- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- Existing MyLite scalar-function design:
  `docs/specs/scalar-built-in-functions/specs.md`
- Existing related MyLite function slices:
  - `docs/specs/string-hex-unhex-functions/specs.md`
  - `docs/specs/base64-string-functions/specs.md`
  - `docs/specs/inet-ipv4-functions/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings --force --binary-as-hex=0`
- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv --binary-as-hex=0`

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## Syntax

The functions use MyLite's existing ordinary scalar function-call grammar.
Function names are identifiers and are matched case-insensitively by the scalar
function registry.

```lemon
scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.
function_name ::= identifier.
```

The supported arities are:

| Function | Arity |
| --- | --- |
| `IS_UUID` | 1 |
| `UUID_TO_BIN` | 1 or 2 |
| `BIN_TO_UUID` | 1 or 2 |

Wrong arity should be rejected through the existing scalar-function binding
path. Exact native MySQL error 1582 exposure remains a known gap in the current
scalar evaluator.

## UUID text grammar

MySQL accepts exactly these text UUID input layouts for `IS_UUID()` and
`UUID_TO_BIN()`:

- canonical dashed form: `8-4-4-4-12` hexadecimal digits
- hyphenless form: 32 hexadecimal digits
- braced canonical form: `{8-4-4-4-12}` hexadecimal digits

Hexadecimal letters are accepted in either case. The functions do not validate
RFC variant or version bits; they only validate parseable text shape and
hexadecimal characters. Whitespace, partial braces, braces around hyphenless
text, misplaced dashes, extra text, and non-hex characters are invalid.

## Semantics

### `IS_UUID()`

`IS_UUID(argument)` returns:

- `1` when the argument string matches one of the accepted UUID layouts
- `0` when the argument is non-`NULL` and not a valid UUID string
- `NULL` when the argument is SQL `NULL`

Non-string arguments are converted to string first using the current scalar
conversion helpers; ordinary numeric inputs such as `123` and `12.5` therefore
return `0`.

### `UUID_TO_BIN()`

`UUID_TO_BIN(string_uuid[, swap_flag])` validates the UUID string, decodes its
32 hexadecimal digits, and returns a 16-byte binary string.

`NULL` in the first argument returns `NULL`. If the first argument is non-NULL
and not a valid UUID string, MySQL raises error 1411 with an "Incorrect string
value" diagnostic for function `uuid_to_bin`.

The optional `swap_flag` is evaluated only when the first argument is non-NULL.
It is converted using MySQL numeric truth rules. `0`, `NULL`, and strings that
convert to `0` keep the UUID bytes in text order. Any nonzero value, including
negative and greater-than-one numbers, swaps the time-low and time-high UUID
parts. Text flags that truncate during numeric conversion produce warning 1292
but still use the converted value.

The swap reorders bytes from canonical order:

```text
time_low[4] time_mid[2] time_high[2] rest[8]
```

to:

```text
time_high[2] time_mid[2] time_low[4] rest[8]
```

### `BIN_TO_UUID()`

`BIN_TO_UUID(binary_uuid[, swap_flag])` converts a 16-byte binary string to
lowercase canonical dashed UUID text.

`NULL` in the first argument returns `NULL`. If the first argument is non-NULL
and does not contain exactly 16 bytes, MySQL raises error 1411 with an
"Incorrect string value" diagnostic for function `bin_to_uuid`.

The optional `swap_flag` follows the same evaluation rules as `UUID_TO_BIN()`.
When true, bytes are interpreted as a swapped UUID and are moved back from:

```text
time_high[2] time_mid[2] time_low[4] rest[8]
```

to canonical UUID order before formatting.

## Verified MySQL 8.4.9 behavior

Runtime probes used `SET NAMES utf8mb4` except where metadata under `latin1`
was tested separately.

| Expression | Result | Warnings/errors |
| --- | --- | --- |
| `IS_UUID('6ccd780c-baba-1026-9564-5b8c656024db')` | `1` | none |
| `IS_UUID('6CCD780C-BABA-1026-9564-5B8C656024DB')` | `1` | none |
| `IS_UUID('6ccd780cbaba102695645b8c656024db')` | `1` | none |
| `IS_UUID('{6ccd780c-baba-1026-9564-5b8c656024db}')` | `1` | none |
| `IS_UUID('{6ccd780cbaba102695645b8c656024db}')` | `0` | none |
| `IS_UUID(NULL)` | `NULL` | none |
| `IS_UUID('6ccd780c-baba-1026-9564-5b8c6560')` | `0` | none |
| `IS_UUID('6ccd780c-baba-1026-9564-5b8c656024dg')` | `0` | none |
| `IS_UUID(' 6ccd780c-baba-1026-9564-5b8c656024db')` | `0` | none |
| `IS_UUID(123)` | `0` | none |
| `HEX(UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db'))` | `6CCD780CBABA102695645B8C656024DB` | none |
| `HEX(UUID_TO_BIN('6ccd780cbaba102695645b8c656024db'))` | `6CCD780CBABA102695645B8C656024DB` | none |
| `HEX(UUID_TO_BIN('{6ccd780c-baba-1026-9564-5b8c656024db}'))` | `6CCD780CBABA102695645B8C656024DB` | none |
| `HEX(UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db', 0))` | `6CCD780CBABA102695645B8C656024DB` | none |
| `HEX(UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db', 1))` | `1026BABA6CCD780C95645B8C656024DB` | none |
| `HEX(UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db', 2))` | `1026BABA6CCD780C95645B8C656024DB` | none |
| `HEX(UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db', -1))` | `1026BABA6CCD780C95645B8C656024DB` | none |
| `HEX(UUID_TO_BIN(NULL))` | `NULL` | none |
| `LENGTH(UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db'))` | `16` | none |
| `BIN_TO_UUID(UNHEX('6CCD780CBABA102695645B8C656024DB'))` | `6ccd780c-baba-1026-9564-5b8c656024db` | none |
| `BIN_TO_UUID(UNHEX('1026BABA6CCD780C95645B8C656024DB'), 1)` | `6ccd780c-baba-1026-9564-5b8c656024db` | none |
| `BIN_TO_UUID(UNHEX('6CCD780CBABA102695645B8C656024DB'), 1)` | `baba1026-780c-6ccd-9564-5b8c656024db` | none |
| `BIN_TO_UUID(UNHEX('1026BABA6CCD780C95645B8C656024DB'), 0)` | `1026baba-6ccd-780c-9564-5b8c656024db` | none |
| `BIN_TO_UUID(NULL)` | `NULL` | none |
| `UUID_TO_BIN('not-a-uuid')` | error | 1411 / `HY000` |
| `BIN_TO_UUID(UNHEX('00'))` | error | 1411 / `HY000` |
| `BIN_TO_UUID(UNHEX('0000000000000000000000000000000000'))` | error | 1411 / `HY000` |
| `UUID_TO_BIN(..., 'abc')` | unswapped bytes | warning 1292 |
| `BIN_TO_UUID(..., 'abc')` | unswapped text | warning 1292 |
| `UUID_TO_BIN(NULL, MOD(7,0))` | `NULL` | none |
| `UUID_TO_BIN(valid_uuid, MOD(7,0))` | unswapped bytes | warning 1365 |
| `BIN_TO_UUID(NULL, MOD(7,0))` | `NULL` | none |
| `BIN_TO_UUID(valid_bin, MOD(7,0))` | unswapped text | warning 1365 |

`BIN_TO_UUID()` result metadata under `SET NAMES utf8mb4` is `VAR_STRING`,
connection collation `utf8mb4_0900_ai_ci` id 255, declared length 144,
decimals 31, nullable. Under `SET NAMES latin1` it is `VAR_STRING`, collation
`latin1_swedish_ci` id 8, declared length 36, decimals 31, nullable.

`UUID_TO_BIN()` result metadata is `VAR_STRING`, binary collation id 63,
declared length 16, decimals 31, `BINARY`, nullable.

`IS_UUID()` result metadata is `LONGLONG`, binary collation id 63, declared
length 1, decimals 0, `BINARY NUM`, nullable.

## Implementation plan

- Extend the scalar function registry with three new function ids and arity
  rules.
- Implement direct UUID validation and byte conversion in
  `mylite_expression.c` without new dependencies.
- Reuse existing scalar conversion helpers for string and swap-flag
  conversions so truncation and division warnings follow existing expression
  behavior.
- Return binary strings as length-aware text values with binary result metadata.
- Add metadata inference in `mylite.c` for `IS_UUID()`, `UUID_TO_BIN()`, and
  `BIN_TO_UUID()` under current connection charsets.
- Preserve the current scalar-function diagnostic limitation for exact native
  arity errors.

## Tests

Runtime tests should cover:

- valid canonical, uppercase, hyphenless, and braced canonical UUID text
- invalid brace, whitespace, dash, length, and hex forms
- `NULL` handling for every function
- numeric conversion for `IS_UUID()`
- byte conversion through `HEX(UUID_TO_BIN(...))`
- formatting through `BIN_TO_UUID(UNHEX(...))`
- optional `swap_flag` values `0`, `1`, `2`, `-1`, `NULL`, numeric-looking
  text, and truncating text
- binary length rejection for `BIN_TO_UUID()`
- result metadata under `utf8mb4` and `latin1`
- unsupported arity through existing parser/runtime paths
- no-table `SELECT`, table projection, `WHERE`, `ORDER BY`, `UPDATE`, and
  `DELETE`

Parser tests should cover ordinary scalar function calls and case-insensitive
names for all three functions.

## Compatibility notes

This slice does not change the current unsupported arity diagnostic path for
scalar functions. Insert expression paths remain deferred until the broader
scalar function execution surface is expanded. `UUID()` and `UUID_SHORT()`
generation are specified separately in `docs/specs/uuid-function/specs.md` and
`docs/specs/uuid-short-function/specs.md`.
