# `BIT_COUNT()` and `BIT_LENGTH()` scalar functions

## Scope

This feature implements two MySQL scalar utility functions:

- `BIT_COUNT(expr)`
- `BIT_LENGTH(expr)`

They are available anywhere MyLite currently evaluates supported scalar
built-ins: no-table `SELECT`, one-table `SELECT` projection, `WHERE`,
`ORDER BY`, and the existing single-table `UPDATE` and `DELETE` expression
paths.

Out of scope:

- aggregate bit functions such as `BIT_AND()`, `BIT_OR()`, and `BIT_XOR()`
- exact native error-code exposure for unsupported arity beyond the existing
  scalar-function binding diagnostic
- `_binary` introducers and hex/bit literal evaluation in all expression paths
  where the broader parser/runtime has not yet implemented those literal forms
- full binary-string `BIT_COUNT()` dispatch for table columns until expression
  values carry enough type information through runtime evaluation

## Sources

- MySQL 8.4 Reference Manual, Bit Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/bit-functions.html
- MySQL 8.4 Reference Manual, String Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions.html
- Existing MyLite scalar-function design:
  `docs/specs/scalar-built-in-functions/specs.md`
- Existing MyLite expression, cast, and metadata specs:
  - `docs/specs/expression-operator-foundation/specs.md`
  - `docs/specs/cast-expression/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --show-warnings`
- `docker exec -i mylite-mysql-849 mysql -uroot --force --batch --raw --show-warnings`
- `docker exec -i mylite-mysql-849 mysql -uroot --column-type-info -vvv`

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 behavior summary

`BIT_COUNT()` returns the number of one bits in its argument. For numeric
values and ordinary text values, MySQL evaluates the argument as an unsigned
64-bit integer. `NULL` returns `NULL`.

Verified `BIT_COUNT()` results:

| Expression | Result | Warnings |
| --- | --- | --- |
| `BIT_COUNT(0)` | `0` | none |
| `BIT_COUNT(1)` | `1` | none |
| `BIT_COUNT(7)` | `3` | none |
| `BIT_COUNT(-1)` | `64` | none |
| `BIT_COUNT(18446744073709551615)` | `64` | none |
| `BIT_COUNT(NULL)` | `NULL` | none |
| `BIT_COUNT('7')` | `3` | none |
| `BIT_COUNT('x')` | `0` | warning 1292, `Truncated incorrect INTEGER value: 'x'` |
| `BIT_COUNT('  15x')` | `4` | warning 1292 |
| `BIT_COUNT('-1')` | `64` | none |
| `BIT_COUNT('-1x')` | `64` | warning 1292 |
| `BIT_COUNT(1.9)` | `1` | none |
| `BIT_COUNT(-1.9)` | `63` | none |
| `BIT_COUNT(_binary X'3135')` | `7` | none |

The decimal numeric cases use MySQL's integer conversion for exact and
approximate numeric arguments. In particular, `1.9` converts to `2`, and
`-1.9` converts to `-2` before unsigned 64-bit bit counting. Text parsing
accepts leading whitespace and an optional sign, stops at the first
non-integer character, and emits warning 1292 when no digits are found,
trailing garbage remains, or overflow occurs.

`BIT_LENGTH()` returns the byte length of the argument after string conversion,
multiplied by eight. `NULL` returns `NULL`.

Verified `BIT_LENGTH()` results:

| Expression | Result | Warnings |
| --- | --- | --- |
| `BIT_LENGTH('abc')` | `24` | none |
| `BIT_LENGTH('')` | `0` | none |
| `BIT_LENGTH(NULL)` | `NULL` | none |
| `BIT_LENGTH(_binary X'4100')` | `16` | none |
| `BIT_LENGTH('é')` with `SET NAMES utf8mb4` | `16` | none |
| `BIT_LENGTH(123)` | `24` | none |
| `BIT_LENGTH(1.25)` | `32` | none |

Unsupported arity returns MySQL error 1582:

| Statement | MySQL result |
| --- | --- |
| `SELECT BIT_COUNT()` | error 1582, SQLSTATE `42000` |
| `SELECT BIT_COUNT(1,2)` | error 1582, SQLSTATE `42000` |
| `SELECT BIT_LENGTH()` | error 1582, SQLSTATE `42000` |
| `SELECT BIT_LENGTH('a','b')` | error 1582, SQLSTATE `42000` |

MyLite's first slice uses the existing unsupported-function/arity diagnostic
path for wrong arity, as other current scalar functions do.

## Metadata

With both `SET NAMES utf8mb4` and `SET NAMES latin1`, MySQL reports binary
numeric metadata:

| Expression | Type | Length | Decimals | Charset | Flags |
| --- | --- | --- | --- | --- | --- |
| `BIT_COUNT(7)` | `LONGLONG` | `21` | `0` | binary `63` | `NOT_NULL BINARY NUM` |
| `BIT_COUNT(NULL)` | `LONGLONG` | `21` | `0` | binary `63` | `BINARY NUM` |
| `BIT_LENGTH('abc')` | `LONGLONG` | `10` | `0` | binary `63` | `NOT_NULL BINARY NUM` |
| `BIT_LENGTH(NULL)` | `LONGLONG` | `10` | `0` | binary `63` | `BINARY NUM` |

## Parser and AST design

The existing generic scalar-function grammar is sufficient. No dedicated token
or AST node is needed.

MyLite Lemon-syntax intent:

```text
scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.
function_name ::= identifier.
```

The binder recognizes `BIT_COUNT` and `BIT_LENGTH` case-insensitively and
requires exactly one argument for each.

## Runtime design

`BIT_LENGTH()`:

1. Evaluate the single argument.
2. If the argument is `NULL`, return `NULL`.
3. Convert the value to its MySQL-style string representation using existing
   expression conversion helpers.
4. Return `byte_length * 8`, guarding against host integer overflow.

`BIT_COUNT()`:

1. Evaluate the single argument.
2. If the argument is `NULL`, return `NULL`.
3. Convert integer values to their unsigned 64-bit bit pattern.
4. Convert approximate numeric values using the existing MySQL numeric-to-signed
   integer rounding helper, then reinterpret as unsigned 64-bit.
5. Convert text values with MySQL integer parsing: leading whitespace, optional
   sign, decimal digits, optional trailing whitespace, and warning 1292 for
   empty, trailing-garbage, or overflow input. Negative text values become the
   corresponding unsigned 64-bit complement without the separate `CAST AS
   UNSIGNED` complement warning.
6. Return the popcount as a signed `LONGLONG` value in range `0..64`.

## Storage, performance, and compatibility

These functions are deterministic and do not touch storage. Runtime cost is
linear in the input byte length for string conversion/parsing and constant for
the final 64-bit popcount. No new dependencies are needed.

The first slice intentionally follows the current MyLite scalar-expression
value model. `BIT_LENGTH()` is exact for current text and binary text values
because byte length is already carried with expression values. Full
binary-string `BIT_COUNT()` dispatch for table values remains deferred until
runtime expression values preserve binary-vs-character type information.

## Test plan

Parser tests:

- `BIT_COUNT(expr)` and `BIT_LENGTH(expr)` parse as ordinary function calls
- mixed-case names are accepted through the generic identifier path

Runtime tests:

- no-table scalar results for positive integers, zero, `NULL`, negative values,
  unsigned 64-bit maximum, approximate numerics, text numerics, invalid text,
  signed text, UTF-8 text, empty text, numeric `BIT_LENGTH()`, and binary text
  from supported producers
- warning 1292 for invalid/truncated `BIT_COUNT()` text inputs
- metadata under `SET NAMES utf8mb4` and `SET NAMES latin1`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- `UPDATE` assignments, predicates, and order keys
- `DELETE` predicates and order keys
- wrong arity rejected through the existing scalar binding path

## Compatibility status

`BIT_COUNT()` and `BIT_LENGTH()` are implemented for supported scalar
expression call sites with MySQL 8.4.9-verified results, warnings, and
metadata. The status remains partial because native wrong-arity error 1582 and
full binary-string `BIT_COUNT()` type dispatch are deferred.
