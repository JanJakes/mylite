# String `HEX()` and `UNHEX()` functions

## Scope

This feature implements MySQL-compatible `HEX(expr)` and `UNHEX(expr)` as
scalar string functions.

The functions are available anywhere MyLite currently evaluates supported
scalar built-ins:

- no-table `SELECT`
- table-backed `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments, predicates, and order keys
- supported single-table `DELETE` predicates and order keys

Out of scope:

- `max_allowed_packet` result-size enforcement
- exact native error-code exposure for unsupported arity paths
- generated-column metadata's MySQL table-character-set exception for `HEX()`
- full binary-string behavior for expression surfaces that still model strings
  as ordinary text values
- exact DECIMAL-versus-DOUBLE rounding distinctions for numeric values after
  they have passed through MyLite expression paths that do not preserve exact
  numeric category metadata

## Sources

- MySQL 8.4 Reference Manual, String Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions.html
- MySQL 8.4 Reference Manual, Character Set and Collation of Function Results:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions-charset.html
- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- Existing MyLite scalar-function design:
  `docs/specs/scalar-built-in-functions/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --show-warnings --force --binary-as-hex=0`
- `docker exec -i mylite-mysql-849 mysql -uroot --column-type-info -vvv --force --binary-as-hex=0`

Runtime probes used `SET NAMES utf8mb4` unless the metadata probe explicitly
switched to `latin1`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## Syntax and arity

`HEX()` and `UNHEX()` are unary scalar functions. Zero-argument and two-argument
calls raise native MySQL error 1582. MyLite may continue to reject those paths
through its existing unsupported-function/arity diagnostic until exact native
function diagnostics are implemented globally.

Generic function-call grammar is sufficient. No dedicated token or AST node is
required.

The intended MyLite Lemon-style grammar remains:

```lemon
scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.
function_name ::= identifier.
```

## Semantics

### `HEX(expr)`

If `expr` is `NULL`, `HEX()` returns `NULL`.

For string input, `HEX()` returns uppercase hexadecimal digits for the input
bytes. Multibyte UTF-8 characters therefore produce two hex digits for each
encoded byte. Empty string input returns the empty string.

For numeric input, `HEX()` converts the value to a signed longlong-compatible
integer value, interprets the resulting 64-bit bit pattern as unsigned, and
returns the uppercase hexadecimal representation without leading zero padding.
Exact decimal half values round away from zero. Approximate half values round
to the nearest even integer in the verified MySQL runtime.

Observed examples:

| Expression | Result | Warnings |
| --- | --- | --- |
| `HEX('Az')` | `417A` | none |
| `HEX('')` | empty string | none |
| `HEX(NULL)` | `NULL` | none |
| `HEX('猫')` | `E78CAB` | none |
| `HEX(255)` | `FF` | none |
| `HEX(0)` | `0` | none |
| `HEX(-1)` | `FFFFFFFFFFFFFFFF` | none |
| `HEX(12.5)` | `D` | none |
| `HEX(12.5E0)` | `C` | none |
| `HEX('255')` | `323535` | none |
| `HEX(CAST('255' AS UNSIGNED))` | `FF` | none |

### `UNHEX(expr)`

If `expr` is `NULL`, `UNHEX()` returns `NULL`.

`UNHEX()` first converts non-string arguments to their string form, then
interprets the resulting text as hexadecimal digits. Uppercase and lowercase
digits are accepted. Odd-length strings are interpreted as if a leading `0`
nibble preceded the first digit. Empty string input returns the empty binary
string.

The result is a binary string. Invalid characters produce `NULL` and warning
1411. This warning remains a warning in ordinary `SELECT`, including
table-backed predicates, but can become a DML execution error in strict
`UPDATE` and `DELETE` paths when evaluated for a row.

Observed examples:

| Expression | Result | Warnings |
| --- | --- | --- |
| `HEX(UNHEX('417a'))` | `417A` | none |
| `HEX(UNHEX('F'))` | `0F` | none |
| `HEX(UNHEX('abc'))` | `0ABC` | none |
| `HEX(UNHEX(''))` | empty string | none |
| `HEX(UNHEX(NULL))` | `NULL` | none |
| `HEX(UNHEX('GG'))` | `NULL` | one 1411 warning |
| `HEX(UNHEX('41 42'))` | `NULL` | one 1411 warning |
| `HEX(UNHEX(1267))` | `1267` | none |
| `HEX(UNHEX(18446744073709551615))` | `18446744073709551615` | none |
| `HEX(UNHEX(12E0))` | `12` | none |
| `HEX(UNHEX(12.67))` | `NULL` | one 1411 warning |
| `HEX(UNHEX(1.0))` | `NULL` | one 1411 warning |
| `HEX(UNHEX(-1))` | `NULL` | one 1411 warning |
| `HEX(UNHEX(-1E0))` | `NULL` | one 1411 warning |
| `LENGTH(UNHEX('4100FF'))` | `3` | none |
| `UNHEX('E78CAB')` | `猫` bytes | none |

Hex and bit literals are supported by the shared scalar expression evaluator,
so `HEX(X'4100')`, `UNHEX(X'3431')`, and bit-literal equivalents operate on
their decoded binary-string bytes.

## Result metadata

`HEX()` reports `VAR_STRING`, MySQL's not-fixed decimals marker `31`, and the
connection result character set/collation. Its declared length is two hex
characters per possible input byte, measured in the connection result character
set. Numeric input reports up to 16 hex characters.

`UNHEX()` reports `VAR_STRING`, MySQL's not-fixed decimals marker `31`, binary
charset/collation, and `BINARY` flag. Its declared length is half of the input
display length, rounded up. It is nullable because invalid input can produce
`NULL` even when the argument expression is not statically nullable.

Verified metadata:

| Expression | Connection | Type | Collation id | Length | Decimals | Flags |
| --- | --- | --- | ---: | ---: | ---: | --- |
| `HEX('Az')` | `utf8mb4` | `VAR_STRING` | `255` | `64` | `31` | none |
| `HEX('')` | `utf8mb4` | `VAR_STRING` | `255` | `0` | `31` | none |
| `HEX(255)` | `utf8mb4` | `VAR_STRING` | `255` | `64` | `31` | none |
| `HEX(NULL)` | `utf8mb4` | `VAR_STRING` | `255` | `0` | `31` | none |
| `UNHEX('417a')` | `utf8mb4` | `VAR_STRING` | `63` | `8` | `31` | `BINARY` |
| `UNHEX('')` | `utf8mb4` | `VAR_STRING` | `63` | `0` | `31` | `BINARY` |
| `UNHEX('GG')` | `utf8mb4` | `VAR_STRING` | `63` | `4` | `31` | `BINARY` |
| `UNHEX(NULL)` | `utf8mb4` | `VAR_STRING` | `63` | `0` | `31` | `BINARY` |
| `UNHEX(1267)` | `utf8mb4` | `VAR_STRING` | `63` | `3` | `31` | `BINARY` |
| `HEX('Az')` | `latin1` | `VAR_STRING` | `8` | `4` | `31` | none |
| `HEX(255)` | `latin1` | `VAR_STRING` | `8` | `16` | `31` | none |
| `UNHEX('417a')` | `latin1` | `VAR_STRING` | `63` | `2` | `31` | `BINARY` |
| `UNHEX('GG')` | `latin1` | `VAR_STRING` | `63` | `1` | `31` | `BINARY` |

MyLite's current metadata API also exposes expression nullability through its
existing `NOT_NULL` flag model. That model can be stricter than the MySQL CLI
flags for deterministic non-`NULL` scalar expressions.

## Runtime design

Implementation extends the scalar-function registry in
`mylite_expression.c`:

- add function ids for `HEX` and `UNHEX`
- validate arity as exactly one argument
- evaluate the argument once, left to right
- return `NULL` for `NULL` input
- encode string values byte-for-byte for `HEX()`
- convert numeric values to a 64-bit integer bit pattern for `HEX()`
- decode uppercase and lowercase hex pairs for `UNHEX()`
- handle odd `UNHEX()` input by decoding the first digit as a low nibble
- append warning 1411 and return `NULL` for invalid `UNHEX()` input
- preserve internal byte lengths for binary `UNHEX()` results so nested
  `HEX(UNHEX(...))` and `LENGTH(UNHEX(...))` handle `0x00` bytes

Metadata inference in `mylite.c` adds dedicated `HEX()` and `UNHEX()`
descriptors. `HEX()` cannot reuse ordinary string-function metadata because its
result charset follows the connection while its declared length derives from
input byte width. `UNHEX()` cannot reuse ordinary string-function metadata
because its result is binary and nullable for invalid input.

No storage or file-format changes are required.

## Tests

Add C tests for:

- parser acceptance of generic `HEX()` and `UNHEX()` calls
- unsupported zero-argument and two-argument arity
- no-table scalar results for ASCII, empty string, `NULL`, UTF-8 bytes,
  integer, unsigned boundary, negative, exact decimal, approximate real, string
  digit input, `CAST(... AS UNSIGNED)`, upper/lower valid `UNHEX()`, odd-length
  input, empty string, invalid characters, numeric stringification, UTF-8
  bytes, embedded-NUL byte length, unsigned digit strings, approximate numeric
  stringification, and nested `HEX(UNHEX('4100'))`
- warning code 1411 for invalid `UNHEX()` input
- metadata under `utf8mb4` and `latin1`, including text, numeric, `NULL`,
  invalid `UNHEX()`, and binary result descriptors
- table-backed `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignment, predicate, and order key
- strict DML warning promotion for invalid `UNHEX()` in `UPDATE` predicates
  and assignments
- supported single-table `DELETE` predicate and order key

## Compatibility status

After this feature, `HEX()` and `UNHEX()` are partially supported for the
existing scalar expression call sites. The status remains partial because exact
native arity diagnostics, `max_allowed_packet`, generated-column charset
exceptions, fully length-aware binary string semantics across all expression
functions, and exact-versus-approximate numeric rounding outside preserved
literal cases remain deferred.
