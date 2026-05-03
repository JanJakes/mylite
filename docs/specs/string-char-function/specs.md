# String `CHAR()` function

## Sources and verification

This specification is independently authored from the MySQL 8.4 reference
manual and MySQL 8.4.9 runtime probes. The relevant manual entry defines
`CHAR(N,... [USING charset_name])`, integer interpretation of each argument,
skipped `NULL` arguments, binary default results, and the optional `USING`
charset clause:

- <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/string-functions-charset.html>
- <https://dev.mysql.com/doc/refman/8.4/en/charset-syntax.html>

Runtime expectations were verified against the local MySQL 8.4.9 Docker
container `mylite-mysql-849` with default strict SQL mode unless otherwise
noted.

## Scope

This feature implements MySQL-compatible `CHAR()` as a scalar string function
for the currently supported MyLite expression contexts:

- no-table `SELECT`
- table-backed `SELECT` projection, `WHERE`, and `ORDER BY`
- supported `UPDATE` assignment, predicate, and order-key expressions
- supported `DELETE` predicate and order-key expressions
- result metadata through the public MyLite statement metadata APIs

The feature is intentionally scoped to `CHAR()`. It does not implement general
`CAST`/`CONVERT` charset conversion, broader collation coercibility, or a full
MySQL charset registry.

## Syntax

`CHAR()` uses CHAR-specific grammar because `CHAR` is also a type keyword and
because the optional `USING` clause appears inside the argument list's closing
parenthesis:

```lemon
scalar_function_call(A) ::= CHAR(T) LPAREN(L) function_argument_list(C) RPAREN(R). {
    A = mylite_sql_parser_make_char_function_call(
        state, (struct mylite_sql_parser_char_function_call_parts){
                   .char_token = T,
                   .left_paren = L,
                   .arguments = C,
                   .charset = NULL,
                   .right_paren = R,
               });
}
scalar_function_call(A) ::= CHAR(T) LPAREN(L) function_argument_list(C)
                            USING charset_value(U) RPAREN(R). {
    A = mylite_sql_parser_make_char_function_call(
        state, (struct mylite_sql_parser_char_function_call_parts){
                   .char_token = T,
                   .left_paren = L,
                   .arguments = C,
                   .charset = U,
                   .right_paren = R,
               });
}
```

`CHAR()` with zero arguments is a syntax error in MySQL 8.4.9. `CHAR(USING
utf8mb4)` is also a syntax error because at least one value argument must
precede `USING`.

`charset_value` accepts the existing MyLite charset spelling forms used by
`SET NAMES` and column definitions: identifiers, quoted strings, and `BINARY`.

## Semantics

`CHAR()` evaluates arguments left to right. Each non-`NULL` argument is
converted to an integer code value. `NULL` arguments are skipped; if all
arguments are `NULL`, the result is an empty string, not `NULL`.

Each converted code value contributes one to four bytes:

- value `0` contributes one `0x00` byte
- otherwise the low 32 bits are encoded in big-endian order
- leading zero bytes are stripped from the four-byte representation

Representative verified results:

| SQL expression | Result bytes | Warnings |
| --- | --- | --- |
| `HEX(CHAR(65))` | `41` | none |
| `HEX(CHAR(65,66,67))` | `414243` | none |
| `LENGTH(CHAR(NULL))` | `0` | none |
| `HEX(CHAR(65,NULL,66))` | `4142` | none |
| `HEX(CHAR(0))` | `00` | none |
| `LENGTH(CHAR(0))` | `1` | none |
| `HEX(CHAR(10,26,39,92))` | `0A1A275C` | none |
| `HEX(CHAR(255))` | `FF` | none |
| `HEX(CHAR(256))` | `0100` | none |
| `HEX(CHAR(65535))` | `FFFF` | none |
| `HEX(CHAR(65536))` | `010000` | none |
| `HEX(CHAR(4294967295))` | `FFFFFFFF` | none |
| `HEX(CHAR(4294967296))` | `00` | none |
| `HEX(CHAR(18446744073709551615))` | `FFFFFFFF` | none |
| `HEX(CHAR(9223372036854775807))` | `FFFFFFFF` | none |
| `HEX(CHAR(9223372036854775808))` | `00` | none |
| `HEX(CHAR(-9223372036854775808))` | `00` | none |
| `HEX(CHAR(-1))` | `FFFFFFFF` | none |
| `HEX(CHAR(-4294967295))` | `01` | none |

Exact decimal numeric arguments use MySQL's rounded integer conversion for
function integer arguments. Approximate numeric arguments use the approximate
numeric conversion path observed in MySQL:

| SQL expression | Result bytes | Warnings |
| --- | --- | --- |
| `HEX(CHAR(12.4,12.5,12.6,12.9))` | `0C0D0D0D` | none |
| `HEX(CHAR(12.4E0,12.5E0,12.6E0,12.9E0))` | `0C0C0D0D` | none |
| `HEX(CHAR(1e2,1.5e2,1e20))` | `6496FFFFFFFF` | none |

String arguments are parsed as integer text. Trailing noninteger content,
fractional text, empty strings, whitespace-only strings, nonnumeric strings,
and out-of-range text emit warning 1292 with MySQL-compatible `INTEGER`
wording and then use the parsed or clamped value:

| SQL expression | Result bytes | Warnings |
| --- | --- | --- |
| `HEX(CHAR('65'))` | `41` | none |
| `HEX(CHAR('65x'))` | `41` | 1292 |
| `HEX(CHAR('65.9'))` | `41` | 1292 |
| `HEX(CHAR('x65'))` | `00` | 1292 |
| `HEX(CHAR(''))` | `00` | 1292 |
| `HEX(CHAR(' '))` | `00` | 1292 |
| `HEX(CHAR('18446744073709551615'))` | `FFFFFFFF` | none |
| `HEX(CHAR('18446744073709551616'))` | `FFFFFFFF` | 1292 |
| `HEX(CHAR('-9223372036854775809'))` | `00` | 1292 |
| `HEX(CHAR(-9223372036854775809))` | `00` | 1292 |
| `HEX(CHAR(-18446744073709551615))` | `00` | 1292 |

