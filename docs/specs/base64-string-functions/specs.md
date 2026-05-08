# Base64 string functions

## Scope

This feature implements MySQL-compatible `TO_BASE64(expr)` and
`FROM_BASE64(expr)` as scalar string functions.

The functions are available anywhere MyLite currently evaluates supported
scalar built-ins:

- no-table `SELECT`
- table-backed `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments, predicates, and order keys
- supported single-table `DELETE` predicates and order keys

Out of scope:

- `max_allowed_packet` result-size enforcement
- exact native error-code exposure for unsupported arity paths
- hex and bit literal execution until those literal kinds are supported by the
  shared scalar expression evaluator
- complete binary-string behavior for all downstream expression functions
- exact metadata for every generated-column and stored-object context

## Sources

- MySQL 8.4 Reference Manual, String Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions.html
- MySQL 8.4 Reference Manual, Built-In Function and Operator Reference:
  https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html
- MySQL 8.4 Reference Manual, Character Set and Collation of Function Results:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions-charset.html
- Existing MyLite scalar-function design:
  `docs/specs/scalar-built-in-functions/specs.md`
- Existing MyLite `HEX()` / `UNHEX()` function design:
  `docs/specs/string-hex-unhex-functions/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings --force --binary-as-hex=0`
- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv --force --binary-as-hex=0`

Runtime probes used `SET NAMES utf8mb4` unless a metadata probe explicitly
switched to `latin1`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## Syntax and arity

`TO_BASE64()` and `FROM_BASE64()` are unary scalar functions. Zero-argument and
two-argument calls raise native MySQL error 1582. MyLite may continue to reject
those paths through its existing unsupported-function/arity diagnostic until
exact native function diagnostics are implemented globally.

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

### `TO_BASE64(expr)`

If `expr` is `NULL`, `TO_BASE64()` returns `NULL`.

For non-`NULL` input, MySQL first converts the argument to its string form and
then encodes the resulting bytes. Text and binary-string inputs are encoded
byte-for-byte. Numeric inputs encode their display string, so `255` becomes the
bytes for `255`, `12.5` becomes the bytes for `12.5`, and `-1` becomes the
bytes for `-1`.

The encoder uses the common Base64 alphabet with `+` for value 62 and `/` for
value 63. Encoded output is grouped in four-character blocks. Inputs that do
not end on a three-byte boundary are padded with `=`. A newline is inserted
after each 76 encoded characters when more encoded text follows. Empty string
input returns the empty string.

Observed examples:

| Expression | Result |
| --- | --- |
| `TO_BASE64('abc')` | `YWJj` |
| `TO_BASE64('')` | empty string |
| `TO_BASE64(NULL)` | `NULL` |
| `TO_BASE64('猫')` | `54yr` |
| `TO_BASE64(255)` | `MjU1` |
| `TO_BASE64(12.5)` | `MTIuNQ==` |
| `TO_BASE64(12.5E0)` | `MTIuNQ==` |
| `TO_BASE64(-1)` | `LTE=` |
| `LENGTH(TO_BASE64(REPEAT('a',57)))` | `76` |
| `LENGTH(TO_BASE64(REPEAT('a',58)))` | `81` |

### `FROM_BASE64(expr)`

If `expr` is `NULL`, `FROM_BASE64()` returns `NULL`.

For non-`NULL` input, MySQL converts the argument to text, ignores ASCII
whitespace bytes space, tab, line feed, vertical tab, form feed, and carriage
return, then decodes Base64. The result is a binary string. Empty input returns
the empty binary string.

Invalid input returns `NULL` without a warning in the verified MySQL runtime.
Invalid input includes non-Base64 characters, a non-multiple-of-four cleaned
length, missing padding, padding outside the final group, too much padding, or
additional input after padding. MySQL does not require zero pad bits in the
last encoded quantum.

Observed examples:

| Expression | Result | Warnings |
| --- | --- | --- |
| `FROM_BASE64('YWJj')` | `abc` | none |
| `HEX(FROM_BASE64(''))` | empty string | none |
| `FROM_BASE64(NULL)` | `NULL` | none |
| `HEX(FROM_BASE64('54yr'))` | `E78CAB` | none |
| `FROM_BASE64('Y W\nJ\v\f\rj\t')` | `abc` | none |
| `FROM_BASE64('not base64')` | `NULL` | none |
| `FROM_BASE64('YWI')` | `NULL` | none |
| `HEX(FROM_BASE64('AA=='))` | `00` | none |
| `HEX(FROM_BASE64('AB=='))` | `00` | none |
| `HEX(FROM_BASE64('AAA='))` | `0000` | none |
| `HEX(FROM_BASE64('AAB='))` | `0000` | none |
| `FROM_BASE64('AA=A')` | `NULL` | none |
| `FROM_BASE64('AA==AA==')` | `NULL` | none |
| `FROM_BASE64('YWJj====')` | `NULL` | none |
| `HEX(FROM_BASE64(TO_BASE64(CHAR(0,255 USING binary))))` | `00FF` | none |

## Result metadata

`TO_BASE64()` reports `VAR_STRING`, MySQL's not-fixed decimals marker `31`, no
binary flag, and the connection result character set/collation. Its declared
length is the maximum encoded length of the input descriptor after Base64
expansion and 76-character line wrapping, measured in bytes for the connection
result character set.

