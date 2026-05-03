# `CRC32()` scalar function

## Scope

This feature implements the MySQL scalar checksum function:

- `CRC32(expr)`

It is available anywhere MyLite currently evaluates supported scalar
built-ins: no-table `SELECT`, one-table `SELECT` projection, `WHERE`,
`ORDER BY`, and the supported single-table `UPDATE` and `DELETE` expression
paths.

Out of scope:

- exact native error-code exposure for unsupported arity beyond the existing
  scalar-function binding diagnostic
- full binary-string type propagation across every expression producer beyond
  the byte-preserving values that the current evaluator already carries
- generated-column and prepared-statement metadata outside the current scalar
  expression descriptor surface

## Sources

- MySQL 8.4 Reference Manual, Built-In Function and Operator Reference:
  https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html
- MySQL 8.4 Reference Manual, Mathematical Functions:
  https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html
- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- Existing MyLite scalar-function design:
  `docs/specs/scalar-built-in-functions/specs.md`
- Existing MyLite related function specs:
  - `docs/specs/bit-utility-functions/specs.md`
  - `docs/specs/base64-string-functions/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings --force --binary-as-hex=0`
- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv --force --binary-as-hex=0`

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## Syntax and arity

`CRC32()` accepts exactly one expression argument. MySQL rejects zero-argument
and multi-argument calls with native error 1582, SQLSTATE `42000`. MyLite's
first slice may continue to reject wrong arity through the existing scalar
function unsupported/arity diagnostic until exact native function diagnostics
are implemented globally.

Generic function-call grammar is sufficient. No dedicated token or AST node is
required.

The intended MyLite Lemon-style grammar remains:

```lemon
scalar_function_call ::= function_name LPAREN opt_function_argument_list RPAREN.
function_name ::= identifier.

opt_function_argument_list ::= .
opt_function_argument_list ::= function_argument_list.

