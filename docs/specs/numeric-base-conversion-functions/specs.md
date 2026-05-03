# Numeric base conversion scalar functions

## Scope

This feature implements MySQL-compatible `BIN(expr)`, `OCT(expr)`, and
`CONV(expr, from_base, to_base)` as scalar built-in functions.

The functions are available anywhere MyLite currently evaluates supported
scalar built-ins:

- no-table `SELECT`
- table-backed `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments, predicates, and order keys
- supported single-table `DELETE` predicates and order keys

Out of scope:

- exact native error-code exposure for unsupported arity paths
- `max_allowed_packet` result-size enforcement
- generated-column metadata's MySQL table-character-set exception for `CONV()`
- full decimal and floating-point category preservation outside the current
  MyLite scalar expression value model
- hex and bit literal execution until those literal kinds are supported by the
  shared scalar expression evaluator

## Sources

- MySQL 8.4 Reference Manual, Mathematical Functions:
  https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html
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

Runtime probes used `SET NAMES utf8mb4` unless a metadata probe explicitly
switched to `latin1`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## Syntax and arity

`BIN()` and `OCT()` are unary scalar functions. `CONV()` accepts exactly three
arguments. Wrong-arity calls raise native MySQL error 1582. MyLite may continue
to reject those paths through the existing unsupported-function/arity
diagnostic until exact native function diagnostics are implemented globally.

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

## Semantics

### Common result rules

All three functions return a character string in the connection result
character set and collation. `BIN()` returns base 2 digits, `OCT()` returns
base 8 digits, and `CONV()` returns base 2 through base 36 digits. Digits above
9 are uppercase `A` through `Z`.

If any required argument is `NULL`, the result is `NULL`.

The conversion uses 64-bit precision. Overflow while parsing the input number
clamps to the appropriate signed or unsigned limit and emits warning 1292.

### `BIN(expr)`

`BIN(expr)` is equivalent to `CONV(expr, 10, 2)`.

The argument follows the same input text conversion and unsigned base-10
parsing rules as `CONV(expr, 10, 2)`. Decimal fractions effectively truncate
when parsing stops at `.`, while approximate scientific notation such as
`1e20` parses only the leading digit. Negative values are displayed as their
unsigned 64-bit two's-complement representation.

Observed examples:

| Expression | Result | Warnings |
| --- | --- | --- |
| `BIN(NULL)` | `NULL` | none |
| `BIN(0)` | `0` | none |
| `BIN(12)` | `1100` | none |
| `BIN(12.9)` | `1100` | none |
| `BIN(-12.9)` | `1111111111111111111111111111111111111111111111111111111111110100` | none |
| `BIN(1e20)` | `1` | none |
| `BIN(-1)` | 64 one digits | none |
| `BIN('12')` | `1100` | none |
| `BIN('12x')` | `1100` | none |
| `BIN('x12')` | `0` | warning 1292 |
| `BIN('')` | `NULL` | none |
| `BIN(' ')` | `0` | warning 1292 |
| `BIN('18446744073709551616')` | 64 one digits | warning 1292 |

### `OCT(expr)`

`OCT(expr)` is equivalent to `CONV(expr, 10, 8)`.

It follows the same input text conversion, sign, `NULL`, and warning rules as
`BIN()`.

Observed examples:

| Expression | Result | Warnings |
| --- | --- | --- |
| `OCT(NULL)` | `NULL` | none |
| `OCT(0)` | `0` | none |
| `OCT(12)` | `14` | none |
| `OCT(12.9)` | `14` | none |
| `OCT(-1)` | `1777777777777777777777` | none |
| `OCT('12x')` | `14` | none |
| `OCT('x12')` | `0` | warning 1292 |
| `OCT('')` | `NULL` | none |

### `CONV(expr, from_base, to_base)`

`from_base` and `to_base` are converted to signed integers using MySQL's
ordinary integer conversion rules. Exact decimal half values round away from
zero. Approximate half values follow the verified MySQL runtime's half-even
behavior through the current MyLite conversion helper. If either base has an
absolute value outside `2..36`, `CONV()` returns `NULL`.

The absolute value of `from_base` determines the input radix. A negative
`from_base` uses signed input parsing. A positive `from_base` uses unsigned
input parsing. A negative `to_base` formats the parsed 64-bit value as signed;
a positive `to_base` formats it as unsigned.

`expr` is converted to text before base parsing:

- integer values are formatted in base 10
- exact integer and decimal literals preserve their decimal text for overflow
  and truncation behavior
- approximate values use MySQL's floating-point string form before digit
  parsing, so scientific notation such as `1e20` parses the leading digit and
  stops at `e`
- string values are parsed directly
- an empty string input returns `NULL`
- leading ASCII whitespace is ignored
- an optional `+` or `-` sign is accepted
- parsing stops at the first digit that is invalid for `from_base`
- trailing invalid characters after at least one valid digit do not warn
- no valid digits, or overflow, emits warning 1292 and returns zero or the
  clamped 64-bit limit

Observed examples:

| Expression | Result | Warnings |
| --- | --- | --- |
| `CONV(NULL,10,2)` | `NULL` | none |
| `CONV(10,NULL,2)` | `NULL` | none |
| `CONV(10,10,NULL)` | `NULL` | none |
| `CONV('a',16,2)` | `1010` | none |
| `CONV('6E',18,8)` | `172` | none |
| `CONV(10,2,10)` | `2` | none |
| `CONV(10,16,10)` | `16` | none |
| `CONV(-17,10,-18)` | `-H` | none |
| `CONV(-1,10,2)` | 64 one digits | none |
| `CONV(-1,10,-2)` | `-1` | none |
| `CONV('FFFFFFFFFFFFFFFF',16,-10)` | `-1` | none |
| `CONV('8000000000000000',16,-10)` | `-9223372036854775808` | none |
| `CONV('FFFFFFFFFFFFFFFF',-16,10)` | `9223372036854775807` | warning 1292 |
| `CONV('8000000000000000',-16,10)` | `9223372036854775807` | warning 1292 |
| `CONV('-8000000000000000',-16,-10)` | `-9223372036854775808` | none |
| `CONV('z',36,10)` | `35` | none |
| `CONV('1z',10,10)` | `1` | none |
| `CONV('z1',10,10)` | `0` | warning 1292 |
| `CONV('',10,2)` | `NULL` | none |
| `CONV('   ',10,2)` | `0` | warning 1292 |
| `CONV('102',2,10)` | `2` | none |
| `CONV('18446744073709551616',10,16)` | `FFFFFFFFFFFFFFFF` | warning 1292 |
| `CONV('-18446744073709551616',10,16)` | `0` | warning 1292 |
| `CONV(18446744073709551616,10,16)` | `FFFFFFFFFFFFFFFF` | warning 1292 |
| `CONV(-18446744073709551616,10,16)` | `0` | warning 1292 |
| `CONV(1e20,10,16)` | `1` | none |
| `CONV(-1e20,10,16)` | `FFFFFFFFFFFFFFFF` | none |
| `CONV(10,'10x',2)` | `1010` | warning 1292 |
| `CONV(10,'x10',2)` | `NULL` | warning 1292 |
| `CONV(10,10,1)` | `NULL` | none |
| `CONV(12.9,10,16)` | `C` | none |
| `CONV(-12.9,10,-16)` | `-C` | none |

## Errors and warnings

Runtime conversion warnings use MySQL warning code 1292. Observed messages use
`Truncated incorrect DECIMAL value: 'value'` for the first argument and
`Truncated incorrect INTEGER value: 'value'` for base arguments.

Warnings remain warnings for ordinary `SELECT`, including table-backed
projection, predicates, and ordering. In the current strict-mode DML paths,
warnings from `BIN()`, `OCT()`, or `CONV()` evaluation are promoted to
execution errors, matching the existing MyLite warning-promotion policy used by
other scalar functions.

Invalid base range is not diagnostic by itself. It returns `NULL` after any
base-argument conversion warnings have been recorded.

## Result metadata

MySQL reports `VAR_STRING`, MySQL's not-fixed decimals marker `31`, no binary
or numeric flags, and the connection result character set/collation for all
three functions. The declared length is 65 characters in the connection result
character set, so it is 260 bytes under `utf8mb4` and 65 bytes under `latin1`.

Verified metadata:

| Expression | Connection | Type | Collation id | Length | Decimals | Flags |
| --- | --- | --- | ---: | ---: | ---: | --- |
| `BIN(12) AS bin_int` | `utf8mb4` | `VAR_STRING` | `255` | `260` | `31` | none |
| `OCT(12) AS oct_int` | `utf8mb4` | `VAR_STRING` | `255` | `260` | `31` | none |
| `CONV('a',16,2) AS conv_text` | `utf8mb4` | `VAR_STRING` | `255` | `260` | `31` | none |
| `CONV(NULL,10,2) AS conv_null` | `utf8mb4` | `VAR_STRING` | `255` | `260` | `31` | none |
| `BIN(12) AS bin_int` | `latin1` | `VAR_STRING` | `8` | `65` | `31` | none |
| `OCT(12) AS oct_int` | `latin1` | `VAR_STRING` | `8` | `65` | `31` | none |
| `CONV('a',16,2) AS conv_text` | `latin1` | `VAR_STRING` | `8` | `65` | `31` | none |

MyLite's current metadata API also exposes expression nullability through its
existing nullable/`NOT_NULL` model. This slice should keep these descriptors
nullable because `NULL` arguments, empty input strings, and invalid bases can
produce `NULL`.

## Runtime design

Implementation extends the scalar-function registry in
`mylite_expression.c`:

- add function ids for `BIN`, `OCT`, and `CONV`
- validate arity as exactly one, one, and three arguments respectively
- evaluate arguments left to right
- return `NULL` if any argument is `NULL`
- convert and validate bases before parsing `CONV()` input
- preserve exact numeric literal text and convert approximate numeric values to
  floating-point text before radix parsing
- parse optional sign, base digits, overflow, and invalid-start warnings
- render unsigned or signed 64-bit values in bases `2..36`
- reuse the existing warning collection so strict DML can promote conversion
  warnings through the shared statement machinery

Metadata inference in `mylite.c` should add a dedicated base-conversion
descriptor because these functions have a fixed MySQL display length and use
the connection result character set.

No storage or file-format changes are required.

## Tests

Add C tests for:

- parser acceptance of `BIN()`, `OCT()`, and `CONV()`
- unsupported zero-argument and wrong-arity calls
- no-table scalar results for `NULL`, zero, positive integers, negative
  integers, unsigned 64-bit boundaries, signed 64-bit boundaries, exact decimal
  and approximate numeric inputs, string digits `A-Z`/`a-z`, invalid starting
  digits, invalid trailing characters, whitespace, signs, invalid base ranges,
  string base arguments, decimal/real base arguments, overflow, and empty input
  strings
- warning code 1292 and representative warning messages
- metadata under `utf8mb4` and `latin1`
- table-backed `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignment, predicate, and order key
- strict DML warning promotion for invalid first-argument conversion in
  `UPDATE` and `DELETE`

## Compatibility status

After this feature, `BIN()`, `OCT()`, and `CONV()` are partially supported for
the existing scalar expression call sites. The status remains partial because
exact native arity diagnostics, `max_allowed_packet`, generated-column charset
exceptions, hex/bit literal evaluation, and exact numeric category preservation
outside the current scalar expression model remain deferred.