`FROM_BASE64()` reports `VAR_STRING`, MySQL's not-fixed decimals marker `31`,
binary charset/collation, and the `BINARY` flag. Its declared length is at
most three decoded bytes for every four possible input bytes after the current
descriptor's display length. It is nullable because invalid input can produce
`NULL` even when the argument expression is not statically nullable.

Verified metadata:

| Expression | Connection | Type | Collation id | Length | Decimals | Flags |
| --- | --- | --- | ---: | ---: | ---: | --- |
| `TO_BASE64('abc')` | `utf8mb4` | `VAR_STRING` | `255` | `64` | `31` | none |
| `TO_BASE64('')` | `utf8mb4` | `VAR_STRING` | `255` | `0` | `31` | none |
| `TO_BASE64(NULL)` | `utf8mb4` | `VAR_STRING` | `255` | `0` | `31` | none |
| `TO_BASE64(255)` | `utf8mb4` | `VAR_STRING` | `255` | `32` | `31` | none |
| `FROM_BASE64('YWJj')` | `utf8mb4` | `VAR_STRING` | `63` | `12` | `31` | `BINARY` |
| `FROM_BASE64('')` | `utf8mb4` | `VAR_STRING` | `63` | `0` | `31` | `BINARY` |
| `FROM_BASE64('bad')` | `utf8mb4` | `VAR_STRING` | `63` | `9` | `31` | `BINARY` |
| `FROM_BASE64(NULL)` | `utf8mb4` | `VAR_STRING` | `63` | `0` | `31` | `BINARY` |
| `TO_BASE64('abc')` | `latin1` | `VAR_STRING` | `8` | `4` | `31` | none |
| `TO_BASE64(255)` | `latin1` | `VAR_STRING` | `8` | `8` | `31` | none |
| `FROM_BASE64('YWJj')` | `latin1` | `VAR_STRING` | `63` | `3` | `31` | `BINARY` |
| `FROM_BASE64('bad')` | `latin1` | `VAR_STRING` | `63` | `2` | `31` | `BINARY` |

For a table with `s VARCHAR(20)`, `vb VARBINARY(8)`, `i INT`, and
`d DECIMAL(6,2)`, verified metadata under `utf8mb4` includes:

| Expression | Length |
| --- | ---: |
| `TO_BASE64(s)` | `436` |
| `TO_BASE64(vb)` | `48` |
| `TO_BASE64(i)` | `64` |
| `TO_BASE64(d)` | `48` |
| `FROM_BASE64(s)` | `60` |

MyLite's current metadata API also exposes expression nullability through its
existing nullable/`NOT_NULL` model. This model can be stricter than the MySQL
CLI flags for deterministic non-`NULL` scalar expressions.

## Runtime design

Implementation extends the scalar-function registry in
`mylite_expression.c`:

- add function ids for `TO_BASE64` and `FROM_BASE64`
- validate arity as exactly one argument
- evaluate the argument once, left to right
- return `NULL` for `NULL` input
- convert non-text values with the same scalar string formatting rules used by
  existing conversion helpers
- encode bytes directly for `TO_BASE64()`, including `=` padding and newline
  insertion after each complete 76-character line when output continues
- decode `FROM_BASE64()` by first removing ASCII whitespace bytes space, tab,
  line feed, vertical tab, form feed, and carriage return
- reject malformed cleaned input by returning `NULL` without appending a
  warning
- preserve internal byte lengths for binary `FROM_BASE64()` results so nested
  `HEX(FROM_BASE64(...))`, `LENGTH(FROM_BASE64(...))`, raw C API reads, and
  length-aware downstream scalar functions handle `0x00` bytes

Metadata inference in `mylite.c` adds dedicated `TO_BASE64()` and
`FROM_BASE64()` descriptors. `TO_BASE64()` cannot reuse ordinary string
function metadata because its length derives from Base64 expansion and newline
wrapping. `FROM_BASE64()` cannot reuse ordinary string-function metadata
because its result is binary and invalid input is nullable without warnings.

No storage or file-format changes are required.

## Tests

Add C tests for:

- parser acceptance of generic `TO_BASE64()` and `FROM_BASE64()` calls,
  including case-insensitive names
- unsupported zero-argument and two-argument arity
- no-table scalar results for ASCII, empty string, `NULL`, UTF-8 bytes,
  integer, decimal, approximate numeric, negative numeric, binary bytes,
  long output line wrapping, whitespace-tolerant decoding, invalid inputs,
  missing padding, misplaced padding, nonzero pad bits, and embedded-NUL byte
  lengths
- direct raw-result byte assertions with `mylite_column_bytes()` for decoded
  leading and embedded `0x00` bytes
- no warnings for invalid `FROM_BASE64()` input
- metadata under `utf8mb4` and `latin1`, including text, numeric, `NULL`,
  invalid `FROM_BASE64()`, and binary result descriptors
- table-backed `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignment, predicate, and order key
- supported single-table `DELETE` predicate and order key

## Compatibility status

After this feature, `TO_BASE64()` and `FROM_BASE64()` are partially supported
for the existing scalar expression call sites. The status remains partial
because exact native arity diagnostics, `max_allowed_packet`, fully
length-aware binary string semantics across all expression functions, and exact
metadata in deferred expression contexts remain deferred.