function_argument_list ::= expression.
function_argument_list ::= function_argument_list COMMA expression.
```

The binder recognizes `CRC32` case-insensitively and requires exactly one
argument.

## Semantics

`CRC32(expr)` evaluates its argument once. If the argument is `NULL`, the result
is `NULL`. Otherwise the argument is converted to its MySQL string form, and the
function returns the ISO/ADCCP CRC-32 checksum over those bytes as an unsigned
32-bit integer widened to MySQL's `LONGLONG` result type.

Text and binary-string values are processed by byte length, including embedded
NUL bytes. Numeric values are stringified before checksumming. Exact numeric
literals preserve their literal scale for string conversion, except leading
integer zeros are normalized away and a leading unary plus sign is not part of
the converted text. Decimal literals without an integer part stringify with a
single leading `0`. Approximate numeric values use MySQL's floating-point
string form for the current scalar evaluator slice.

The function does not emit warnings for ordinary successful conversion.

Verified results:

| Expression | Result | Warnings |
| --- | ---: | --- |
| `CRC32('MySQL')` | `3259397556` | none |
| `CRC32('mysql')` | `2501908538` | none |
| `CRC32('')` | `0` | none |
| `CRC32(NULL)` | `NULL` | none |
| `CRC32('é')` with `SET NAMES utf8mb4` | `235179326` | none |
| `CRC32(123)` | `2286445522` | none |
| `CRC32(12.5)` | `327728169` | none |
| `CRC32(12.50)` | `3061501937` | none |
| `CRC32(+12.50)` | `3061501937` | none |
| `CRC32(-12.50)` | `2057867175` | none |
| `CRC32(0012.50)` | `3061501937` | none |
| `CRC32(00012)` | `1330857165` | none |
| `CRC32(.50)` | `1733606989` | none |
| `CRC32(+.50)` | `1733606989` | none |
| `CRC32(0.50)` | `1733606989` | none |
| `CRC32(-.50)` | `3110305273` | none |
| `CRC32(12.5E0)` | `327728169` | none |
| `CRC32(1e20)` | `3966119374` | none |
| `CRC32(-1e20)` | `844230266` | none |
| `CRC32(CHAR(0,255 USING binary))` | `1826356594` | none |
| `CRC32(_binary X'00FF')` | `1826356594` | none |

Wrong-arity behavior:

| Statement | MySQL result |
| --- | --- |
| `SELECT CRC32()` | error 1582, SQLSTATE `42000` |
| `SELECT CRC32('a','b')` | error 1582, SQLSTATE `42000` |

## Metadata

MySQL reports `CRC32()` as an unsigned numeric `LONGLONG` result with binary
charset metadata. The descriptor does not depend on the connection character
set.

Verified metadata under both `SET NAMES utf8mb4` and `SET NAMES latin1`:

| Expression | Type | Length | Decimals | Charset | Flags |
| --- | --- | ---: | ---: | --- | --- |
| `CRC32('abc')` | `LONGLONG` | `10` | `0` | binary `63` | `NOT_NULL UNSIGNED BINARY NUM` |
| `CRC32('')` | `LONGLONG` | `10` | `0` | binary `63` | `NOT_NULL UNSIGNED BINARY NUM` |
| `CRC32(NULL)` | `LONGLONG` | `10` | `0` | binary `63` | `UNSIGNED BINARY NUM` |
| `CRC32(123)` | `LONGLONG` | `10` | `0` | binary `63` | `NOT_NULL UNSIGNED BINARY NUM` |

## Runtime design

Implementation extends the scalar-function registry in
`mylite_expression.c`:

1. Add a function id for `CRC32`.
2. Validate arity as exactly one argument.
3. Evaluate the argument left to right through the shared expression evaluator.
4. Return `NULL` for `NULL`.
5. Convert the argument to its checksum bytes with a helper that preserves text
   byte lengths, exact decimal literal scale after MySQL-style leading-zero
   normalization, and approximate numeric display text consistently with
   current string-encoding function behavior.
6. Compute CRC-32 directly with the reflected polynomial `0xEDB88320`, initial
   state `0xFFFFFFFF`, and final XOR `0xFFFFFFFF`.
7. Return the result as `MYLITE_EXPRESSION_VALUE_UINT64`.

Metadata inference in `mylite.c` should add a dedicated descriptor for `CRC32`
with unsigned `LONGLONG`, length `10`, decimals `0`, binary charset id `63`,
and nullability derived from the argument/value as with existing scalar
functions.

No storage changes are required.

## Storage, performance, and compatibility

`CRC32()` is deterministic and does not touch storage. Runtime cost is linear in
the input byte length. The direct bitwise implementation avoids a dependency
and keeps the binary small. A table-driven implementation can be considered
later only if profiling shows this function is a material hotspot.

The first slice follows the current MyLite scalar-expression value model.
Byte-preserving text values, including `CHAR(... USING binary)` and
`UNHEX()` results, are checksummed exactly. Broader binary-string metadata
propagation remains deferred with the rest of the scalar-function surface.

## Test plan

Parser tests:

- `CRC32(expr)` parses as an ordinary function call.
- mixed-case names are accepted through the generic identifier path.

Runtime tests:

- no-table scalar results for ordinary text, case sensitivity, empty string,
  `NULL`, UTF-8 text, binary bytes through `CHAR(... USING binary)`, integers,
  exact decimals with preserved scale and leading-zero normalization,
  approximate numerics, and negative values
- no warnings for successful scalar evaluation
- metadata under `SET NAMES utf8mb4` and `SET NAMES latin1`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- `UPDATE` assignments, predicates, and order keys
- `DELETE` predicates and order keys
- wrong arity rejected through the existing scalar binding path

## Compatibility status

`CRC32()` is implemented for supported scalar expression call sites with
MySQL 8.4.9-verified results, nullability, metadata, and DML expression
behavior. The status remains partial because exact native wrong-arity error
1582 and full binary-string type propagation are deferred to shared
scalar-function compatibility work.