## Optional `USING` charset

Without `USING`, `CHAR()` returns a binary string. `USING binary` keeps the same
binary result behavior.

This feature supports the following `USING` charsets locally for `CHAR()`:

- `binary`: byte string, no charset validation
- `latin1`: byte-preserving nonbinary string
- `utf8mb4`: byte sequence must be valid UTF-8
- `utf8mb3` and `utf8`: byte sequence must be valid UTF-8 without four-byte
  code points
- `ascii`: byte-preserving result with warning 1300 for bytes above `0x7F`

MySQL also emits deprecation or alias warnings for `utf8mb3` and `utf8`.
MyLite defers those general charset deprecation diagnostics in this slice.

Unknown charset names fail with MySQL error 1115, `Unknown character set:
'name'`.

For invalid `utf8mb4` or `utf8mb3` byte sequences, MySQL 8.4.9 in default
strict SQL mode returns `NULL` and warning 1300. MyLite implements that default
strict behavior. Non-strict SQL mode's empty-string result for invalid UTF-8 is
deferred until MyLite has SQL mode support.

Verified charset examples:

| SQL expression | Result | Warnings |
| --- | --- | --- |
| `CHAR(77,121,83,81,'76' USING utf8mb4)` | `MySQL` | none |
| `HEX(CHAR(65 USING latin1))` | `41` | none |
| `HEX(CHAR(255 USING binary))` | `FF` | none |
| `HEX(CHAR(128 USING ascii))` | `80` | 1300 |
| `CHAR(255 USING utf8mb4) IS NULL` | `1` | 1300 |

## Metadata

MySQL reports `CHAR()` as `VAR_STRING` with decimals `31` and nullable result
metadata. The display length is `4 * argument_count` bytes for binary results,
because each argument can contribute up to four bytes. For nonbinary `USING`
charsets, client metadata uses the current connection charset and multiplies
the same character count by the connection charset's maximum bytes per
character.

Verified metadata:

| Expression | `SET NAMES` | Type | Charset id | Length | Decimals | Flags |
| --- | --- | --- | --- | --- | --- | --- |
| `CHAR(65)` | `utf8mb4` | `VAR_STRING` | `63` | `4` | `31` | `BINARY` |
| `CHAR(65,66)` | `utf8mb4` | `VAR_STRING` | `63` | `8` | `31` | `BINARY` |
| `CHAR(65 USING utf8mb4)` | `utf8mb4` | `VAR_STRING` | `255` | `16` | `31` | none |
| `CHAR(65,66 USING utf8mb4)` | `utf8mb4` | `VAR_STRING` | `255` | `32` | `31` | none |
| `CHAR(65 USING latin1)` | `utf8mb4` | `VAR_STRING` | `255` | `16` | `31` | none |
| `CHAR(65 USING binary)` | `utf8mb4` | `VAR_STRING` | `63` | `4` | `31` | `BINARY` |
| `CHAR(65 USING ascii)` | `utf8mb4` | `VAR_STRING` | `255` | `16` | `31` | none |
| `CHAR(65,66 USING utf8mb4)` | `latin1` | `VAR_STRING` | `8` | `8` | `31` | none |

## Warnings, errors, and DML

Argument conversion warnings are ordinary expression warnings in no-table and
table `SELECT`. In strict DML paths, MyLite already promotes expression
warnings during `UPDATE` and `DELETE` to statement errors and rolls back the
statement. `CHAR()` participates in that existing mechanism for invalid integer
text.

Invalid UTF-8 under `USING utf8mb4` returns `NULL` with warning 1300 in scalar
contexts. Full non-strict SQL mode behavior and exact DML treatment for every
charset warning category remain deferred with broader SQL mode support.

## Runtime and storage impact

`CHAR()` is a pure scalar function. It does not change the `.mylite` file
format, SQLite storage layout, indexes, transactions, or schema catalog.

Runtime evaluation must preserve embedded `NUL` bytes through MyLite's
length-aware text values so `HEX(CHAR(...))`, `LENGTH(CHAR(...))`, table
projection, and DML assignments can observe the full byte string.

## Test coverage

The implementation must add coverage for:

- parser acceptance of `CHAR(expr, ...)` and `CHAR(expr USING charset)`
- parser rejection of `CHAR()` and `CHAR(USING charset)`
- `NULL` skipping and all-`NULL` empty-string behavior
- embedded `NUL` byte generation and byte length
- ASCII/control bytes, quotes, backslashes, and Control-Z byte values
- values above 255, multi-byte code-value output, negative values, unsigned and
  signed 64-bit boundaries, and out-of-range numeric/text inputs
- exact and approximate numeric conversion differences
- supported `USING` charsets and invalid charset diagnostics
- invalid charset byte warnings for UTF-8 and ASCII
- table projection, `WHERE`, `ORDER BY`, `UPDATE`, and `DELETE`
- result metadata under `SET NAMES utf8mb4` and `latin1`

## Compatibility status

After this feature, `CHAR()` is partially supported for the scoped expression
contexts and supported local `USING` charsets. Deferred items are full MySQL
charset conversion, collation/coercibility, non-strict SQL mode variants,
`utf8mb3`/`utf8` deprecation and alias warnings, `max_allowed_packet`, full
native diagnostic fidelity for every expression shape, and generated-column
charset metadata exceptions.
